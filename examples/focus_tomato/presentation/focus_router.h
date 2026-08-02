#pragma once

#include <cstddef>
#include <string_view>

#include "focus_assets.h"
#include "focus_view_model.h"
#include "wui/component.h"
#include "wui/view.h"

namespace wui {
class UiWindow;
}

namespace whatsui::focus_tomato::presentation {

enum class FocusRoute {
    Tasks,
    Setup,
    Timer,
    Completion,
    ShortBreak,
};

class FocusRouter {
public:
    FocusRouter(wui::UiWindow& window,
                FocusViewModel& viewModel,
                const FocusAssets& assets,
                float pageWidth,
                float pageHeight);
    ~FocusRouter();

    FocusRouter(const FocusRouter&) = delete;
    FocusRouter& operator=(const FocusRouter&) = delete;

    void start();
    void showTasks();
    void showSetup();
    void showTimer();
    void showCompletion();
    void showShortBreak();
    void refresh();
    void updateClock();
    void shutdown() noexcept;

    [[nodiscard]] FocusRoute currentRoute() const noexcept;

private:
    [[nodiscard]] wui::View buildCurrentPage();
    void install(FocusRoute route);
    void requestNewTask();

    wui::UiWindow* window_{nullptr};
    FocusViewModel* viewModel_{nullptr};
    const FocusAssets* assets_{nullptr};
    float pageWidth_{0.0f};
    float pageHeight_{0.0f};
    FocusRoute currentRoute_{FocusRoute::Tasks};
    wui::CallbackLifetime lifetime_;
};

[[nodiscard]] std::string_view focusRouteKey(FocusRoute route) noexcept;

} // namespace whatsui::focus_tomato::presentation
