#pragma once

#include <functional>
#include <string>

namespace wui {
class UiWindow;
}

namespace whatsui::focus_tomato::presentation {

struct ConfirmationDialogSpec {
    std::string title;
    std::string message;
    std::string confirmLabel;
};

void showConfirmationDialog(
    wui::UiWindow& window,
    ConfirmationDialogSpec spec,
    std::function<void()> onConfirm);

} // namespace whatsui::focus_tomato::presentation
