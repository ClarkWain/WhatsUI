#include "focus_router.h"

#include "components/common_components.h"
#include "dialogs/edit_task_dialog.h"
#include "dialogs/new_task_dialog.h"
#include "focus_style.h"
#include "dialogs/confirmation_dialog.h"
#include "pages/completion_page.h"
#include "pages/focus_timer_page.h"
#include "pages/session_setup_page.h"
#include "pages/short_break_page.h"
#include "pages/task_list_page.h"
#include "wui/app.h"
#include "wui/declarative.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace whatsui::focus_tomato::presentation {
namespace {

FocusRoute routeFromKey(std::string_view key) noexcept
{
    for (const auto route : {
             FocusRoute::Tasks,
             FocusRoute::Setup,
             FocusRoute::Timer,
             FocusRoute::Completion,
             FocusRoute::Break,
         }) {
        if (focusRouteKey(route) == key) return route;
    }
    return FocusRoute::Tasks;
}

std::string routeTitle(FocusRoute route)
{
    switch (route) {
    case FocusRoute::Tasks: return "FocusTomato · 任务";
    case FocusRoute::Setup: return "FocusTomato · 开始专注";
    case FocusRoute::Timer: return "FocusTomato · 专注";
    case FocusRoute::Completion: return "FocusTomato · 完成";
    case FocusRoute::Break: return "FocusTomato · 短休息";
    }
    return "FocusTomato";
}

} // namespace

std::string_view focusRouteKey(FocusRoute route) noexcept
{
    switch (route) {
    case FocusRoute::Tasks: return "tasks";
    case FocusRoute::Setup: return "setup";
    case FocusRoute::Timer: return "timer";
    case FocusRoute::Completion: return "completion";
    case FocusRoute::Break: return "break";
    }
    return "tasks";
}

FocusRoute focusInitialRoute(const FocusViewModel& viewModel) noexcept
{
    const auto* session = viewModel.activeSession();
    if (session == nullptr) return FocusRoute::Tasks;
    return session->type == SessionType::Focus
        ? FocusRoute::Timer
        : FocusRoute::Break;
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
    // CallbackLifetime invalidates retained page actions automatically.
}

void FocusRouter::start()
{
    if (!window_->navigator().empty()) return;
    constexpr float captionButtonWidth = 46.0f;
    window_->platformWindow().setFrameRegions({
        {{0.0f, 0.0f, pageWidth_ - captionButtonWidth * 2.0f,
          kFocusWindowBarHeight},
         wui::WindowFrameRegionKind::Caption},
        {{pageWidth_ - captionButtonWidth * 2.0f, 0.0f,
          captionButtonWidth, kFocusWindowBarHeight},
         wui::WindowFrameRegionKind::MinimizeButton},
        {{pageWidth_ - captionButtonWidth, 0.0f,
          captionButtonWidth, kFocusWindowBarHeight},
         wui::WindowFrameRegionKind::CloseButton},
    });
    installRoot(FocusRoute::Tasks);
    const FocusRoute initialRoute = focusInitialRoute(*viewModel_);
    if (initialRoute != FocusRoute::Tasks) push(initialRoute);
}

void FocusRouter::showTasks()
{
    if (window_->navigator().empty()) {
        installRoot(FocusRoute::Tasks);
        return;
    }
    if (currentRoute_ == FocusRoute::Tasks
        && !window_->navigator().canPop()) {
        refresh();
        return;
    }
    window_->navigator().popToRoot();
    currentRoute_ = FocusRoute::Tasks;
}

void FocusRouter::showSetup()
{
    if (viewModel_->activeSession() != nullptr) {
        showActiveSession();
        return;
    }
    if (viewModel_->selectedTask() == nullptr) {
        showTasks();
        return;
    }
    if (currentRoute_ == FocusRoute::Tasks) {
        push(FocusRoute::Setup);
    } else {
        replace(FocusRoute::Setup);
    }
}

void FocusRouter::showTimer()
{
    const auto* session = viewModel_->activeSession();
    if (session == nullptr || session->type != SessionType::Focus) {
        showTasks();
        return;
    }
    if (currentRoute_ == FocusRoute::Timer) {
        refresh();
    } else if (currentRoute_ == FocusRoute::Tasks) {
        push(FocusRoute::Timer);
    } else {
        replace(FocusRoute::Timer);
    }
}

void FocusRouter::showCompletion()
{
    if (currentRoute_ == FocusRoute::Tasks) {
        push(FocusRoute::Completion);
    } else if (currentRoute_ == FocusRoute::Completion) {
        refresh();
    } else {
        replace(FocusRoute::Completion);
    }
}

void FocusRouter::showBreak()
{
    const auto* session = viewModel_->activeSession();
    if (session == nullptr || session->type == SessionType::Focus) {
        showTasks();
        return;
    }
    if (currentRoute_ == FocusRoute::Break) {
        refresh();
    } else if (currentRoute_ == FocusRoute::Tasks) {
        push(FocusRoute::Break);
    } else {
        replace(FocusRoute::Break);
    }
}

