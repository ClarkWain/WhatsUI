#pragma once

#include <functional>
#include <string>

#include "../focus_assets.h"
#include "../focus_view_model.h"
#include "wui/declarative/layout.h"

namespace whatsui::focus_tomato::presentation {

struct TaskListPageActions {
    std::function<void(std::string)> selectTask;
    std::function<void()> createTask;
};

[[nodiscard]] wui::Box buildTaskListPage(
    FocusViewModel& viewModel,
    const FocusAssets& assets,
    float pageWidth,
    float pageHeight,
    TaskListPageActions actions);

} // namespace whatsui::focus_tomato::presentation
