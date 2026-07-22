#pragma once

#include <functional>
#include <memory>

#include "view_model/visual_qa_view_model.h"
#include "wui/app.h"
#include "wui/node.h"

namespace whatsui::gallery::view::pages {

// Lifetime contract: viewModel and window must outlive the returned Node tree.

using ApplyVisualQaThemeHandler = std::function<void(ThemePreview theme)>;

[[nodiscard]] std::unique_ptr<wui::Node> buildVisualQaPage(
    VisualQaViewModel& viewModel,
    wui::UiWindow& window,
    ApplyVisualQaThemeHandler applyTheme);

} // namespace whatsui::gallery::view::pages
