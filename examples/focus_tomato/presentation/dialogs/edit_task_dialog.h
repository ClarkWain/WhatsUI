#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "../focus_view_model.h"

namespace wui {
class UiWindow;
}

namespace whatsui::focus_tomato::presentation {

using TaskEditedCallback = std::function<void()>;
using TaskDeleteRequestCallback = std::function<void(
    std::string taskId,
    std::string taskTitle,
    std::int64_t expectedRevision)>;

[[nodiscard]] bool showEditTaskDialog(
    wui::UiWindow& window,
    FocusViewModel& viewModel,
    const std::string& taskId,
    TaskEditedCallback onEdited,
    TaskDeleteRequestCallback onDeleteRequested);

} // namespace whatsui::focus_tomato::presentation
