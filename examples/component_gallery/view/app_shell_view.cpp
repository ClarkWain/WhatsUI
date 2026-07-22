#include "view/app_shell_view.h"

#include <array>
#include <stdexcept>
#include <string>
#include <utility>

#include "view/components/navigation_rail.h"
#include "wui/theme.h"
#include "wui/ui.h"

namespace whatsui::gallery::view {
namespace {

struct Destination {
    GalleryRoute route;
    const char* label;
    wui::IconName icon;
};

constexpr std::array<Destination, 6> kDestinations{{
    {GalleryRoute::Overview, "Overview", wui::IconName::TaskList},
    {GalleryRoute::AllComponents, "All components", wui::IconName::Square},
    {GalleryRoute::Controls, "Controls", wui::IconName::CheckmarkCircle},
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

std::unique_ptr<wui::Node> buildAppShell(GalleryRoute selectedRoute,
                                         std::unique_ptr<wui::Node> pageContent,
                                         ShellNavigateHandler navigate)
{
    if (!pageContent) throw std::invalid_argument("App shell requires page content");
    pageContent->setFlex(1.0f);
    auto rail = components::buildNavigationRail(
        railConfig(selectedRoute),
        [navigate = std::move(navigate)](const std::string& id) {
            if (navigate) navigate(routeForId(id));
        });

    using namespace wui::ui;
    return Box()
        .background(wui::theme().colors.background)
        .children(Row()
                      .align(wui::Alignment::Stretch)
                      .children(std::move(rail), std::move(pageContent)))
        .intoNode();
}

} // namespace whatsui::gallery::view
