#include "focus_router.h"

#include "dialogs/new_task_dialog.h"
#include "pages/completion_page.h"
#include "pages/focus_timer_page.h"
#include "pages/session_setup_page.h"
#include "pages/short_break_page.h"
#include "pages/task_list_page.h"
#include "wui/app.h"

#include <stdexcept>
#include <string>

namespace whatsui::focus_tomato::presentation {

std::string_view focusRouteKey(FocusRoute route) noexcept
{
    switch (route) {
    case FocusRoute::Tasks: return "tasks";
    case FocusRoute::Setup: return "setup";
    case FocusRoute::Timer: return "timer";
    case FocusRoute::Completion: return "completion";
    case FocusRoute::ShortBreak: return "short-break";
    }
    return "tasks";
}

FocusRouter::FocusRouter(wui::UiWindow& window,
                         FocusViewModel& viewModel,
                         const FocusAssets& assets,
                         float pageWidth,
                         float pageHeight)
    : window_(&window)
    , viewModel_(&viewModel)
    , assets_(&assets)
    , pageWidth_(pageWidth)
    , pageHeight_(pageHeight)
{
}

FocusRouter::~FocusRouter()
{
    // UiApp owns UiWindow and may release a closed native window before the
    // application-level router leaves scope. The window tears down its own
    // Navigator, so a router destructor must never dereference this observer.
}

void FocusRouter::start()
{
    if (window_->navigator().empty()) showTasks();
}

void FocusRouter::showTasks()
{
    install(FocusRoute::Tasks);
}

void FocusRouter::showSetup()
{
    if (viewModel_->selectedTask() == nullptr) {
        showTasks();
        return;
    }
    install(FocusRoute::Setup);
}

void FocusRouter::showTimer()
{
    if (viewModel_->activeSession() == nullptr) {
        showTasks();
        return;
    }
    install(FocusRoute::Timer);
}

void FocusRouter::showCompletion()
{
    install(FocusRoute::Completion);
}

void FocusRouter::showShortBreak()
{
    const auto* session = viewModel_->activeSession();
    if (session == nullptr || session->type != SessionType::ShortBreak) {
        showTasks();
        return;
    }
    install(FocusRoute::ShortBreak);
}

void FocusRouter::refresh()
{
    install(currentRoute_);
}

void FocusRouter::updateClock()
{
    if (currentRoute_ != FocusRoute::Timer
        && currentRoute_ != FocusRoute::ShortBreak) {
        return;
    }
    const FocusRoute completingRoute = currentRoute_;
    if (viewModel_->updateClock()) {
        if (completingRoute == FocusRoute::Timer) {
            showCompletion();
        } else {
            showTasks();
        }
    }
}

void FocusRouter::shutdown() noexcept
{
    wui::UiWindow* const window = window_;
    window_ = nullptr;
    if (window != nullptr) window->navigator().clear();
}

FocusRoute FocusRouter::currentRoute() const noexcept
{
    return currentRoute_;
}

std::unique_ptr<wui::Node> FocusRouter::buildCurrentPage()
{
    switch (currentRoute_) {
    case FocusRoute::Tasks:
        return buildTaskListPage(
            *viewModel_, *assets_, pageWidth_, pageHeight_,
            {
                [this](std::string taskId) {
                    viewModel_->selectTask(std::move(taskId));
                    showSetup();
                },
                [this] { requestNewTask(); },
            });
    case FocusRoute::Setup:
        return buildSessionSetupPage(
            *viewModel_, *assets_, pageWidth_, pageHeight_,
            {
                [this] {
                    const auto result = viewModel_->startSelectedFocus();
                    if (result.succeeded()) showTimer();
                },
                [this] { showTasks(); },
            });
    case FocusRoute::Timer:
        return buildFocusTimerPage(
            *viewModel_, *assets_, pageWidth_, pageHeight_,
            {
                [this] {
                    (void)viewModel_->toggleActiveSession();
                    refresh();
                },
                [this] {
                    (void)viewModel_->resetActiveSession();
                    refresh();
                },
                [this] {
                    const auto result = viewModel_->abortActiveSession();
                    if (result.succeeded()) showTasks();
                },
                [] {},
            });
    case FocusRoute::Completion:
        return buildCompletionPage(
            *viewModel_, *assets_, pageWidth_, pageHeight_,
            {
                [this] {
                    const auto result = viewModel_->startShortBreak();
                    if (result.succeeded()) showShortBreak();
                },
                [this] {
                    const auto result = viewModel_->startSelectedFocus();
                    if (result.succeeded()) {
                        showTimer();
                    } else {
                        showTasks();
                    }
                },
            });
    case FocusRoute::ShortBreak:
        return buildShortBreakPage(
            *viewModel_, *assets_, pageWidth_, pageHeight_,
            {
                [this] {
                    (void)viewModel_->toggleActiveSession();
                    refresh();
                },
                [this] {
                    (void)viewModel_->resetActiveSession();
                    refresh();
                },
                [this] {
                    const auto result = viewModel_->skipActiveBreak();
                    if (result.succeeded()) showTasks();
                },
            });
    }
    throw std::logic_error("unknown FocusTomato route");
}

void FocusRouter::install(FocusRoute route)
{
    currentRoute_ = route;
    window_->navigator().clear();
    window_->navigator().setRoot(
        std::string(focusRouteKey(route)),
        [this] { return buildCurrentPage(); },
        wui::PageRetention::DisposeOnHide);
}

void FocusRouter::requestNewTask()
{
    showNewTaskDialog(
        *window_, *viewModel_,
        [this] { refresh(); });
}

} // namespace whatsui::focus_tomato::presentation
