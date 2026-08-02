#pragma once

#include <functional>
#include <string>
#include <utility>

#include "../focus_assets.h"
#include "../focus_view_model.h"
#include "wui/declarative/layout.h"

namespace whatsui::focus_tomato::presentation {

struct TaskListPageActions {
    std::function<void(std::string)> selectTask;
    std::function<void()> createTask;
};

class TaskListPage {
public:
    TaskListPage(FocusViewModel& viewModel,
                 const FocusAssets& assets,
                 float pageWidth,
                 float pageHeight,
                 TaskListPageActions actions)
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
    TaskListPageActions actions_;
};

} // namespace whatsui::focus_tomato::presentation
