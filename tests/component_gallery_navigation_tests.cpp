#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "application/gallery_router.h"
#include "domain/component_catalog.h"
#include "domain/gallery_route.h"
#include "view/app_shell_view.h"
#include "view/components/navigation_rail.h"
#include "view/components/page_header.h"
#include "view/pages/all_components_page.h"
#include "view_model/gallery_view_model.h"
#include "view_model/navigation_view_model.h"
#include "wui/accessibility.h"
#include "wui/basic_controls.h"
#include "wui/runtime.h"
#include "wui/widgets.h"

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
    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        return constraints.clamp({1.0f, 1.0f});
    }
    void paint(wui::PaintContext&) override {}

private:
    GalleryRoute route_;
};

class ResponsivePageProbe final : public wui::Node {
public:
    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        return constraints.clamp({1.0f, 1.0f});
    }

    void paint(wui::PaintContext&) override {}
};

class FakeSurface final : public wui::RenderSurface {
public:
    [[nodiscard]] wui::CanvasBackend backend() const noexcept override
    {
        return wui::CanvasBackend::Software;
    }
    [[nodiscard]] wui::SizeF framebufferSize() const noexcept override
    {
        return {640.0f, 360.0f};
    }
    void beginFrame() override {}
    void endFrame() override {}
    void resize(wui::SizeF) override {}
};

class FakeClipboard final : public wui::Clipboard {
public:
    void setText(std::string_view text) override { text_ = text; }
    [[nodiscard]] std::string getText() const override { return text_; }
    [[nodiscard]] bool hasText() const override { return !text_.empty(); }

private:
    std::string text_;
};

class FakeCursor final : public wui::CursorService {
public:
    void setCursor(wui::CursorIcon) override {}
};

class FakeTextInput final : public wui::TextInputSession {
public:
    void activate() override {}
    void deactivate() override {}
    void setCaretRect(const wui::RectF&) override {}
    void setSurroundingText(std::string_view, std::size_t, std::size_t) override {}
};

class FakeWindow final : public wui::PlatformWindow {
public:
    explicit FakeWindow(wui::SizeF logicalSize = {320.0f, 180.0f}, float scale = 2.0f)
        : metrics_{{logicalSize.width, logicalSize.height},
                   {logicalSize.width * scale, logicalSize.height * scale}, scale}
    {
    }

    [[nodiscard]] wui::WindowId id() const noexcept override { return 1; }
    [[nodiscard]] wui::WindowMetrics metrics() const noexcept override
    {
        return metrics_;
    }
    void setLogicalSize(wui::SizeF logicalSize) noexcept
    {
        metrics_.logicalSize = logicalSize;
        metrics_.framebufferSize = {logicalSize.width * metrics_.scaleFactor,
                                    logicalSize.height * metrics_.scaleFactor};
    }
    void show() override { open_ = true; }
    void close() override { open_ = false; }
    [[nodiscard]] bool isOpen() const noexcept override { return open_; }
    [[nodiscard]] bool isFocused() const noexcept override { return true; }
    void setTitle(std::string_view title) override { title_ = title; }
    [[nodiscard]] std::string title() const override { return title_; }
    void requestRedraw() override { ++redraws; }
    [[nodiscard]] wui::RenderSurface& surface() override { return surface_; }
    [[nodiscard]] wui::Clipboard& clipboard() override { return clipboard_; }
    [[nodiscard]] wui::CursorService& cursor() override { return cursor_; }
    [[nodiscard]] wui::TextInputSession& textInput() override { return textInput_; }

