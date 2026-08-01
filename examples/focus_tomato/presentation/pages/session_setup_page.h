#pragma once

#include <functional>
#include <memory>

#include "../focus_assets.h"
#include "../focus_view_model.h"
#include "wui/node.h"

namespace whatsui::focus_tomato::presentation {

struct SessionSetupPageActions {
    std::function<void()> start;
    std::function<void()> back;
};

[[nodiscard]] std::unique_ptr<wui::Node> buildSessionSetupPage(
    FocusViewModel& viewModel,
    const FocusAssets& assets,
    float pageWidth,
    float pageHeight,
    SessionSetupPageActions actions);

} // namespace whatsui::focus_tomato::presentation
