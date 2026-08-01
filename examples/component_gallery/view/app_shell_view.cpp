#include "view/app_shell_view.h"

#include <array>
#include <stdexcept>
#include <string>
#include <utility>

#include "view/components/navigation_rail.h"
#include "view/components/compact_navigation.h"
#include "view/components/responsive_gallery_shell.h"
#include "wui/theme.h"
#include "wui/declarative.h"

namespace whatsui::gallery::view {
namespace {

struct Destination {
    GalleryRoute route;
    const char* label;
    wui::IconName icon;
};

constexpr std::array<Destination, 7> kDestinations{{
    {GalleryRoute::Overview, "Overview", wui::IconName::TaskList},
    {GalleryRoute::AllComponents, "All components", wui::IconName::Square},
    {GalleryRoute::Controls, "Controls", wui::IconName::CheckmarkCircle},
    {GalleryRoute::LongText, "Long text", wui::IconName::TaskList},
    {GalleryRoute::AddOns, "Add-ons", wui::IconName::MoreHorizontal},
    {GalleryRoute::VisualQa, "Visual QA", wui::IconName::Checkmark},
    {GalleryRoute::About, "About", wui::IconName::Info},
}};

GalleryRoute navigationSelection(GalleryRoute route) noexcept
{
    return route == GalleryRoute::ButtonDetail ? GalleryRoute::AllComponents : route;
}

components::NavigationRailConfig railConfig(GalleryRoute selectedRoute)
{
    components::NavigationRailConfig config;
    config.selectedId = std::string(galleryRouteKey(navigationSelection(selectedRoute)));
    config.items.reserve(kDestinations.size());
    for (const auto& destination : kDestinations) {
        config.items.push_back({std::string(galleryRouteKey(destination.route)),
                                destination.label, destination.icon});
    }
    return config;
}

GalleryRoute routeForId(const std::string& id) noexcept
{
    for (const auto& destination : kDestinations) {
        if (id == galleryRouteKey(destination.route)) return destination.route;
    }
    return GalleryRoute::Overview;
}

} // namespace

std::unique_ptr<wui::Node> buildAppShell(wui::UiWindow& window,
                                         GalleryRoute selectedRoute,
                                         std::unique_ptr<wui::Node> pageContent,
                                         ShellNavigateHandler navigate)
{
    if (!pageContent) throw std::invalid_argument("App shell requires page content");
    const auto config = railConfig(selectedRoute);
    return std::make_unique<components::ResponsiveGalleryShell>(
        window, std::move(pageContent),
        [config, navigate = std::move(navigate), &window](bool compact) {
            if (compact) {
                return components::buildCompactNavigationBar(
                    config,
                    [navigate](GalleryRoute route) {
                        if (navigate) navigate(route);
                    },
                    window);
            }
            return components::buildNavigationRail(
                config,
                [navigate](const std::string& id) {
                    if (navigate) navigate(routeForId(id));
                });
        });
}

} // namespace whatsui::gallery::view
