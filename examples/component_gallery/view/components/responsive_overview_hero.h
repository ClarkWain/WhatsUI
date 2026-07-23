#pragma once

#include <functional>
#include <memory>

#include "domain/gallery_route.h"
#include "view_model/gallery_view_model.h"
#include "wui/node.h"

namespace whatsui::gallery::view::components {

using OverviewNavigateHandler = std::function<void(GalleryRoute)>;

// Keeps the overview hero legible when a desktop Gallery is resized to a
// phone-width logical viewport. The compact variant intentionally omits the
// decorative preview so the primary actions and content stay first-class.
[[nodiscard]] std::unique_ptr<wui::Node> buildResponsiveOverviewHero(
    GalleryViewModel& gallery, OverviewNavigateHandler navigate);

} // namespace whatsui::gallery::view::components
