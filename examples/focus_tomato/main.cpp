#include "application/focus_data_service.h"
#include "infrastructure/file_focus_repository.h"
#include "presentation/focus_assets.h"
#include "presentation/dialogs/confirmation_dialog.h"
#include "presentation/dialogs/new_task_dialog.h"
#include "presentation/focus_router.h"
#include "presentation/focus_style.h"
#include "presentation/focus_view_model.h"

#include "wui/animation.h"
#include "wui/glfw_platform.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using namespace whatsui::focus_tomato;
using namespace whatsui::focus_tomato::presentation;

std::int64_t nowUtcMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::int64_t monotonicMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string nextId()
{
    static std::atomic<std::uint64_t> sequence{1};
    return "local-" + std::to_string(nowUtcMs()) + "-"
        + std::to_string(sequence.fetch_add(1));
}

std::filesystem::path applicationStorePath()
{
    if (const char* localAppData = std::getenv("LOCALAPPDATA");
        localAppData != nullptr && *localAppData != '\0') {
        return std::filesystem::path(localAppData)
            / "WhatsUI" / "FocusTomato" / "focus.store";
    }
    return std::filesystem::current_path()
        / "WhatsUI" / "FocusTomato" / "focus.store";
}

FocusData loadInitialData(FileFocusRepository& repository)
{
    auto loaded = repository.load();
    if (!loaded.hasUsableData()) {
        std::cerr << "FocusTomato: " << loaded.message << std::endl;
        return {};
    }
    if (!loaded.message.empty()) {
        std::cout << "FocusTomato: " << loaded.message << std::endl;
    }
    return std::move(loaded.data);
}

struct NewTaskSmokeState {
    bool dialogOpened{false};
    bool dialogRendered{false};
};

struct DesktopIntegrationState {
    bool forceExit{false};
    bool trayInstalled{false};
    std::string sessionId;
    bool sessionRunning{false};
    wui::DesktopIcon trayIcon;
};

wui::TrayIconOptions makeTrayOptions(
    const FocusViewModel& viewModel,
    const wui::DesktopIcon& icon)
{
    const bool hasSession = viewModel.activeSession() != nullptr;
    return {
        "FocusTomato 番茄钟",
        icon,
        {
            {"show", "打开 FocusTomato"},
            {"toggle-session",
             viewModel.isRunning() ? "暂停当前计时" : "继续当前计时",
             wui::TrayMenuItemKind::Action,
             hasSession},
            {{}, {}, wui::TrayMenuItemKind::Separator},
            {"exit", "退出 FocusTomato"},
        },
        "show",
    };
}

void updateTrayState(
    wui::DesktopServices& desktop,
    const FocusViewModel& viewModel,
    DesktopIntegrationState& state,
    bool force = false)
{
    const auto* session = viewModel.activeSession();
    const std::string sessionId = session == nullptr ? "" : session->id;
    const bool running = viewModel.isRunning();
    if (!force && sessionId == state.sessionId
        && running == state.sessionRunning) {
        return;
    }
    state.sessionId = sessionId;
    state.sessionRunning = running;
    state.trayInstalled = desktop.setTrayIcon(
        makeTrayOptions(viewModel, state.trayIcon))
        == wui::DesktopOperationResult::Succeeded;
}

void showMainWindow(wui::UiWindow& window)
{
    window.platformWindow().show();
    window.platformWindow().restore();
    window.platformWindow().focus();
}

void requestApplicationExit(
    wui::UiWindow& window,
    const FocusViewModel& viewModel,
    const std::shared_ptr<DesktopIntegrationState>& state)
{
    if (viewModel.activeSession() == nullptr) {
        state->forceExit = true;
        window.platformWindow().close();
        return;
    }
    showMainWindow(window);
    if (window.hasDialog()) return;
    showConfirmationDialog(
        window,
        {
            "退出 FocusTomato？",
            "当前计时进度已经保存。退出后计时会停止，下次启动时可继续。",
            "仍要退出",
        },
        [&window, state] {
            state->forceExit = true;
            window.platformWindow().close();
        });
}

void installNewTaskNativeSmoke(
    wui::UiWindow& window,
    FocusViewModel& viewModel,
    const std::shared_ptr<NewTaskSmokeState>& state)
{
    auto smoke = wui::Animation(
        0.9f,
        [&window, &viewModel, state](float progress) {
            if (!state->dialogOpened && progress >= 0.20f) {
                state->dialogOpened = true;
                showNewTaskDialog(window, viewModel, [] {});
                std::cout << "FocusTomato new-task smoke: dialog-opened"
                          << std::endl;
            }
            if (state->dialogOpened && window.hasDialog()
                && progress >= 0.65f) {
                state->dialogRendered = true;
            }
        });
    smoke.onComplete([&window] {
        window.platformWindow().close();
    });
    (void)wui::Ticker::instance().add(std::move(smoke));
}

} // namespace

