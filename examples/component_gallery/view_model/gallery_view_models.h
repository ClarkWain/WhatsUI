#pragma once

#include "domain/component_catalog.h"
#include "view_model/button_detail_view_model.h"
#include "view_model/gallery_view_model.h"
#include "view_model/navigation_view_model.h"
#include "view_model/theme_studio_view_model.h"
#include "view_model/visual_qa_view_model.h"

namespace whatsui::gallery {

class GalleryViewModels {
public:
    GalleryViewModels();

    [[nodiscard]] const ComponentCatalog& catalog() const noexcept;
    [[nodiscard]] NavigationViewModel& navigation() noexcept;
    [[nodiscard]] GalleryViewModel& gallery() noexcept;
    [[nodiscard]] VisualQaViewModel& visualQa() noexcept;
    [[nodiscard]] ButtonDetailViewModel& buttonDetail() noexcept;
    [[nodiscard]] ThemeStudioViewModel& themeStudio() noexcept;

private:
    ComponentCatalog catalog_;
    NavigationViewModel navigation_;
    GalleryViewModel gallery_;
    VisualQaViewModel visualQa_;
    ButtonDetailViewModel buttonDetail_;
    ThemeStudioViewModel themeStudio_;
};

} // namespace whatsui::gallery
