#pragma once

#include <functional>
#include <memory>

#include "../focus_assets.h"
#include "../focus_view_model.h"
#include "wui/node.h"

namespace whatsui::focus_tomato::presentation {

struct ShortBreakPageActions {
    std::function<void()> toggle;
    std::function<void()> reset;
    std::function<void()> skip;
};

[[nodiscard]] std::unique_ptr<wui::Node> buildShortBreakPage(
    FocusViewModel& viewModel,
    const FocusAssets& assets,
    float pageWidth,
    float pageHeight,
    ShortBreakPageActions actions);

} // namespace whatsui::focus_tomato::presentation