    int redraws{0};

private:
    bool open_{true};
    std::string title_;
    wui::WindowMetrics metrics_;
    FakeSurface surface_;
    FakeClipboard clipboard_;
    FakeCursor cursor_;
    FakeTextInput textInput_;
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

void collectToggleButtons(wui::Node& node, std::vector<wui::ToggleButton*>& buttons)
{
    if (auto* button = dynamic_cast<wui::ToggleButton*>(&node)) {
        buttons.push_back(button);
    }
    if (auto* container = dynamic_cast<wui::ContainerNode*>(&node)) {
        for (const auto& child : container->children()) {
            collectToggleButtons(*child, buttons);
        }
    }
}

const wui::AccessibilitySnapshotEntry* findAccessibleEntry(
    const wui::AccessibilitySnapshot& snapshot,
    wui::AccessibilityRole role,
    std::string_view label)
{
    for (const auto& entry : snapshot) {
        if (entry.properties.role == role && entry.properties.label == label) {
            return &entry;
        }
    }
    return nullptr;
}

const wui::AccessibilitySnapshotEntry* findAccessibleEntryById(
    const wui::AccessibilitySnapshot& snapshot,
    std::string_view automationId)
{
    for (const auto& entry : snapshot) {
        if (entry.properties.automationId == automationId) return &entry;
    }
    return nullptr;
}

wui::AccessibilityActionRequest invokeRequest(
    const wui::AccessibilitySnapshotEntry& entry)
{
    wui::AccessibilityActionRequest request;
    request.kind = wui::AccessibilityActionKind::Invoke;
    request.path = entry.path;
    request.expectedRole = entry.properties.role;
    request.automationId = entry.properties.automationId;
    request.expectedLabel = entry.properties.label;
    return request;
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

void expectRouterState(const GalleryRouter& router,
                       const NavigationViewModel& viewModel,
                       const wui::UiWindow& window,
                       GalleryRoute route,
                       std::size_t stackSize)
{
    expect(router.currentRoute() == route && viewModel.currentRoute().get() == route,
           "GalleryRouter and NavigationViewModel must expose one active route");
    expect(window.navigator().size() == stackSize
               && window.navigator().currentKey() != nullptr
               && *window.navigator().currentKey() == galleryRouteKey(route),
           "GalleryRouter route must match the active Navigator entry");
    expect(window.root() == window.navigator().current() && window.root() != nullptr,
           "GalleryRouter must mount the active Navigator page into UiWindow");
    const auto* page = dynamic_cast<const RoutePage*>(window.root());
    expect(page != nullptr && page->route() == route,
           "Mounted route page must represent the observable route");
}

void testProductionRouterOwnsNavigationStateAndHistory()
{
    NavigationViewModel viewModel;
    auto platform = std::make_unique<FakeWindow>();
    auto* platformRaw = platform.get();
    wui::UiWindow window(std::move(platform));
    int factoryCalls = 0;
    GalleryRouter router(
        window, viewModel,
        [&](GalleryRoute route, GalleryRouter&) -> std::unique_ptr<wui::Node> {
            ++factoryCalls;
            expect(viewModel.currentRoute().get() == route,
                   "Router must publish route state before its lazy page factory runs");
            return std::make_unique<RoutePage>(route);
        });

    router.start(GalleryRoute::Controls);
    expectRouterState(router, viewModel, window, GalleryRoute::Controls, 1);
    expect(factoryCalls == 1 && platformRaw->redraws > 0,
           "Starting GalleryRouter must build and invalidate one root page");

    ComponentDescriptor button;
    button.id = "button";
    router.openComponent(button);
    expectRouterState(router, viewModel, window, GalleryRoute::ButtonDetail, 2);
    expect(router.canGoBack() && factoryCalls == 2,
           "Opening Button must push one detail destination");

    router.refresh();
    expectRouterState(router, viewModel, window, GalleryRoute::ButtonDetail, 2);
    expect(factoryCalls == 3 && router.canGoBack(),
           "Refreshing must rebuild the active page without losing history");

    router.goBack();
    expectRouterState(router, viewModel, window, GalleryRoute::Controls, 1);
    expect(!router.canGoBack() && factoryCalls == 4,
           "Going back must lazily rebuild and restore the originating top-level route");

    router.navigate(GalleryRoute::About);
    expectRouterState(router, viewModel, window, GalleryRoute::About, 1);
    expect(!router.canGoBack() && factoryCalls == 5,
           "Top-level navigation must replace history rather than append it");

    router.shutdown();
    expect(window.navigator().empty() && window.root() == nullptr
               && viewModel.currentRoute().get() == GalleryRoute::Overview,
           "Router shutdown must clear Navigator ownership and reset navigation state");
}

void testPageHeadingProjectsAccessibleSemantics()
{
    auto header = gallery_components::buildPageHeader(
        {"CATALOG", "All components", "Browse the catalog.", {}});
    const auto snapshot = wui::snapshotAccessibilityTree(*header);
    const auto* heading = findAccessibleEntry(
        snapshot, wui::AccessibilityRole::Heading, "All components");
    expect(heading != nullptr,
           "Gallery page title must project a Heading accessibility role");
}

void testCategoryTogglesExposeCheckedAndMutuallyExclusiveAccessibility()
{
    ComponentCatalog catalog;
    GalleryViewModel viewModel(catalog);
    auto page = whatsui::gallery::view::pages::buildAllComponentsPage(viewModel, {});
    std::vector<wui::ToggleButton*> toggles;
    collectToggleButtons(*page, toggles);
    expect(toggles.size() == 10,
           "All Components must expose one ToggleButton per visible category");

    auto findToggle = [&](std::string_view label) -> wui::ToggleButton* {
        for (auto* toggle : toggles) {
            if (toggle->label() == label) return toggle;
        }
        return nullptr;
    };
    auto* all = findToggle("All");
    auto* controls = findToggle("Controls");
    expect(findToggle("Layout") != nullptr
               && findToggle("Identity") != nullptr
               && findToggle("Date and time") != nullptr
               && findToggle("Overlays") != nullptr,
           "All Components must expose filters for every catalog category");
    expect(all != nullptr && controls != nullptr && all->isChecked()
               && !controls->isChecked(),
           "Category toggles must begin with exactly All selected");

    auto snapshot = wui::snapshotAccessibilityTree(*page);
    const auto* allEntry = findAccessibleEntry(
        snapshot, wui::AccessibilityRole::Button, "All");
    const auto* controlsEntry = findAccessibleEntry(
        snapshot, wui::AccessibilityRole::Button, "Controls");
    expect(allEntry != nullptr && allEntry->properties.checked == true
               && allEntry->properties.actions.toggle
               && controlsEntry != nullptr && controlsEntry->properties.checked == false
               && controlsEntry->properties.actions.toggle,
           "Category ToggleButtons must expose checked state and Toggle actions");

    expect(controls->performAccessibilityAction(
               wui::AccessibilityActionKind::Toggle, {})
               == wui::AccessibilityActionStatus::Succeeded,
           "An unselected category must be selectable through accessibility");
    expect(viewModel.selectedCategory().get() == ComponentCategory::Controls
               && controls->isChecked() && !all->isChecked(),
           "Selecting a category must update ViewModel and mutually exclusive Toggle state");

    expect(controls->performAccessibilityAction(
               wui::AccessibilityActionKind::Toggle, {})
               == wui::AccessibilityActionStatus::Succeeded,
           "The selected category must safely handle a repeated Toggle action");
    expect(viewModel.selectedCategory().get() == ComponentCategory::Controls
               && controls->isChecked(),
           "A repeated Toggle action must not leave the category group unselected");
}

void testResponsiveShellKeepsNarrowContentUsableAndNavigationReachable()
{
    auto platform = std::make_unique<FakeWindow>(wui::SizeF{680.0f, 520.0f});
    wui::UiWindow window(std::move(platform));
    auto page = std::make_unique<ResponsivePageProbe>();
    auto* const pageProbe = page.get();
    GalleryRoute selectedRoute = GalleryRoute::Overview;
    window.setRoot(whatsui::gallery::view::buildAppShell(
        window, GalleryRoute::Overview, std::move(page),
        [&selectedRoute](GalleryRoute route) { selectedRoute = route; }));
    window.update();
    window.layout();

    expect(pageProbe->bounds().x == 0.0f && pageProbe->bounds().y == 48.0f
               && pageProbe->bounds().width == 680.0f
               && pageProbe->bounds().height == 472.0f,
           "A sub-720 DIP Gallery must reserve a compact top bar, not consume content width with a rail");

    auto snapshot = window.accessibilitySnapshot();
    const auto* const openNavigation = findAccessibleEntryById(
        snapshot, "gallery.navigation.open");
    expect(openNavigation != nullptr
               && openNavigation->properties.role == wui::AccessibilityRole::Button
               && openNavigation->properties.label == "Open navigation"
               && openNavigation->properties.actions.invoke,
           "The compact Gallery shell must expose an invokable Open navigation control");
    expect(window.performAccessibilityAction(invokeRequest(*openNavigation))
               == wui::AccessibilityActionStatus::Succeeded,
           "Open navigation must work through the window accessibility boundary");

    window.update();
    window.layout();
    snapshot = window.accessibilitySnapshot();
    constexpr std::array<std::string_view, 6> compactDestinationIds{
        "gallery.navigation.overview", "gallery.navigation.all-components",
        "gallery.navigation.controls", "gallery.navigation.add-ons",
        "gallery.navigation.visual-qa", "gallery.navigation.about"};
    for (const auto id : compactDestinationIds) {
        const auto* const destination = findAccessibleEntryById(snapshot, id);
        expect(destination != nullptr
                   && destination->properties.role == wui::AccessibilityRole::Button
                   && destination->properties.actions.invoke,
               "Every Gallery route must remain reachable through the compact navigation Drawer");
    }

    const auto* const controls = findAccessibleEntryById(
        snapshot, "gallery.navigation.controls");
    expect(controls != nullptr
               && window.performAccessibilityAction(invokeRequest(*controls))
                      == wui::AccessibilityActionStatus::Succeeded
               && selectedRoute == GalleryRoute::Controls,
           "A compact navigation destination must invoke the same Gallery route callback as the rail");
}

void testResponsiveShellRetainsDesktopRailAtAndAboveBreakpoint()
{
    auto platform = std::make_unique<FakeWindow>(wui::SizeF{720.0f, 520.0f});
    wui::UiWindow window(std::move(platform));
    auto page = std::make_unique<ResponsivePageProbe>();
    auto* const pageProbe = page.get();
    GalleryRoute selectedRoute = GalleryRoute::Overview;
    window.setRoot(whatsui::gallery::view::buildAppShell(
        window, GalleryRoute::Overview, std::move(page),
        [&selectedRoute](GalleryRoute route) { selectedRoute = route; }));
    window.update();
    window.layout();

    expect(pageProbe->bounds().x == 232.0f && pageProbe->bounds().y == 0.0f
               && pageProbe->bounds().width == 488.0f
               && pageProbe->bounds().height == 520.0f,
           "A 720 DIP Gallery must retain the desktop rail and allocate the remaining width to content");

    const auto snapshot = window.accessibilitySnapshot();
    expect(findAccessibleEntryById(snapshot, "gallery.navigation.open") == nullptr,
           "Desktop Gallery navigation must not render the compact Drawer trigger");
    for (const GalleryRoute route : {GalleryRoute::Overview, GalleryRoute::AllComponents,
                                     GalleryRoute::Controls, GalleryRoute::AddOns,
                                     GalleryRoute::VisualQa, GalleryRoute::About}) {
        const auto* const destination = findAccessibleEntry(
            snapshot, wui::AccessibilityRole::Button, galleryRouteTitle(route));
        expect(destination != nullptr && destination->properties.actions.invoke,
               "Desktop Gallery rail must retain every directly invokable route");
    }
    const auto* const about = findAccessibleEntry(
        snapshot, wui::AccessibilityRole::Button, "About");
    expect(about != nullptr
               && window.performAccessibilityAction(invokeRequest(*about))
                      == wui::AccessibilityActionStatus::Succeeded
               && selectedRoute == GalleryRoute::About,
           "Desktop rail destinations must keep their original route callbacks");
}

void testResponsiveShellSwitchesModesWhenOneWindowCrossesBreakpoint()
{
    auto platform = std::make_unique<FakeWindow>(wui::SizeF{720.0f, 520.0f});
    auto* const platformProbe = platform.get();
    wui::UiWindow window(std::move(platform));
    auto page = std::make_unique<ResponsivePageProbe>();
    auto* const pageProbe = page.get();
    window.setRoot(whatsui::gallery::view::buildAppShell(
        window, GalleryRoute::Overview, std::move(page)));
    window.update();
    window.layout();
    expect(pageProbe->bounds().x == 232.0f && pageProbe->bounds().width == 488.0f,
           "The resize fixture must begin in desktop rail mode at 720 DIP");

    platformProbe->setLogicalSize({680.0f, 520.0f});
    window.layout();
    expect(pageProbe->bounds().x == 0.0f && pageProbe->bounds().y == 48.0f
               && pageProbe->bounds().width == 680.0f,
           "Resizing below 720 DIP must replace the rail with compact navigation in the same UiWindow");
    expect(findAccessibleEntryById(window.accessibilitySnapshot(), "gallery.navigation.open")
               != nullptr,
           "The compact navigation trigger must appear after a live narrow resize");

    platformProbe->setLogicalSize({720.0f, 520.0f});
    window.layout();
    expect(pageProbe->bounds().x == 232.0f && pageProbe->bounds().y == 0.0f
               && pageProbe->bounds().width == 488.0f,
           "Resizing back to 720 DIP must restore the desktop rail without rebuilding the page");
    expect(findAccessibleEntryById(window.accessibilitySnapshot(), "gallery.navigation.open")
               == nullptr,
           "The compact navigation trigger must not remain in the desktop accessibility tree after resize");
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
        testProductionRouterOwnsNavigationStateAndHistory();
        testPageHeadingProjectsAccessibleSemantics();
        testCategoryTogglesExposeCheckedAndMutuallyExclusiveAccessibility();
        testResponsiveShellKeepsNarrowContentUsableAndNavigationReachable();
        testResponsiveShellRetainsDesktopRailAtAndAboveBreakpoint();
        testResponsiveShellSwitchesModesWhenOneWindowCrossesBreakpoint();
        std::puts("Component Gallery navigation tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Component Gallery navigation test failure: %s\n", error.what());
        return 1;
    }
}
