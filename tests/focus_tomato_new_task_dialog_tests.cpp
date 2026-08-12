#include "application/focus_repository.h"
#include "capture/headless_platform.h"
#include "presentation/components/task_execution_preferences.h"
#include "presentation/dialogs/new_task_dialog.h"
#include "presentation/focus_router.h"
#include "presentation/focus_style.h"
#include "presentation/focus_view_model.h"

#include "wui/app.h"
#include "wui/declarative.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using namespace whatsui::focus_tomato;
using namespace whatsui::focus_tomato::presentation;

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

bool sameColor(wui::Color left, wui::Color right) noexcept
{
    return left.r == right.r && left.g == right.g
        && left.b == right.b && left.a == right.a;
}

class RecordingRepository final : public FocusRepository {
public:
    RepositoryWriteResult save(const FocusData& data) override
    {
        ++saveCalls;
        if (failNextSave) {
            failNextSave = false;
            return {
                RepositoryWriteStatus::IoError,
                {},
                "forced persistence failure",
            };
        }
        const auto validation = validateFocusData(data);
        if (!validation.ok()) {
            return {
                RepositoryWriteStatus::ValidationRejected,
                validation,
                validation.summary(),
            };
        }
        persisted = data;
        return {};
    }

    FocusData persisted;
    int saveCalls{0};
    bool failNextSave{false};
};

wui::Node* findByAutomationId(
    wui::Node* node, const std::string& automationId)
{
    if (node == nullptr) return nullptr;
    if (node->automationId() == automationId) return node;
    for (const auto& child : node->children()) {
        if (auto* match = findByAutomationId(
                child.get(), automationId)) {
            return match;
        }
    }
    return nullptr;
}

wui::RadioNode* findRadioByValue(
    wui::Node* node, const std::string& value)
{
    if (node == nullptr) return nullptr;
    if (auto* radio = dynamic_cast<wui::RadioNode*>(node);
        radio != nullptr && radio->value() == value) {
        return radio;
    }
    for (const auto& child : node->children()) {
        if (auto* match = findRadioByValue(child.get(), value)) return match;
    }
    return nullptr;
}

wui::PointerEvent pointer(
    wui::WindowId windowId,
    wui::PointerAction action,
    wui::PointF position)
{
    return {
        windowId,
        wui::PointerType::Mouse,
        action,
        wui::MouseButton::Left,
        position,
        0,
    };
}

void focusThemeOwnsEveryBrandInteractionToken()
{
    const wui::Theme themed = style::focusTheme();
    expect(sameColor(
               themed.colors.brandBackground.rest, style::actionPrimary)
               && sameColor(
                   themed.colors.brandBackground.hover,
                   style::actionPrimaryHover)
               && sameColor(
                   themed.colors.brandBackground.pressed,
                   style::actionPrimaryPressed),
           "Primary buttons must use the complete tomato interaction ramp");
    expect(sameColor(
               themed.colors.compoundBrandStroke.rest, style::accent)
               && sameColor(
                   themed.colors.brandForeground1, style::actionPrimary)
               && sameColor(themed.colors.accent, style::actionPrimary),
           "Inputs, selection and compatibility aliases must share the tomato brand");
    expect(sameColor(
               themed.colors.neutralBackground1.rest, style::surface)
               && sameColor(themed.colors.neutralStroke1, style::border),
           "Standard controls must inherit the warm FocusTomato neutral palette");
    expect(themed.colors.dangerBackground.rest.r
                   > themed.colors.dangerBackground.rest.b
               && themed.colors.brandBackground.rest.r
                   > themed.colors.brandBackground.rest.b,
           "Primary and danger actions must stay in one warm color family");
    wui::setTheme(themed);
}

void taskExecutionPreferenceDraftsAreExplicitAndValidated()
{
    std::string error;
    TaskExecutionDraft inherited;
    const auto inheritedValue = parseTaskExecutionPreferences(
        inherited, error);
    expect(inheritedValue
               && !inheritedValue->focusMinutes
               && inheritedValue->sound == TaskSoundPreference::Inherit,
           "An untouched task should explicitly inherit global execution settings");

    inherited.focusMinutes.set("181");
    expect(!parseTaskExecutionPreferences(inherited, error)
               && !error.empty(),
           "Out-of-range task duration must fail before reaching persistence");

    inherited.focusMinutes.set("45");
    inherited.soundChoice.set("off");
    const auto custom = parseTaskExecutionPreferences(inherited, error);
    expect(custom
               && custom->focusMinutes == std::optional<int>{45}
               && custom->sound == TaskSoundPreference::Off
               && custom->soundscapeId.empty(),
           "Custom duration and explicit silence must remain distinct from inheritance");
}

