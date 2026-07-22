#include "view_model/navigation_view_model.h"

namespace whatsui::gallery {

wui::State<GalleryRoute>& NavigationViewModel::currentRoute() noexcept { return currentRoute_; }
const wui::State<GalleryRoute>& NavigationViewModel::currentRoute() const noexcept { return currentRoute_; }
bool NavigationViewModel::isSelected(GalleryRoute route) const noexcept { return currentRoute_.get() == route; }
void NavigationViewModel::select(GalleryRoute route) { currentRoute_.set(route); }

} // namespace whatsui::gallery
