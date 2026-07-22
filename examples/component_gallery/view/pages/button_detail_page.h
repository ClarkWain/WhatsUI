#pragma once

#include <functional>
#include <memory>

#include "view_model/button_detail_view_model.h"
#include "wui/node.h"

namespace whatsui::gallery::view::pages {

// Lifetime contract: viewModel must outlive the returned Node tree; the page
// unsubscribes all State observers from the root Node teardown callback.

[[nodiscard]] std::unique_ptr<wui::Node> buildButtonDetailPage(
    ButtonDetailViewModel& viewModel,
    std::function<void()> onBack = {});

} // namespace whatsui::gallery::view::pages
