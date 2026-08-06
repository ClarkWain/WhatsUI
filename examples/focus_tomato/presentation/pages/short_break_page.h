#pragma once

#include <functional>
#include <utility>

#include "../focus_assets.h"
#include "../focus_view_model.h"
#include "wui/declarative/layout.h"

namespace whatsui::focus_tomato::presentation {

struct BreakTimerPageActions {
    std::function<void()> minimize;
    std::function<void()> toggle;
    std::function<void()> reset;
    std::function<void()> skip;
};

class BreakTimerPage {
public:
    BreakTimerPage(FocusViewModel& viewModel,
                   const FocusAssets& assets,
                   float pageWidth,
                   float pageHeight,
                   BreakTimerPageActions actions)
        : viewModel_(&viewModel)
        , assets_(&assets)
        , pageWidth_(pageWidth)
        , pageHeight_(pageHeight)
        , actions_(std::move(actions))
    {
    }

    [[nodiscard]] wui::Box body();

private:
    FocusViewModel* viewModel_;
    const FocusAssets* assets_;
    float pageWidth_;
    float pageHeight_;
    BreakTimerPageActions actions_;
};

} // namespace whatsui::focus_tomato::presentation
