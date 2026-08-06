#pragma once

#include <functional>
#include <utility>

#include "../focus_assets.h"
#include "../focus_view_model.h"
#include "wui/declarative/layout.h"

namespace whatsui::focus_tomato::presentation {

struct CompletionPageActions {
    std::function<void()> returnToTasks;
    std::function<void()> startBreak;
    std::function<void()> continueFocus;
};

class CompletionPage {
public:
    CompletionPage(FocusViewModel& viewModel,
                   const FocusAssets& assets,
                   float pageWidth,
                   float pageHeight,
                   CompletionPageActions actions)
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
    CompletionPageActions actions_;
};

} // namespace whatsui::focus_tomato::presentation
