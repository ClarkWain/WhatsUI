#pragma once

#include <functional>
#include <memory>

#include "domain/gallery_route.h"
#include "view/components/navigation_rail.h"
#include "wui/app.h"
#include "wui/node.h"

namespace whatsui::gallery::view::components {

using CompactNavigationHandler = std::function<void(GalleryRoute)>;

// The compact top bar is deliberately separate from the desktop rail.  It
// keeps narrow windows useful without duplicating routing state in the view.
[[nodiscard]] std::unique_ptr<wui::Node> buildCompactNavigationBar(
    const NavigationRailConfig& config,
    CompactNavigationHandler onNavigate,
    wui::UiWindow& window);

} // namespace whatsui::gallery::view::components
