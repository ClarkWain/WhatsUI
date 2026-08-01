#pragma once

#include <functional>

#include "../focus_assets.h"
#include "../focus_view_model.h"
#include "wui/declarative/layout.h"

namespace whatsui::focus_tomato::presentation {

struct CompletionPageActions {
    std::function<void()> startBreak;
    std::function<void()> continueFocus;
};

[[nodiscard]] wui::Box buildCompletionPage(
    FocusViewModel& viewModel,
    const FocusAssets& assets,
    float pageWidth,
    float pageHeight,
    CompletionPageActions actions);

} // namespace whatsui::focus_tomato::presentation