void successfulSubmitRefreshesOnlyAfterDialogDismissal()
{
    RecordingRepository repository;
    FocusDataService service(repository);
    std::int64_t now = 10'000;
    int nextId = 0;
    FocusViewModel viewModel(
        service,
        [&now] { return now; },
        [&nextId] { return "dialog-task-" + std::to_string(++nextId); });

    wui::UiApp app(
        whatsui::gallery::capture::createHeadlessPlatformHost(1.0f));
    auto& window = app.openWindow("new task regression", {520.0f, 720.0f});

    bool refreshed = false;
    bool refreshSawDialog = false;
    auto newTask = wui::Button("新建任务")
        .onClick([&] {
            showNewTaskDialog(window, viewModel, [&] {
                refreshed = true;
                refreshSawDialog = window.hasDialog();
                window.setRoot(wui::Text("已刷新任务列表"));
            });
        });
    auto* newTaskRaw = newTask.node();
    window.setRoot(std::move(newTask));
    window.layout();

    const auto& bounds = newTaskRaw->bounds();
    const wui::PointF center{
        bounds.x + bounds.width * 0.5f,
        bounds.y + bounds.height * 0.5f,
    };
    expect(
        window.dispatchPointer(
            pointer(window.id(), wui::PointerAction::Down, center))
            && window.dispatchPointer(
                pointer(window.id(), wui::PointerAction::Up, center)),
        "The real new-task button interaction should open the dialog");
    expect(window.hasDialog(), "New-task interaction should present a modal");

    expect(
        window.dispatchTextInput({window.id(), "完成崩溃回归测试"}),
        "The focused dialog input should accept text");
    auto* estimateInput = findByAutomationId(
        window.overlayHost().top()->content.get(),
        "focus.new-task.estimate");
    expect(estimateInput != nullptr,
           "The dialog should expose an estimated-pomodoro input");
    window.focusManager().setFocused(estimateInput);
    expect(window.dispatchTextInput({window.id(), "3"}),
           "The estimate editor should accept a valid custom value");
    window.layout();
    auto* saveButton = findByAutomationId(
        window.overlayHost().top()->content.get(), "focus.new-task.save");
    expect(saveButton != nullptr,
           "The dialog should expose a stable Save automation target");
    const auto& saveBounds = saveButton->bounds();
    const wui::PointF saveCenter{
        saveBounds.x + saveBounds.width * 0.5f,
        saveBounds.y + saveBounds.height * 0.5f,
    };
    expect(
        window.dispatchPointer(
            pointer(window.id(), wui::PointerAction::Down, saveCenter))
            && window.dispatchPointer(
                pointer(window.id(), wui::PointerAction::Up, saveCenter)),
        "The real Save button should submit the new-task dialog");

    expect(!window.hasDialog(),
           "The modal should be gone when input dispatch returns");
    expect(!refreshed,
           "Page replacement must wait for the next safe frame boundary");

    window.update();
    expect(refreshed && !refreshSawDialog,
           "The task page should refresh only after modal teardown");
    expect(service.data().tasks.size() == 1
               && service.data().tasks.front().title
                    == "完成崩溃回归测试"
               && service.data().tasks.front().estimatedPomodoros == 3
               && repository.persisted == service.data(),
           "A successful dialog submit should persist exactly one valid task");
}

