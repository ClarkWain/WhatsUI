#include "compact_navigation.h"

#include <utility>

#include "wui/drawer.h"
#include "wui/theme.h"
#include "wui/ui.h"

namespace whatsui::gallery::view::components {
namespace {

GalleryRoute routeForId(const std::string& id) noexcept
{
    for (const auto route : {GalleryRoute::Overview, GalleryRoute::AllComponents,
                             GalleryRoute::Controls, GalleryRoute::AddOns,
                             GalleryRoute::VisualQa, GalleryRoute::About}) {
        if (id == galleryRouteKey(route)) return route;
    }
    return GalleryRoute::Overview;
}

std::string selectedLabel(const NavigationRailConfig& config)
{
    for (const auto& item : config.items) {
        if (item.id == config.selectedId) return item.label;
    }
    return config.productName;
}

void showCompactNavigationDrawer(const NavigationRailConfig& config,
                                 CompactNavigationHandler onNavigate,
                                 wui::UiWindow& window)
{
    using namespace wui::ui;

    auto drawer = std::make_unique<wui::Drawer>("WhatsUI", "Component Gallery");
    drawer->type(wui::DrawerType::Overlay)
        .position(wui::DrawerPosition::Start)
        .width(280.0f)
        .modal(true)
        .dismissOnOutsidePress(true)
        .closeOnEscape(true);
    auto* const drawerNode = drawer.get();

    auto destinations = std::make_unique<wui::Column>();
    destinations->setGap(4.0f);
    destinations->setAlign(wui::Alignment::Stretch);
    for (const auto& item : config.items) {
        const bool selected = item.id == config.selectedId;
        const GalleryRoute route = routeForId(item.id);
        auto button = Button(item.label)
            .appearance(selected ? wui::ButtonAppearance::Primary
                                 : wui::ButtonAppearance::Subtle)
            .icon(item.icon)
            .iconPosition(wui::ButtonIconPosition::Before)
            .accessibilityId("gallery.navigation." + item.id)
            .onClick([onNavigate, drawerNode, route] {
                if (onNavigate) onNavigate(route);
                drawerNode->dismiss();
            });
        destinations->appendChild(std::move(button).intoNode());
    }
    drawer->content(std::move(destinations));
    (void)window.overlayHost().show(std::move(drawer));
}

} // namespace

std::unique_ptr<wui::Node> buildCompactNavigationBar(
    const NavigationRailConfig& config,
    CompactNavigationHandler onNavigate,
    wui::UiWindow& window)
{
    using namespace wui::ui;
    const auto& colors = wui::theme().colors;
    const auto selected = selectedLabel(config);

    return Box()
        .height(48.0f)
        .background(colors.surface)
        .padding({8.0f, 12.0f, 8.0f, 12.0f})
        .children(
            Row()
                .gap(8.0f)
                .align(wui::Alignment::Center)
                .children(
                    Button("Open navigation")
                        .appearance(wui::ButtonAppearance::Subtle)
                        .icon(wui::IconName::MoreHorizontal)
                        .iconOnly()
                        .accessibilityId("gallery.navigation.open")
                        .onClick([config, onNavigate = std::move(onNavigate), &window] {
                            showCompactNavigationDrawer(config, onNavigate, window);
                        }),
                    Text(selected)
                        .size(16.0f)
                        .weight(600)
                        .color(colors.text)))
        .intoNode();
}

} // namespace whatsui::gallery::view::components
