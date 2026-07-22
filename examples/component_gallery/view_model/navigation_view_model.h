#pragma once

#include "domain/gallery_route.h"
#include "wui/state.h"

namespace whatsui::gallery {

class NavigationViewModel {
public:
    [[nodiscard]] wui::State<GalleryRoute>& currentRoute() noexcept;
    [[nodiscard]] const wui::State<GalleryRoute>& currentRoute() const noexcept;
    [[nodiscard]] bool isSelected(GalleryRoute route) const noexcept;
    void select(GalleryRoute route);

private:
    wui::State<GalleryRoute> currentRoute_{GalleryRoute::Overview};
};

} // namespace whatsui::gallery
