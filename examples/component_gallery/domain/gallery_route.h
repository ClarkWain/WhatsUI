#pragma once

#include <string_view>

namespace whatsui::gallery {

enum class GalleryRoute {
    Overview,
    AllComponents,
    Controls,
    LongText,
    AddOns,
    VisualQa,
    About,
    ButtonDetail,
};

[[nodiscard]] constexpr std::string_view galleryRouteKey(GalleryRoute route) noexcept
{
    switch (route) {
    case GalleryRoute::Overview: return "overview";
    case GalleryRoute::AllComponents: return "all-components";
    case GalleryRoute::Controls: return "controls";
    case GalleryRoute::LongText: return "long-text";
    case GalleryRoute::AddOns: return "add-ons";
    case GalleryRoute::VisualQa: return "visual-qa";
    case GalleryRoute::About: return "about";
    case GalleryRoute::ButtonDetail: return "button-detail";
    }
    return "overview";
}

[[nodiscard]] constexpr std::string_view galleryRouteTitle(GalleryRoute route) noexcept
{
    switch (route) {
    case GalleryRoute::Overview: return "Overview";
    case GalleryRoute::AllComponents: return "All components";
    case GalleryRoute::Controls: return "Controls";
    case GalleryRoute::LongText: return "Long text";
    case GalleryRoute::AddOns: return "Add-ons";
    case GalleryRoute::VisualQa: return "Visual QA";
    case GalleryRoute::About: return "About";
    case GalleryRoute::ButtonDetail: return "Button";
    }
    return "Overview";
}

} // namespace whatsui::gallery