void rejectedSubmitsKeepTheDialogAndNeverRefreshThePage()
{
    {
        RecordingRepository repository;
        FocusDataService service(repository);
        std::int64_t now = 20'000;
        int nextId = 0;
        FocusViewModel viewModel(
            service,
            [&now] { return now; },
            [&nextId] { return "invalid-" + std::to_string(++nextId); });
        wui::UiApp app(
            whatsui::gallery::capture::createHeadlessPlatformHost(1.0f));
        auto& window = app.openWindow("invalid task", {520.0f, 720.0f});
        window.setRoot(wui::Text("任务列表"));
        bool refreshed = false;
        showNewTaskDialog(
            window, viewModel, [&refreshed] { refreshed = true; });

        expect(window.dispatchTextInput({window.id(), "   \t"}),
               "Whitespace should reach the focused editor before validation");
        expect(window.dispatchKey(
                   {window.id(), wui::KeyAction::Down, 13, 0, false}),
               "Enter should invoke validation for an invalid title");
        window.update();
        expect(window.hasDialog() && !refreshed
                   && service.data().tasks.empty()
                   && repository.saveCalls == 0,
               "Domain-invalid input must retain the modal and skip persistence");
        (void)window.dismissTopDialog();
    }

    {
        RecordingRepository repository;
        repository.failNextSave = true;
        FocusDataService service(repository);
        std::int64_t now = 30'000;
        int nextId = 0;
        FocusViewModel viewModel(
            service,
            [&now] { return now; },
            [&nextId] { return "io-failure-" + std::to_string(++nextId); });
        wui::UiApp app(
            whatsui::gallery::capture::createHeadlessPlatformHost(1.0f));
        auto& window = app.openWindow("failed save", {520.0f, 720.0f});
        window.setRoot(wui::Text("任务列表"));
        bool refreshed = false;
        showNewTaskDialog(
            window, viewModel, [&refreshed] { refreshed = true; });

        expect(window.dispatchTextInput({window.id(), "无法保存的任务"}),
               "Valid input should reach the editor");
        expect(window.dispatchKey(
                   {window.id(), wui::KeyAction::Down, 13, 0, false}),
               "Enter should attempt persistence");
        window.update();
        expect(window.hasDialog() && !refreshed
                   && service.data().tasks.empty()
                   && repository.saveCalls == 1,
               "Persistence failure must keep the draft modal without publishing data");
        (void)window.dismissTopDialog();
    }
}

void routerCanOutliveAClosedUiWindow()
{
    RecordingRepository repository;
    FocusDataService service(repository);
    std::int64_t now = 40'000;
    FocusViewModel viewModel(
        service,
        [&now] { return now; },
        [] { return std::string("router-lifetime"); });
    FocusAssets assets;

    auto app = std::make_unique<wui::UiApp>(
        whatsui::gallery::capture::createHeadlessPlatformHost(1.0f));
    auto& window = app->openWindow(
        "router lifetime", {520.0f, 720.0f});
    auto router = std::make_unique<FocusRouter>(
        window, viewModel, assets, 520.0f, 720.0f);
    router->start();

    app.reset();
    router.reset();
    expect(true,
           "Destroying an application-owned window before its router must be safe");
}

void renderedPageCallbacksExpireWithTheirRouter()
{
    RecordingRepository repository;
    FocusDataService service(repository);
    std::int64_t now = 50'000;
    FocusViewModel viewModel(
        service,
        [&now] { return now; },
        [] { return std::string("expired-router"); });
    FocusAssets assets;

    wui::UiApp app(
        whatsui::gallery::capture::createHeadlessPlatformHost(1.0f));
    auto& window = app.openWindow(
        "router callback lifetime", {520.0f, 720.0f});
    auto router = std::make_unique<FocusRouter>(
        window, viewModel, assets, 520.0f, 720.0f);
    router->start();
    window.layout();

    auto* createTask = findByAutomationId(
        window.uiRoot().content(), "focus.tasks.create");
    expect(createTask != nullptr,
           "The task page should expose its primary create action");
    const auto bounds = createTask->bounds();
    const wui::PointF center{
        bounds.x + bounds.width * 0.5f,
        bounds.y + bounds.height * 0.5f,
    };

    router.reset();
    expect(
        window.dispatchPointer(
            pointer(window.id(), wui::PointerAction::Down, center))
            && window.dispatchPointer(
                pointer(window.id(), wui::PointerAction::Up, center)),
        "The retained control should still complete input routing");
    expect(!window.hasDialog(),
           "A page action must become inert after its Router is destroyed");
}

