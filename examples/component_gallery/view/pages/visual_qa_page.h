#pragma once

#include <memory>

#include "view_model/visual_qa_view_model.h"
#include "wui/app.h"
#include "wui/node.h"

namespace whatsui::gallery::view::pages {

// Lifetime contract: viewModel and window must outlive the returned Node tree.

[[nodiscard]] std::unique_ptr<wui::Node> buildVisualQaPage(
    VisualQaViewModel& viewModel,
    wui::UiWindow& window);

} // namespace whatsui::gallery::view::pages
