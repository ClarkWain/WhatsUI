#pragma once

#include <functional>

#include "../focus_view_model.h"

namespace wui {
class UiWindow;
}

namespace whatsui::focus_tomato::presentation {

using TaskCreatedCallback = std::function<void()>;

void showNewTaskDialog(wui::UiWindow& window,
                       FocusViewModel& viewModel,
                       TaskCreatedCallback onCreated);

} // namespace whatsui::focus_tomato::presentation