void taskFilterAndCompletionControlsDrivePersistedUiState()
{
    RecordingRepository repository;
    FocusData data;
    data.tasks = {
        {"active", "进行中", TaskStatus::Active, 1, 0,
         1024, 1, 1'000, 1'000},
        {"done", "已完成", TaskStatus::Done, 1, 0,
         2048, 1, 1'000, 1'000},
    };
    FocusDataService service(repository, data);
    std::int64_t now = 2'000;
    FocusViewModel viewModel(
        service, [&now] { return now; }, [] { return "unused"; });
    FocusAssets assets;
    wui::UiApp app(
        whatsui::gallery::capture::createHeadlessPlatformHost(1.0f));
    auto& window = app.openWindow("task controls", {640.0f, 820.0f});
    FocusRouter router(window, viewModel, assets, 640.0f, 820.0f);
    router.start();
    window.layout();

    auto click = [&window](wui::Node* node, const char* message) {
        expect(node != nullptr, message);
        const auto bounds = node->bounds();
        const wui::PointF center{
            bounds.x + bounds.width * 0.5f,
            bounds.y + bounds.height * 0.5f,
        };
        expect(
            window.dispatchPointer(
                pointer(window.id(), wui::PointerAction::Down, center))
                && window.dispatchPointer(
                    pointer(window.id(), wui::PointerAction::Up, center)),
            message);
        window.layout();
    };

    click(
        findByAutomationId(
            window.uiRoot().content(), "focus.tasks.filter.completed"),
        "The completed filter should be a real interaction target");
    expect(findByAutomationId(
               window.uiRoot().content(), "focus.tasks.row.active") == nullptr
               && findByAutomationId(
                   window.uiRoot().content(), "focus.tasks.row.done") != nullptr,
           "Filtering should rebuild the page with only matching task rows");

    click(
        findByAutomationId(
            window.uiRoot().content(), "focus.tasks.toggle.done"),
        "A completed task should expose a restore control");
    expect(service.data().tasks[1].status == TaskStatus::Active
               && repository.persisted == service.data(),
           "Restoring from the row must persist through the validation gateway");
    expect(findByAutomationId(
               window.uiRoot().content(), "focus.tasks.row.done") == nullptr,
           "After restore, the completed filter should show a filtered empty state");
}

void destructiveTimerActionsRequireExplicitConfirmation()
{
    RecordingRepository repository;
    FocusDataService service(repository);
    std::int64_t now = 10'000;
    int id = 0;
    FocusViewModel viewModel(
        service,
        [&now] { return now; },
        [&id] { return "confirm-" + std::to_string(++id); });
    expect(viewModel.addTask("确认保护", 1).succeeded(),
           "Confirmation test task should be created");
    viewModel.selectTask(service.data().tasks.front().id);
    expect(viewModel.startSelectedFocus().succeeded(),
           "Confirmation test focus should start");

    FocusAssets assets;
    wui::UiApp app(
        whatsui::gallery::capture::createHeadlessPlatformHost(1.0f));
    auto& window = app.openWindow("timer confirmation", {480.0f, 720.0f});
    FocusRouter router(window, viewModel, assets, 480.0f, 720.0f);
    router.start();
    window.layout();

    auto click = [&window](wui::Node* node, const char* message) {
        expect(node != nullptr, message);
        const auto bounds = node->bounds();
        const wui::PointF center{
            bounds.x + bounds.width * 0.5f,
            bounds.y + bounds.height * 0.5f,
        };
        expect(
            window.dispatchPointer(
                pointer(window.id(), wui::PointerAction::Down, center))
                && window.dispatchPointer(
                    pointer(window.id(), wui::PointerAction::Up, center)),
            message);
        window.layout();
    };

    click(
        findByAutomationId(
            window.uiRoot().content(), "focus.timer.abort"),
        "The timer should expose an abort control");
    expect(window.hasDialog()
               && viewModel.activeSession()->status == SessionStatus::Running,
           "Requesting abort must not mutate the session before confirmation");
    click(
        findByAutomationId(
            window.overlayHost().top()->content.get(),
            "focus.confirm.cancel"),
        "The safe cancel action should be keyboard-focusable and clickable");
    expect(!window.hasDialog()
               && viewModel.activeSession()->status == SessionStatus::Running,
           "Canceling must preserve the running focus exactly");

    click(
        findByAutomationId(
            window.uiRoot().content(), "focus.timer.abort"),
        "Abort should reopen its confirmation dialog");
    click(
        findByAutomationId(
            window.overlayHost().top()->content.get(),
            "focus.confirm.accept"),
        "The explicit destructive confirmation should be actionable");
    window.update();
    expect(!service.data().activeSessionId
               && service.data().sessions.back().status
                    == SessionStatus::Aborted
               && router.currentRoute() == FocusRoute::Tasks,
           "Confirmed abort must persist one aborted terminal fact and return to tasks");
}

