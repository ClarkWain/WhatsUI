#pragma once

#include <memory>

#include "domain/gallery_route.h"
#include "wui/node.h"

namespace whatsui::gallery {
class GalleryRouter;
class GalleryViewModels;
}

namespace whatsui::gallery::view {

[[nodiscard]] std::unique_ptr<wui::Node> buildGalleryPage(
    GalleryRoute route,
    GalleryViewModels& viewModels,
    GalleryRouter& router);

} // namespace whatsui::gallery::view
