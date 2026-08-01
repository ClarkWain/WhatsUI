#include "application/focus_repository.h"
#include "capture/headless_platform.h"
#include "presentation/dialogs/new_task_dialog.h"
#include "presentation/focus_router.h"
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

wui::Node* findByAccessibilityId(
    wui::Node* node, const std::string& automationId)
{
    if (node == nullptr) return nullptr;
    if (node->automationId() == automationId) return node;
    for (const auto& child : node->children()) {
        if (auto* match = findByAccessibilityId(
                child.get(), automationId)) {
            return match;
        }
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
    auto newTask = std::make_unique<wui::ButtonNode>("新建任务");
    auto* newTaskRaw = newTask.get();
    newTask->onClick([&] {
        showNewTaskDialog(window, viewModel, [&] {
            refreshed = true;
            refreshSawDialog = window.hasDialog();
            window.setRoot(wui::Text("已刷新任务列表").build());
        });
    });
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
    window.layout();
    auto* saveButton = findByAccessibilityId(
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
        window.setRoot(wui::Text("任务列表").build());
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
        window.setRoot(wui::Text("任务列表").build());
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

} // namespace

int main()
{
    try {
        successfulSubmitRefreshesOnlyAfterDialogDismissal();
        rejectedSubmitsKeepTheDialogAndNeverRefreshThePage();
        routerCanOutliveAClosedUiWindow();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