int main(int argc, char** argv)
{
    try {
        constexpr float width = 520.0f;
        constexpr float height = 720.0f;

        bool newTaskSmoke = false;
        for (int index = 1; index < argc; ++index) {
            if (std::string(argv[index]) == "--new-task-smoke") {
                newTaskSmoke = true;
            }
        }

        const std::filesystem::path storePath = newTaskSmoke
            ? std::filesystem::temp_directory_path()
                / "whatsui-focus-new-task-smoke.store"
            : applicationStorePath();
        FileFocusRepository repository(storePath);
        FocusDataService service(repository, loadInitialData(repository));
        FocusViewModel viewModel(service, nowUtcMs, nextId, monotonicMs);
        const FocusAssets assets =
            loadFocusAssets(std::filesystem::path(FOCUS_TOMATO_ASSET_DIR));

        wui::setTheme(style::focusTheme());
        wui::UiApp application(wui::createGlfwPlatformHost());
        wui::WindowOptions windowOptions;
        windowOptions.title = "FocusTomato";
        windowOptions.initialSize = {width, height};
        windowOptions.minimumSize = {width, height};
        windowOptions.maximumSize = {width, height};
        windowOptions.frameStyle = wui::WindowFrameStyle::Custom;
        windowOptions.resizable = false;
        windowOptions.visibleOnCreate = false;
        auto& window = application.openWindow(windowOptions);
        FocusRouter router(window, viewModel, assets, width, height);
        router.start();

        auto& desktop = application.desktopServices();
        auto desktopState = std::make_shared<DesktopIntegrationState>();
        desktopState->trayIcon = {
            assets.brandTomato.rgbaPixels(),
            assets.brandTomato.pixelWidth(),
            assets.brandTomato.pixelHeight(),
        };
        updateTrayState(desktop, viewModel, *desktopState, true);
        window.platformWindow().setCloseRequestHandler(
            [&window, &viewModel, desktopState] {
                if (!desktopState->forceExit
                    && desktopState->trayInstalled
                    && !desktopState->sessionId.empty()) {
                    return wui::WindowCloseDecision::Hide;
                }
                if (!desktopState->forceExit
                    && viewModel.activeSession() != nullptr) {
                    requestApplicationExit(
                        window, viewModel, desktopState);
                    return wui::WindowCloseDecision::Cancel;
                }
                return wui::WindowCloseDecision::Close;
            });
        desktop.setEventHandler(
            [&window, &viewModel, &router, &desktop, desktopState](
                const wui::DesktopEvent& event) {
                if (event.kind == wui::DesktopEventKind::NotificationActivated) {
                    showMainWindow(window);
                    if (event.payload == "focus-completed") {
                        router.showCompletion();
                    } else {
                        router.showActiveSession();
                    }
                    return;
                }
                if (event.id == "show") {
                    showMainWindow(window);
                } else if (event.id == "toggle-session") {
                    if (viewModel.activeSession() != nullptr) {
                        (void)viewModel.toggleActiveSession();
                        router.refresh();
                        updateTrayState(
                            desktop, viewModel, *desktopState, true);
                    }
                } else if (event.id == "exit") {
                    requestApplicationExit(
                        window, viewModel, desktopState);
                }
            });
        showMainWindow(window);

        auto timer = wui::Animation(
            0.25f,
            [&router, &viewModel, &desktop, desktopState](float) {
                const auto* active = viewModel.activeSession();
                const bool hadActiveSession = active != nullptr;
                const SessionType completingType = hadActiveSession
                    ? active->type
                    : SessionType::Focus;
                const std::string completedTitle = hadActiveSession
                    ? active->titleSnapshot
                    : std::string{};
                router.updateClock();
                if (hadActiveSession
                    && viewModel.activeSession() == nullptr) {
                    const bool focusCompleted =
                        completingType == SessionType::Focus;
                    (void)desktop.showNotification({
                        focusCompleted ? "focus-completed" : "break-completed",
                        focusCompleted ? "一个番茄完成了" : "休息结束了",
                        focusCompleted
                            ? (completedTitle.empty()
                                   ? "做得好，休息一下再继续。"
                                   : completedTitle + " 已完成一轮专注。")
                            : "准备好后，开始下一轮专注。",
                        focusCompleted ? "focus-completed" : "break-completed",
                    });
                }
                updateTrayState(desktop, viewModel, *desktopState);
            });
        timer.repeat(-1);
        const auto timerId = wui::Ticker::instance().add(std::move(timer));
        auto smokeState = std::make_shared<NewTaskSmokeState>();
        if (newTaskSmoke) {
            installNewTaskNativeSmoke(window, viewModel, smokeState);
        }
        const int exitCode = wui::runGlfwUiApp(application);
        wui::Ticker::instance().cancel(timerId);
        desktop.setEventHandler({});
        desktop.removeTrayIcon();
        if (newTaskSmoke) {
            if (!smokeState->dialogOpened || !smokeState->dialogRendered) {
                std::cerr << "FocusTomato new-task smoke: failed"
                          << std::endl;
                return 2;
            }
            std::cout << "FocusTomato new-task smoke: passed"
                      << std::endl;
        }
        return exitCode;
    } catch (const std::exception& error) {
        std::cerr << "FocusTomato: " << error.what() << std::endl;
        return 1;
    }
}
