#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "domain/gallery_route.h"
#include "view/components/navigation_rail.h"
#include "view_model/navigation_view_model.h"
#include "wui/accessibility.h"
#include "wui/basic_controls.h"
#include "wui/runtime.h"

#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <crtdbg.h>
#include <windows.h>
#endif

namespace {

using namespace whatsui::gallery;
namespace gallery_components = whatsui::gallery::view::components;

void expect(bool condition, std::string_view message)
{
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

class RoutePage final : public wui::Node {
public:
    explicit RoutePage(GalleryRoute route)
        : route_(route)
    {
    }

    [[nodiscard]] GalleryRoute route() const noexcept { return route_; }

private:
    GalleryRoute route_;
};

class NavigationBinding final {
public:
    NavigationBinding(NavigationViewModel& viewModel, wui::Navigator& navigator)
        : viewModel_(viewModel)
        , navigator_(navigator)
    {
        setNavigatorRoute(viewModel_.currentRoute().get(), true);
        subscription_ = viewModel_.currentRoute().subscribe(
            [this](const GalleryRoute& route) { setNavigatorRoute(route, false); });
    }

    ~NavigationBinding()
    {
        viewModel_.currentRoute().unsubscribe(subscription_);
    }

    NavigationBinding(const NavigationBinding&) = delete;
    NavigationBinding& operator=(const NavigationBinding&) = delete;

private:
    void setNavigatorRoute(GalleryRoute route, bool root)
    {
        auto page = std::make_unique<RoutePage>(route);
        if (root) {
            navigator_.setRoot(std::string(galleryRouteKey(route)), std::move(page));
        } else {
            navigator_.replace(std::string(galleryRouteKey(route)), std::move(page));
        }
    }

    NavigationViewModel& viewModel_;
    wui::Navigator& navigator_;
    wui::State<GalleryRoute>::SubscriptionId subscription_{0};
};

constexpr GalleryRoute allRoutes[] = {
    GalleryRoute::Overview,
    GalleryRoute::AllComponents,
    GalleryRoute::Controls,
    GalleryRoute::AddOns,
    GalleryRoute::VisualQa,
    GalleryRoute::About,
    GalleryRoute::ButtonDetail,
};

GalleryRoute routeForKey(std::string_view key)
{
    for (const GalleryRoute route : allRoutes) {
        if (galleryRouteKey(route) == key) return route;
    }
    return GalleryRoute::Overview;
}

gallery_components::NavigationRailConfig railConfig(GalleryRoute selected)
{
    gallery_components::NavigationRailConfig config;
    config.selectedId = std::string(galleryRouteKey(selected));
    for (const GalleryRoute route : allRoutes) {
        config.items.push_back({std::string(galleryRouteKey(route)),
                                std::string(galleryRouteTitle(route)),
                                wui::IconName::Circle});
    }
    return config;
}

void collectButtons(wui::Node& node, std::vector<wui::Button*>& buttons)
{
    if (auto* button = dynamic_cast<wui::Button*>(&node)) {
        buttons.push_back(button);
    }
    if (auto* container = dynamic_cast<wui::ContainerNode*>(&node)) {
        for (const auto& child : container->children()) {
            collectButtons(*child, buttons);
        }
    }
}

void expectNavigatorMatches(const NavigationViewModel& viewModel,
                            const wui::Navigator& navigator)
{
    expect(navigator.size() == 1 && !navigator.canPop(),
           "Top-level gallery selection must replace rather than grow the Navigator stack");
    expect(navigator.currentKey() != nullptr
               && *navigator.currentKey() == galleryRouteKey(viewModel.currentRoute().get()),
           "Navigator key and NavigationViewModel route must stay synchronized");
    const auto* page = dynamic_cast<const RoutePage*>(navigator.current());
    expect(page != nullptr && page->route() == viewModel.currentRoute().get(),
           "Navigator content must represent the same route as observable state");
}

void testAllRouteSelectionsStaySynchronizedWithNavigator()
{
    NavigationViewModel viewModel;
    wui::Navigator navigator;
    NavigationBinding binding(viewModel, navigator);
    expectNavigatorMatches(viewModel, navigator);

    for (const GalleryRoute route : allRoutes) {
        viewModel.select(route);
        expect(viewModel.isSelected(route),
               "Selected route must be visible through NavigationViewModel");
        expectNavigatorMatches(viewModel, navigator);
    }
}

void testNavigationRailInvokesSevenAccessibleDestinations()
{
    NavigationViewModel viewModel;
    wui::Navigator navigator;
    NavigationBinding binding(viewModel, navigator);
    int selections = 0;
    auto rail = gallery_components::buildNavigationRail(
        railConfig(viewModel.currentRoute().get()),
        [&](const std::string& key) {
            ++selections;
            viewModel.select(routeForKey(key));
        });

    std::vector<wui::Button*> buttons;
    collectButtons(*rail, buttons);
    expect(buttons.size() == std::size(allRoutes),
           "Navigation rail must retain one real Button per gallery route");

    for (std::size_t index = 0; index < buttons.size(); ++index) {
        expect(buttons[index]->label() == galleryRouteTitle(allRoutes[index]),
               "Navigation Button labels must follow the stable route order");
        expect(buttons[index]->performAccessibilityAction(
                   wui::AccessibilityActionKind::Invoke, {})
                   == wui::AccessibilityActionStatus::Succeeded,
               "Every gallery destination must support accessible invocation");
        expect(viewModel.currentRoute().get() == allRoutes[index],
               "Invoking a navigation Button must select its route");
        expectNavigatorMatches(viewModel, navigator);
    }
    expect(selections == 7,
           "Each route Button must invoke exactly one selection callback");
}

void testSelectedRailStateTracksViewModelRebuild()
{
    NavigationViewModel viewModel;
    for (const GalleryRoute route : allRoutes) {
        viewModel.select(route);
        auto rail = gallery_components::buildNavigationRail(
            railConfig(viewModel.currentRoute().get()));
        std::vector<wui::Button*> buttons;
        collectButtons(*rail, buttons);
        std::size_t primaryButtons = 0;
        for (std::size_t index = 0; index < buttons.size(); ++index) {
            const bool selected = buttons[index]->appearance()
                                  == wui::ButtonAppearance::Primary;
            primaryButtons += selected ? 1U : 0U;
            expect(selected == (allRoutes[index] == route),
                   "Only the current route may use the selected rail appearance");
        }
        expect(primaryButtons == 1,
               "A rebuilt rail must expose exactly one selected destination");
    }
}

void testUnknownRailKeyDoesNotCorruptNavigationState()
{
    NavigationViewModel viewModel;
    wui::Navigator navigator;
    NavigationBinding binding(viewModel, navigator);
    const GalleryRoute before = viewModel.currentRoute().get();

    auto config = railConfig(before);
    config.items = {{"unknown-route", "Unknown", wui::IconName::Circle}};
    auto rail = gallery_components::buildNavigationRail(
        std::move(config), [&](const std::string& key) {
            if (key != "unknown-route") viewModel.select(routeForKey(key));
        });
    std::vector<wui::Button*> buttons;
    collectButtons(*rail, buttons);
    expect(buttons.size() == 1,
           "Unknown-route fixture must contain one invokable Button");
    expect(buttons.front()->performAccessibilityAction(
               wui::AccessibilityActionKind::Invoke, {})
               == wui::AccessibilityActionStatus::Succeeded,
           "Rail callbacks may safely reject an unknown application route");
    expect(viewModel.currentRoute().get() == before,
           "An unknown route id must not corrupt ViewModel navigation state");
    expectNavigatorMatches(viewModel, navigator);
}

void disableSystemFailureDialogs()
{
#ifdef _MSC_VER
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX
                 | SEM_NOOPENFILEERRORBOX);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
}

} // namespace

int main()
{
    disableSystemFailureDialogs();
    try {
        testAllRouteSelectionsStaySynchronizedWithNavigator();
        testNavigationRailInvokesSevenAccessibleDestinations();
        testSelectedRailStateTracksViewModelRebuild();
        testUnknownRailKeyDoesNotCorruptNavigationState();
        std::puts("Component Gallery navigation tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Component Gallery navigation test failure: %s\n", error.what());
        return 1;
    }
}
