#include "application/focus_data_service.h"
#include "infrastructure/file_focus_repository.h"
#include "presentation/focus_assets.h"
#include "presentation/dialogs/new_task_dialog.h"
#include "presentation/focus_router.h"
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
        FocusViewModel viewModel(service, nowUtcMs, nextId);
        const FocusAssets assets =
            loadFocusAssets(std::filesystem::path(FOCUS_TOMATO_ASSET_DIR));

        wui::UiApp application(wui::createGlfwPlatformHost());
        auto& window = application.openWindow("FocusTomato", {width, height});
        FocusRouter router(window, viewModel, assets, width, height);
        router.start();
        window.platformWindow().show();

        auto timer = wui::Animation(
            0.25f, [&router](float) { router.updateClock(); });
        timer.repeat(-1);
        const auto timerId = wui::Ticker::instance().add(std::move(timer));
        auto smokeState = std::make_shared<NewTaskSmokeState>();
        if (newTaskSmoke) {
            installNewTaskNativeSmoke(window, viewModel, smokeState);
        }
        const int exitCode = wui::runGlfwUiApp(application);
        wui::Ticker::instance().cancel(timerId);
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
