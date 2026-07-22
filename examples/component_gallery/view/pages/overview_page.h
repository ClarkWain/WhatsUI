#pragma once

#include <functional>
#include <memory>

#include "domain/component_catalog.h"
#include "domain/gallery_route.h"
#include "view_model/gallery_view_model.h"
#include "wui/node.h"

namespace whatsui::gallery::view::pages {

// Lifetime contract: gallery must outlive the returned Node tree because
// navigation callbacks keep a non-owning reference to it.

using NavigateHandler = std::function<void(GalleryRoute)>;

[[nodiscard]] std::unique_ptr<wui::Node> buildOverviewPage(
    const ComponentCatalog& catalog,
    GalleryViewModel& gallery,
    NavigateHandler navigate);

} // namespace whatsui::gallery::view::pages