void reversibleNavigationNeverMutatesTheActiveSession()
{
    RecordingRepository repository;
    FocusData data;
    data.tasks.push_back({
        "reversible-task",
        "检查下一步安排",
        TaskStatus::Active,
        2,
        0,
        1024,
        1,
        1'000,
        1'000,
    });
    FocusDataService service(repository, data);
    std::int64_t now = 10'000;
    int id = 0;
    FocusViewModel viewModel(
        service,
        [&now] { return now; },
        [&id] { return "navigation-" + std::to_string(++id); });
    FocusAssets assets;
    wui::UiApp app(
        whatsui::gallery::capture::createHeadlessPlatformHost(1.0f));
    auto& window = app.openWindow("reversible navigation", {520.0f, 720.0f});
    FocusRouter router(window, viewModel, assets, 520.0f, 720.0f);
    router.start();
    window.layout();

    auto click = [&window](wui::Node* node, const char* message) {
        expect(node != nullptr, message);
        const auto bounds = node->bounds();
        const wui::PointF center{
            bounds.x + bounds.width * 0.5f,
            bounds.y + bounds.height * 0.5f,
        };
        expect(
            window.dispatchPointer(
                pointer(window.id(), wui::PointerAction::Down, center))
                && window.dispatchPointer(
                    pointer(window.id(), wui::PointerAction::Up, center)),
            message);
        window.layout();
    };

    viewModel.selectTask("reversible-task");
    router.showSetup();
    window.layout();
    expect(router.currentRoute() == FocusRoute::Setup
               && window.navigator().canPop(),
           "Setup should be a reversible child route of the task list");
    click(
        findByAutomationId(
            window.uiRoot().content(), "focus.setup.back"),
        "Setup should expose a visible Back to tasks action");
    expect(router.currentRoute() == FocusRoute::Tasks
               && repository.saveCalls == 0,
           "Leaving setup should only pop navigation and never write data");

    expect(viewModel.startSelectedFocus().succeeded(),
           "The selected task should start a focus session");
    router.showTimer();
    window.layout();
    const std::string focusId = viewModel.activeSession()->id;
    const int writesAfterStart = repository.saveCalls;
    click(
        findByAutomationId(
            window.uiRoot().content(), "focus.timer.minimize"),
        "The focus timer should expose a non-destructive minimize action");
    expect(router.currentRoute() == FocusRoute::Tasks
               && viewModel.activeSession() != nullptr
               && viewModel.activeSession()->id == focusId
               && viewModel.activeSession()->status == SessionStatus::Running
               && repository.saveCalls == writesAfterStart,
           "Minimizing focus must preserve the exact running session");
    click(
        findByAutomationId(
            window.uiRoot().content(), "focus.active-session.open"),
        "Tasks should expose the active focus as a return target");
    expect(router.currentRoute() == FocusRoute::Timer
               && repository.saveCalls == writesAfterStart,
           "Returning to focus should navigate without persisting a command");
}