void FocusRouter::showActiveSession()
{
    const auto* session = viewModel_->activeSession();
    if (session == nullptr) {
        showTasks();
        return;
    }
    if (session->type == SessionType::Focus) {
        showTimer();
    } else {
        showBreak();
    }
}

void FocusRouter::goBack()
{
    if (window_ == nullptr || !window_->navigator().canPop()) {
        showTasks();
        return;
    }
    (void)window_->navigator().pop();
    const auto* key = window_->navigator().currentKey();
    currentRoute_ = key == nullptr
        ? FocusRoute::Tasks
        : routeFromKey(*key);
}

void FocusRouter::refresh()
{
    if (window_ == nullptr || window_->navigator().empty()) return;
    const FocusRoute route = currentRoute_;
    window_->navigator().replace(
        std::string(focusRouteKey(route)),
        [this, route] { return buildPage(route); },
        wui::PageRetention::DisposeOnHide);
}

void FocusRouter::updateClock()
{
    const auto* session = viewModel_->activeSession();
    if (session == nullptr) return;
    const SessionType completingType = session->type;
    if (viewModel_->updateClock()) {
        if (completingType == SessionType::Focus) {
            showCompletion();
        } else {
            showTasks();
        }
    }
}

void FocusRouter::shutdown() noexcept
{
    lifetime_.invalidate();
    wui::UiWindow* const window = window_;
    window_ = nullptr;
    if (window != nullptr) window->navigator().clear();
}

FocusRoute FocusRouter::currentRoute() const noexcept
{
    return currentRoute_;
}

wui::View FocusRouter::buildPage(FocusRoute route)
{
    using namespace wui;
    auto* const platformWindow = &window_->platformWindow();
    return Box()
        .background(style::canvas)
        .width(pageWidth_)
        .height(pageHeight_)
        .onKey([this, route](const KeyEvent& event) {
            return handlePageShortcut(route, event);
        })
        .children(
            Column()
                .align(Alignment::Start)
                .children(
                    buildWindowBar(
                        pageWidth_, routeTitle(route), *assets_,
                        {
                            [platformWindow] { platformWindow->minimize(); },
                            [platformWindow] {
                                platformWindow->toggleMaximized();
                            },
                            [platformWindow] {
                                platformWindow->requestClose();
                            },
                        },
                        false),
                    buildRouteContent(route)
                )
        );
}

bool FocusRouter::handlePageShortcut(
    FocusRoute route, const wui::KeyEvent& event)
{
    if (event.action != wui::KeyAction::Down) return false;
    const bool requestsBack = event.keyCode == 27
        || (event.keyCode == 263
            && (event.modifiers & wui::KeyModifierAlt) != 0);
    if (requestsBack && route != FocusRoute::Tasks) {
        goBack();
        return true;
    }
    if (event.keyCode != 32) return false;
    switch (route) {
    case FocusRoute::Setup: {
        const auto result = viewModel_->startSelectedFocus();
        if (result.succeeded()) showTimer();
        return true;
    }
    case FocusRoute::Timer:
    case FocusRoute::Break:
        (void)viewModel_->toggleActiveSession();
        refresh();
        return true;
    case FocusRoute::Tasks:
    case FocusRoute::Completion:
        return false;
    }
    return false;
}

wui::View FocusRouter::buildRouteContent(FocusRoute route)
{
    const float contentHeight =
        std::max(0.0f, pageHeight_ - kFocusWindowBarHeight);
    switch (route) {
    case FocusRoute::Tasks:
        return TaskListPage(
            *viewModel_, *assets_, pageWidth_, contentHeight,
            {
                lifetime_.guard([this] { showActiveSession(); }),
                lifetime_.guard([this](std::string taskId) {
                    if (viewModel_->activeSession() != nullptr) {
                        showActiveSession();
                        return;
                    }
                    viewModel_->selectTask(std::move(taskId));
                    showSetup();
                }),
                lifetime_.guard([this](std::string taskId) {
                    (void)viewModel_->toggleTaskCompletion(taskId);
                    refresh();
                }),
                lifetime_.guard([this](std::string taskId) {
                    requestEditTask(std::move(taskId));
                }),
                lifetime_.guard(
                    [this](std::string taskId, std::int64_t revision) {
                        (void)viewModel_->restoreTask(taskId, revision);
                        refresh();
                    }),
                lifetime_.guard([this](TaskFilter filter) {
                    viewModel_->setTaskFilter(filter);
                    refresh();
                }),
                lifetime_.guard([this] { requestNewTask(); }),
                lifetime_.guard([this] {
                    if (viewModel_->activeSession() != nullptr) {
                        showActiveSession();
                        return;
                    }
                    const auto result = viewModel_->startFreeFocus();
                    if (result.succeeded()) showTimer();
                }),
            });
    case FocusRoute::Setup:
        return SessionSetupPage(
            *viewModel_, *assets_, pageWidth_, contentHeight,
            {
                lifetime_.guard([this] {
                    const auto result = viewModel_->startSelectedFocus();
                    if (result.succeeded()) showTimer();
                }),
                lifetime_.guard([this] { goBack(); }),
                lifetime_.guard([this] {
                    const auto* task = viewModel_->selectedTask();
                    if (task != nullptr) requestEditTask(task->id);
                }),
            });
    case FocusRoute::Timer:
        return FocusTimerPage(
            *viewModel_, *assets_, pageWidth_, contentHeight,
            {
                lifetime_.guard([this] { goBack(); }),
                lifetime_.guard([this] {
                    (void)viewModel_->toggleActiveSession();
                    refresh();
                }),
                lifetime_.guard([this] {
                    requestResetConfirmation();
                }),
                lifetime_.guard([this] {
                    requestAbortConfirmation();
                }),
            });
    case FocusRoute::Completion:
        return CompletionPage(
            *viewModel_, *assets_, pageWidth_, contentHeight,
            {
                lifetime_.guard([this] { goBack(); }),
                lifetime_.guard([this] {
                    const auto result = viewModel_->startBreak();
                    if (result.succeeded()) showBreak();
                }),
                lifetime_.guard([this] {
                    const auto result = viewModel_->continueLastFocus();
                    if (result.succeeded()) {
                        showTimer();
                    } else {
                        showTasks();
                    }
                }),
            });
    case FocusRoute::Break:
        return BreakTimerPage(
            *viewModel_, *assets_, pageWidth_, contentHeight,
            {
                lifetime_.guard([this] { goBack(); }),
                lifetime_.guard([this] {
                    (void)viewModel_->toggleActiveSession();
                    refresh();
                }),
                lifetime_.guard([this] {
                    requestResetConfirmation();
                }),
                lifetime_.guard([this] {
                    requestSkipBreakConfirmation();
                }),
            });
    }
    throw std::logic_error("unknown FocusTomato route");
}

