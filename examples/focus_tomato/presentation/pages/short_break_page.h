#pragma once

#include <functional>

#include "../focus_assets.h"
#include "../focus_view_model.h"
#include "wui/declarative/layout.h"

namespace whatsui::focus_tomato::presentation {

struct ShortBreakPageActions {
    std::function<void()> toggle;
    std::function<void()> reset;
    std::function<void()> skip;
};

[[nodiscard]] wui::Box buildShortBreakPage(
    FocusViewModel& viewModel,
    const FocusAssets& assets,
    float pageWidth,
    float pageHeight,
    ShortBreakPageActions actions);

} // namespace whatsui::focus_tomato::presentation
