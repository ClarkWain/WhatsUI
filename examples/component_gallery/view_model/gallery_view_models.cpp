#include "view_model/gallery_view_models.h"

namespace whatsui::gallery {

GalleryViewModels::GalleryViewModels()
    : gallery_(catalog_)
{
}

const ComponentCatalog& GalleryViewModels::catalog() const noexcept { return catalog_; }
NavigationViewModel& GalleryViewModels::navigation() noexcept { return navigation_; }
GalleryViewModel& GalleryViewModels::gallery() noexcept { return gallery_; }
VisualQaViewModel& GalleryViewModels::visualQa() noexcept { return visualQa_; }
ButtonDetailViewModel& GalleryViewModels::buttonDetail() noexcept { return buttonDetail_; }

} // namespace whatsui::gallery
