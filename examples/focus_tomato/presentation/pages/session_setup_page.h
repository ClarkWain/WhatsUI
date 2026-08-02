#pragma once

#include <functional>
#include <utility>

#include "../focus_assets.h"
#include "../focus_view_model.h"
#include "wui/declarative/layout.h"

namespace whatsui::focus_tomato::presentation {

struct SessionSetupPageActions {
    std::function<void()> start;
    std::function<void()> back;
};

class SessionSetupPage {
public:
    SessionSetupPage(FocusViewModel& viewModel,
                     const FocusAssets& assets,
                     float pageWidth,
                     float pageHeight,
                     SessionSetupPageActions actions)
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
    SessionSetupPageActions actions_;
};

} // namespace whatsui::focus_tomato::presentation
