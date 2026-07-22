#pragma once

#include <functional>
#include <memory>

#include "domain/gallery_route.h"
#include "wui/node.h"

namespace whatsui::gallery::view {

using ShellNavigateHandler = std::function<void(GalleryRoute)>;

[[nodiscard]] std::unique_ptr<wui::Node> buildAppShell(
    GalleryRoute selectedRoute,
    std::unique_ptr<wui::Node> pageContent,
    ShellNavigateHandler navigate = {});

} // namespace whatsui::gallery::view
