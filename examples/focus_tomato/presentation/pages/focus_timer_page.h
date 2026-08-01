#pragma once

#include <functional>

#include "../focus_assets.h"
#include "../focus_view_model.h"
#include "wui/declarative/layout.h"

namespace whatsui::focus_tomato::presentation {

struct FocusTimerPageActions {
    std::function<void()> toggle;
    std::function<void()> reset;
    std::function<void()> abort;
    std::function<void()> recordInterruption;
};

[[nodiscard]] wui::Box buildFocusTimerPage(
    FocusViewModel& viewModel,
    const FocusAssets& assets,
    float pageWidth,
    float pageHeight,
    FocusTimerPageActions actions);

} // namespace whatsui::focus_tomato::presentation