void completedFocusCanReturnToTasksWithoutAForcedNextStep()
{
    RecordingRepository repository;
    FocusDataService service(repository);
    std::int64_t now = 10'000;
    int id = 0;
    FocusViewModel viewModel(
        service,
        [&now] { return now; },
        [&id] { return "completion-nav-" + std::to_string(++id); });
    expect(viewModel.startFreeFocus().succeeded(),
           "Completion navigation fixture should start a free focus");
    now = *viewModel.activeSession()->targetEndAtUtcMs;
    expect(viewModel.updateClock(),
           "Completion navigation fixture should finish its focus");

    FocusAssets assets;
    wui::UiApp app(
        whatsui::gallery::capture::createHeadlessPlatformHost(1.0f));
    auto& window = app.openWindow("completion navigation", {520.0f, 720.0f});
    FocusRouter router(window, viewModel, assets, 520.0f, 720.0f);
    router.start();
    router.showCompletion();
    window.layout();
    const int writesAfterCompletion = repository.saveCalls;
    const std::size_t sessionsAfterCompletion = service.data().sessions.size();

    auto* back = findByAutomationId(
        window.uiRoot().content(), "focus.completion.back");
    expect(back != nullptr,
           "Completion should offer returning to tasks without choosing rest or focus");
    const auto bounds = back->bounds();
    const wui::PointF center{
        bounds.x + bounds.width * 0.5f,
        bounds.y + bounds.height * 0.5f,
    };
    expect(
        window.dispatchPointer(
            pointer(window.id(), wui::PointerAction::Down, center))
            && window.dispatchPointer(
                pointer(window.id(), wui::PointerAction::Up, center)),
        "The completion Back action should use normal pointer routing");
    window.layout();
    expect(router.currentRoute() == FocusRoute::Tasks
               && !service.data().activeSessionId
               && service.data().sessions.size() == sessionsAfterCompletion
               && repository.saveCalls == writesAfterCompletion,
           "Returning from completion must not create a break or another focus");
}

void minimizedTimersKeepRunningAndRemainRecoverable()
{
    RecordingRepository repository;
    FocusData data;
    data.settings.autoStartBreak = true;
    FocusDataService service(repository, data);
    std::int64_t now = 10'000;
    int id = 0;
    FocusViewModel viewModel(
        service,
        [&now] { return now; },
        [&id] { return "background-" + std::to_string(++id); });
    expect(viewModel.startFreeFocus().succeeded(),
           "Background navigation fixture should start a focus");

    FocusAssets assets;
    wui::UiApp app(
        whatsui::gallery::capture::createHeadlessPlatformHost(1.0f));
    auto& window = app.openWindow("background timer", {520.0f, 720.0f});
    FocusRouter router(window, viewModel, assets, 520.0f, 720.0f);
    router.start();
    window.layout();
    expect(router.currentRoute() == FocusRoute::Timer,
           "An active focus should initially open above the task root");

    router.goBack();
    expect(router.currentRoute() == FocusRoute::Tasks
               && viewModel.activeSession() != nullptr,
           "Minimizing should reveal tasks without ending focus");
    now = *viewModel.activeSession()->targetEndAtUtcMs;
    router.updateClock();
    window.layout();
    expect(router.currentRoute() == FocusRoute::Completion
               && viewModel.activeSession() == nullptr,
           "A minimized focus must still settle and present completion");

    router.goBack();
    expect(viewModel.startBreak().succeeded(),
           "A completed background focus should be able to start its break");
    router.showBreak();
    window.layout();
    const std::string breakId = viewModel.activeSession()->id;
    const int writesAfterBreakStart = repository.saveCalls;
    router.goBack();
    window.layout();
    expect(router.currentRoute() == FocusRoute::Tasks
               && viewModel.activeSession()->id == breakId
               && repository.saveCalls == writesAfterBreakStart,
           "Minimizing a break must not skip, pause, or rewrite it");

    auto* activeSession = findByAutomationId(
        window.uiRoot().content(), "focus.active-session.open");
    expect(activeSession != nullptr,
           "The task root should expose a minimized break");
    const auto bounds = activeSession->bounds();
    const wui::PointF center{
        bounds.x + bounds.width * 0.5f,
        bounds.y + bounds.height * 0.5f,
    };
    expect(
        window.dispatchPointer(
            pointer(window.id(), wui::PointerAction::Down, center))
            && window.dispatchPointer(
                pointer(window.id(), wui::PointerAction::Up, center)),
        "The minimized break banner should reopen through pointer input");
    window.layout();
    expect(router.currentRoute() == FocusRoute::Break
               && viewModel.activeSession()->id == breakId
               && repository.saveCalls == writesAfterBreakStart,
           "Reopening a minimized break must preserve its exact session");

    router.goBack();
    now = *viewModel.activeSession()->targetEndAtUtcMs;
    router.updateClock();
    window.layout();
    expect(router.currentRoute() == FocusRoute::Tasks
               && viewModel.activeSession() == nullptr
               && findByAutomationId(
                   window.uiRoot().content(),
                   "focus.active-session.open") == nullptr,
           "A minimized break must settle and remove its stale task-page banner");
}