void FocusRouter::installRoot(FocusRoute route)
{
    currentRoute_ = route;
    window_->navigator().setRoot(
        std::string(focusRouteKey(route)),
        [this, route] { return buildPage(route); },
        wui::PageRetention::DisposeOnHide);
}

void FocusRouter::push(FocusRoute route)
{
    currentRoute_ = route;
    window_->navigator().push(
        std::string(focusRouteKey(route)),
        [this, route] { return buildPage(route); },
        wui::PageRetention::DisposeOnHide);
}

void FocusRouter::replace(FocusRoute route)
{
    currentRoute_ = route;
    window_->navigator().replace(
        std::string(focusRouteKey(route)),
        [this, route] { return buildPage(route); },
        wui::PageRetention::DisposeOnHide);
}

void FocusRouter::requestNewTask()
{
    showNewTaskDialog(
        *window_, *viewModel_,
        lifetime_.guard([this] { refresh(); }));
}

void FocusRouter::requestEditTask(std::string taskId)
{
    (void)showEditTaskDialog(
        *window_,
        *viewModel_,
        taskId,
        lifetime_.guard([this] { refresh(); }),
        lifetime_.guard(
            [this](std::string deletingTaskId,
                   std::string taskTitle,
                   std::int64_t expectedRevision) {
                requestDeleteTask(
                    std::move(deletingTaskId),
                    std::move(taskTitle),
                    expectedRevision);
            }));
}

void FocusRouter::requestDeleteTask(
    std::string taskId,
    std::string taskTitle,
    std::int64_t expectedRevision)
{
    showConfirmationDialog(
        *window_,
        {
            "删除“" + taskTitle + "”？",
            "任务会从普通列表移到“已删除”。历史专注和统计会保留，也可以稍后恢复。",
            "删除任务",
        },
        lifetime_.guard(
            [this,
             taskId = std::move(taskId),
             expectedRevision] {
                (void)viewModel_->deleteTask(taskId, expectedRevision);
                refresh();
            }));
}

void FocusRouter::requestResetConfirmation()
{
    const auto* session = viewModel_->activeSession();
    if (session == nullptr) return;
    const bool isBreak = session->type != SessionType::Focus;
    showConfirmationDialog(
        *window_,
        {
            isBreak ? "重置休息计时？" : "重置本轮专注？",
            isBreak
                ? "已经过的休息时间会清零，会话仍保持当前暂停或运行状态。"
                : "已专注进度会清零，本轮不会因此计为完成。",
            "确认重置",
        },
        lifetime_.guard([this] {
            (void)viewModel_->resetActiveSession();
            refresh();
        }));
}

void FocusRouter::requestAbortConfirmation()
{
    showConfirmationDialog(
        *window_,
        {
            "提前结束本轮专注？",
            "本次不会计入完成番茄和专注统计，已保存的历史不受影响。",
            "结束本轮",
        },
        lifetime_.guard([this] {
            const auto result = viewModel_->abortActiveSession();
            if (result.succeeded()) showTasks();
        }));
}

void FocusRouter::requestSkipBreakConfirmation()
{
    showConfirmationDialog(
        *window_,
        {
            "提前结束休息？",
            "休息会记录为已跳过，不会增加番茄或专注时长。",
            "结束休息",
        },
        lifetime_.guard([this] {
            const auto result = viewModel_->skipActiveBreak();
            if (result.succeeded()) showTasks();
        }));
}

} // namespace whatsui::focus_tomato::presentation
