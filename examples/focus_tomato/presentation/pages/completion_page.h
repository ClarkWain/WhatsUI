#pragma once

#include <functional>
#include <memory>

#include "../focus_assets.h"
#include "../focus_view_model.h"
#include "wui/node.h"

namespace whatsui::focus_tomato::presentation {

struct CompletionPageActions {
    std::function<void()> startBreak;
    std::function<void()> continueFocus;
};

[[nodiscard]] std::unique_ptr<wui::Node> buildCompletionPage(
    FocusViewModel& viewModel,
    const FocusAssets& assets,
    float pageWidth,
    float pageHeight,
    CompletionPageActions actions);

} // namespace whatsui::focus_tomato::presentation