void emptyTaskFreeFocusAndSpaceShortcutAreConnected()
{
    RecordingRepository repository;
    FocusDataService service(repository);
    std::int64_t now = 10'000;
    int id = 0;
    FocusViewModel viewModel(
        service,
        [&now] { return now; },
        [&id] { return "free-ui-" + std::to_string(++id); });
    FocusAssets assets;
    wui::UiApp app(
        whatsui::gallery::capture::createHeadlessPlatformHost(1.0f));
    auto& window = app.openWindow("free focus", {520.0f, 720.0f});
    FocusRouter router(window, viewModel, assets, 520.0f, 720.0f);
    router.start();
    window.layout();

    auto* freeFocus = findByAutomationId(
        window.uiRoot().content(), "focus.tasks.free-focus");
    expect(freeFocus != nullptr,
           "The empty task page must expose the promised free-focus action");
    const auto bounds = freeFocus->bounds();
    const wui::PointF center{
        bounds.x + bounds.width * 0.5f,
        bounds.y + bounds.height * 0.5f,
    };
    expect(
        window.dispatchPointer(
            pointer(window.id(), wui::PointerAction::Down, center))
            && window.dispatchPointer(
                pointer(window.id(), wui::PointerAction::Up, center)),
        "Clicking free focus should route through normal pointer input");
    window.layout();
    expect(router.currentRoute() == FocusRoute::Timer
               && viewModel.activeSession() != nullptr
               && !viewModel.activeSession()->taskId,
           "The UI action should create one task-free focus and open its timer");

    expect(window.dispatchKey(
               {window.id(), wui::KeyAction::Down, 32, 0, false}),
           "Space should be consumed by the timer page");
    expect(viewModel.activeSession()->status == SessionStatus::Paused,
           "The keyboard shortcut must call the same validated toggle command");
}

