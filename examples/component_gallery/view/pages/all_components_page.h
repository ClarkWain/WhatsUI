#pragma once

#include <functional>
#include <memory>
#include <string>

#include "view_model/gallery_view_model.h"
#include "wui/node.h"

namespace whatsui::gallery::view::pages {

// Lifetime contract: viewModel must outlive the returned Node tree because
// control callbacks and structural bindings retain non-owning references.

using OpenComponentHandler = std::function<void(const std::string& componentId)>;

[[nodiscard]] std::unique_ptr<wui::Node> buildAllComponentsPage(
    GalleryViewModel& viewModel,
    OpenComponentHandler onOpenComponent = {});

} // namespace whatsui::gallery::view::pages