void taskManagementSupportsEditDeleteAndRestore()
{
    RecordingRepository repository;
    FocusData data;
    data.tasks.push_back({
        "managed-task", "待整理任务", TaskStatus::Active,
        2, 0, 1024, 1, 1'000, 1'000,
    });
    FocusDataService service(repository, data);
    std::int64_t now = 10'000;
    int id = 0;
    FocusViewModel viewModel(
        service,
        [&now] { return now; },
        [&id] { return "managed-" + std::to_string(++id); });
    FocusAssets assets;
    wui::UiApp app(
        whatsui::gallery::capture::createHeadlessPlatformHost(1.0f));
    auto& window = app.openWindow("task management", {520.0f, 720.0f});
    FocusRouter router(window, viewModel, assets, 520.0f, 720.0f);
    router.start();
    window.layout();

    auto click = [&window](wui::Node* node, const char* message) {
        expect(node != nullptr, message);
        const auto bounds = node->bounds();
        const wui::PointF center{
            bounds.x + bounds.width * 0.5f,
            bounds.y + bounds.height * 0.5f,
        };
        expect(
            window.dispatchPointer(
                pointer(window.id(), wui::PointerAction::Down, center))
                && window.dispatchPointer(
                    pointer(window.id(), wui::PointerAction::Up, center)),
            message);
        window.layout();
    };
    auto replaceFocusedText = [&window](std::string text) {
        expect(window.dispatchKey({
                   window.id(), wui::KeyAction::Down, 65,
                   wui::KeyModifierControl, false}),
               "Ctrl+A should select the editor value");
        expect(window.dispatchTextInput({window.id(), std::move(text)}),
               "The selected editor value should be replaceable");
    };

    click(
        findByAutomationId(
            window.uiRoot().content(),
            "focus.tasks.manage.managed-task"),
        "Every visible task should expose an independent management action");
    expect(window.hasDialog(),
           "The management action should open the task editor");
    replaceFocusedText("整理发布文档");
    auto* estimate = findByAutomationId(
        window.overlayHost().top()->content.get(),
        "focus.edit-task.estimate");
    expect(estimate != nullptr, "The editor should expose the estimate");
    window.focusManager().setFocused(estimate);
    replaceFocusedText("4");
    auto* focusMinutes = findByAutomationId(
        window.overlayHost().top()->content.get(),
        "focus.edit-task.focus-minutes");
    expect(focusMinutes != nullptr,
           "The editor should expose task-specific focus duration");
    window.focusManager().setFocused(focusMinutes);
    replaceFocusedText("40");
    click(
        findRadioByValue(
            window.overlayHost().top()->content.get(), "forest"),
        "The editor should offer a real forest-sound preference option");
    click(
        findByAutomationId(
            window.overlayHost().top()->content.get(),
            "focus.edit-task.save"),
        "Saving the task editor should use a real button target");
    window.update();
    window.layout();
    expect(service.data().tasks.front().title == "整理发布文档"
               && service.data().tasks.front().estimatedPomodoros == 4
               && service.data().tasks.front().execution.focusMinutes
                    == std::optional<int>{40}
               && service.data().tasks.front().execution.sound
                    == TaskSoundPreference::Soundscape
               && service.data().tasks.front().execution.soundscapeId
                    == "forest"
               && service.data().tasks.front().revision == 2,
           "Task editing should persist content and execution preferences in one revision");

    ++now;
    click(
        findByAutomationId(
            window.uiRoot().content(),
            "focus.tasks.manage.managed-task"),
        "The updated row should remain manageable");
    click(
        findByAutomationId(
            window.overlayHost().top()->content.get(),
            "focus.edit-task.delete"),
        "Deleting should be available from the editor");
    window.update();
    window.layout();
    expect(window.hasDialog(),
           "Deletion should require a second explicit confirmation");
    click(
        findByAutomationId(
            window.overlayHost().top()->content.get(),
            "focus.confirm.accept"),
        "The destructive confirmation should be actionable");
    window.update();
    window.layout();
    expect(service.data().tasks.front().status == TaskStatus::Archived,
           "Deletion should archive instead of breaking historical references");

    click(
        findByAutomationId(
            window.uiRoot().content(),
            "focus.tasks.filter.deleted"),
        "The task list should expose a deleted-task filter");
    click(
        findByAutomationId(
            window.uiRoot().content(),
            "focus.tasks.restore.managed-task"),
        "A deleted task should expose a one-step restore action");
    expect(service.data().tasks.front().status == TaskStatus::Active
               && service.data().tasks.front().revision == 4,
           "Restoring should return the task to active management");

    ++now;
    viewModel.selectTask("managed-task");
    expect(viewModel.startSelectedFocus().succeeded(),
           "The restored task should be usable for a new focus session");
    expect(viewModel.activeSession()->plannedDurationMs == 40 * kMinuteMs
               && viewModel.activeSession()->soundscapeIdSnapshot
                    == std::optional<std::string>{"forest"},
           "Starting focus must resolve task preferences into immutable session snapshots");
    viewModel.setTaskFilter(TaskFilter::All);
    router.showTasks();
    window.layout();
    click(
        findByAutomationId(
            window.uiRoot().content(),
            "focus.tasks.manage.managed-task"),
        "An active timer task should remain editable");
    auto* protectedDelete = findByAutomationId(
        window.overlayHost().top()->content.get(),
        "focus.edit-task.delete");
    const auto* protectedDeleteControl =
        dynamic_cast<wui::ControlNode*>(protectedDelete);
    expect(protectedDeleteControl != nullptr
               && !protectedDeleteControl->isEnabled(),
           "The editor must disable deletion while the task owns the active timer");
}

} // namespace

int main()
{
    try {
        focusThemeOwnsEveryBrandInteractionToken();
        taskExecutionPreferenceDraftsAreExplicitAndValidated();
        successfulSubmitRefreshesOnlyAfterDialogDismissal();
        rejectedSubmitsKeepTheDialogAndNeverRefreshThePage();
        routerCanOutliveAClosedUiWindow();
        renderedPageCallbacksExpireWithTheirRouter();
        taskFilterAndCompletionControlsDrivePersistedUiState();
        destructiveTimerActionsRequireExplicitConfirmation();
        reversibleNavigationNeverMutatesTheActiveSession();
        completedFocusCanReturnToTasksWithoutAForcedNextStep();
        minimizedTimersKeepRunningAndRemainRecoverable();
        emptyTaskFreeFocusAndSpaceShortcutAreConnected();
        taskManagementSupportsEditDeleteAndRestore();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
