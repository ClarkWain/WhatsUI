#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "domain/component_catalog.h"
#include "domain/gallery_route.h"
#include "view_model/button_detail_view_model.h"
#include "view_model/gallery_view_model.h"
#include "view_model/navigation_view_model.h"
#include "view_model/visual_qa_view_model.h"

#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <crtdbg.h>
#include <windows.h>
#endif

namespace {

using namespace whatsui::gallery;

void expect(bool condition, std::string_view message)
{
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

ComponentDescriptor component(std::string id,
                              std::string name,
                              ComponentCategory category,
                              std::vector<std::string> keywords = {})
{
    return {std::move(id), std::move(name), "Test component", category,
            ComponentIcon::Component, ComponentMaturity::Stable, false,
            std::move(keywords)};
}

ComponentCatalog testCatalog()
{
    std::vector<ComponentDescriptor> components;
    components.push_back(component("button", "Button", ComponentCategory::Controls,
                                   {"action", "command"}));
    components.push_back(component("checkbox", "Checkbox", ComponentCategory::Controls,
                                   {"selection"}));
    components.push_back(component("text-input", "Text input", ComponentCategory::Inputs,
                                   {"field", "editor"}));
    components.push_back(component("toast", "Toast", ComponentCategory::Feedback,
                                   {"notification"}));
    components.push_back(component("breadcrumb", "Breadcrumb", ComponentCategory::Navigation,
                                   {"path"}));
    components.push_back(component("table", "Table", ComponentCategory::DataDisplay,
                                   {"grid"}));
    components.push_back(component("divider", "Divider", ComponentCategory::Layout,
                                   {"separator"}));
    components.push_back(component("avatar", "Avatar", ComponentCategory::Identity,
                                   {"person"}));
    components.push_back(component("calendar", "Calendar", ComponentCategory::DateTime,
                                   {"date"}));
    components.push_back(component("dialog", "Dialog", ComponentCategory::Overlays,
                                   {"modal"}));
    return ComponentCatalog(std::move(components));
}

void testRouteContractCoversAllSevenPages()
{
    constexpr GalleryRoute routes[] = {
        GalleryRoute::Overview,
        GalleryRoute::AllComponents,
        GalleryRoute::Controls,
        GalleryRoute::AddOns,
        GalleryRoute::VisualQa,
        GalleryRoute::About,
        GalleryRoute::ButtonDetail,
    };
    constexpr std::string_view keys[] = {
        "overview", "all-components", "controls", "add-ons",
        "visual-qa", "about", "button-detail",
    };

    for (std::size_t index = 0; index < std::size(routes); ++index) {
        expect(!galleryRouteTitle(routes[index]).empty(),
               "Every gallery route must expose a page title");
        expect(galleryRouteKey(routes[index]) == keys[index],
               "Gallery route keys must remain stable for Navigator integration");
    }
}

void testNavigationStateSelectsEveryRouteWithoutDuplicateNotifications()
{
    NavigationViewModel viewModel;
    expect(viewModel.currentRoute().get() == GalleryRoute::Overview,
           "Navigation must start on Overview");
    expect(viewModel.isSelected(GalleryRoute::Overview),
           "Overview must be selected initially");

    int notifications = 0;
    const auto subscription = viewModel.currentRoute().subscribe(
        [&](const GalleryRoute&) { ++notifications; });
    for (const auto route : {GalleryRoute::AllComponents, GalleryRoute::Controls,
                             GalleryRoute::AddOns, GalleryRoute::VisualQa,
                             GalleryRoute::About, GalleryRoute::ButtonDetail,
                             GalleryRoute::Overview}) {
        viewModel.select(route);
        expect(viewModel.currentRoute().get() == route && viewModel.isSelected(route),
               "Selecting a route must update the observable navigation state");
    }
    expect(notifications == 7,
           "Selecting seven distinct routes must notify exactly once per transition");

    viewModel.select(GalleryRoute::Overview);
    expect(notifications == 7,
           "Selecting the current route must not publish a redundant state change");
    viewModel.currentRoute().unsubscribe(subscription);
}

void testGalleryFiltersByCategoryNameAndKeyword()
{
    const auto catalog = testCatalog();
    GalleryViewModel viewModel(catalog);
    expect(viewModel.selectedCategory().get() == ComponentCategory::All,
           "Gallery must begin with the All category");
    expect(viewModel.searchQuery().get().empty(),
           "Gallery must begin with an empty search query");
    expect(viewModel.visibleComponents().get().size() == catalog.components().size()
               && viewModel.resultCount().get() == catalog.components().size(),
           "Gallery must expose the complete catalog before filtering");

    viewModel.selectCategory(ComponentCategory::Controls);
    expect(viewModel.resultCount().get() == 2,
           "Controls category must retain only control descriptors");
    for (const auto& descriptor : viewModel.visibleComponents().get()) {
        expect(descriptor.category == ComponentCategory::Controls,
               "Category filtering must not leak components from another category");
    }

    viewModel.setSearchQuery("check");
    expect(viewModel.resultCount().get() == 1
               && viewModel.visibleComponents().get().front().id == "checkbox",
           "Search and category filters must compose");

    viewModel.selectCategory(ComponentCategory::All);
    viewModel.setSearchQuery("");

    for (const auto category : {ComponentCategory::Layout,
                                ComponentCategory::Identity,
                                ComponentCategory::DateTime,
                                ComponentCategory::Overlays}) {
        viewModel.selectCategory(category);
        expect(viewModel.resultCount().get() == 1
                   && viewModel.visibleComponents().get().front().category == category,
               "Every catalog category must be selectable through the ViewModel");
    }

    viewModel.selectCategory(ComponentCategory::All);
    viewModel.setSearchQuery("NoTiFiCaTiOn");
    expect(viewModel.resultCount().get() == 1
               && viewModel.visibleComponents().get().front().id == "toast",
           "Search must be case-insensitive and include descriptor keywords");

    viewModel.setSearchQuery("Text input");
    expect(viewModel.resultCount().get() == 1
               && viewModel.visibleComponents().get().front().id == "text-input",
           "Search must match a component's display name");

    viewModel.setSearchQuery("no component matches this");
    expect(viewModel.visibleComponents().get().empty()
               && viewModel.resultCount().get() == 0,
           "An unmatched query must publish a stable empty result set");
}

void testGalleryFilterStateAndClearAreObservable()
{
    const auto catalog = testCatalog();
    GalleryViewModel viewModel(catalog);
    int queryNotifications = 0;
    int categoryNotifications = 0;
    int resultNotifications = 0;
    const auto querySubscription = viewModel.searchQuery().subscribe(
        [&](const std::string&) { ++queryNotifications; });
    const auto categorySubscription = viewModel.selectedCategory().subscribe(
        [&](const ComponentCategory&) { ++categoryNotifications; });
    const auto resultSubscription = viewModel.visibleComponents().subscribe(
        [&](const std::vector<ComponentDescriptor>&) { ++resultNotifications; });

    viewModel.setSearchQuery("button");
    viewModel.setSearchQuery("button");
    viewModel.selectCategory(ComponentCategory::Controls);
    viewModel.selectCategory(ComponentCategory::Controls);
    expect(queryNotifications == 1 && categoryNotifications == 1,
           "Equivalent filter assignments must not republish State values");
    expect(resultNotifications == 1,
           "A category change that preserves the visible result must not republish it");

    viewModel.clearFilters();
    expect(viewModel.searchQuery().get().empty()
               && viewModel.selectedCategory().get() == ComponentCategory::All,
           "Clear filters must reset both query and category state");
    expect(viewModel.resultCount().get() == catalog.components().size(),
           "Clear filters must restore the complete catalog");

    viewModel.searchQuery().unsubscribe(querySubscription);
    viewModel.selectedCategory().unsubscribe(categorySubscription);
    viewModel.visibleComponents().unsubscribe(resultSubscription);
}

void testDetailAndVisualQaStateRemainIndependent()
{
    ButtonDetailViewModel detail;
    expect(detail.appearance().get() == ButtonAppearanceSample::Primary
               && detail.size().get() == ButtonSizeSample::Medium
               && !detail.iconVisible().get() && detail.enabled().get(),
           "Button detail must expose reviewed defaults");
    detail.selectAppearance(ButtonAppearanceSample::Outline);
    detail.selectSize(ButtonSizeSample::Large);
    detail.setIconVisible(true);
    detail.setEnabled(false);
    expect(detail.appearance().get() == ButtonAppearanceSample::Outline
               && detail.size().get() == ButtonSizeSample::Large
               && detail.iconVisible().get() && !detail.enabled().get(),
           "Button detail controls must update independent observable state");

    VisualQaViewModel visualQa;
    expect(visualQa.actualScaleFactor().get() == 1.0f
               && visualQa.selectedDpi().get() == DpiProfile::System
               && visualQa.selectedTheme().get() == ThemePreview::Light
               && visualQa.selectedInteraction().get() == InteractionPreview::Rest,
           "Visual QA must expose deterministic defaults");
    visualQa.setActualScaleFactor(1.5f);
    visualQa.selectDpi(DpiProfile::Dpi150);
    visualQa.selectTheme(ThemePreview::Dark);
    visualQa.selectInteraction(InteractionPreview::Focused);
    expect(visualQa.actualScaleFactor().get() == 1.5f
               && visualQa.selectedDpi().get() == DpiProfile::Dpi150
               && visualQa.selectedTheme().get() == ThemePreview::Dark
               && visualQa.selectedInteraction().get() == InteractionPreview::Focused,
           "Visual QA selections must update without coupling unrelated state");
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
        testRouteContractCoversAllSevenPages();
        testNavigationStateSelectsEveryRouteWithoutDuplicateNotifications();
        testGalleryFiltersByCategoryNameAndKeyword();
        testGalleryFilterStateAndClearAreObservable();
        testDetailAndVisualQaStateRemainIndependent();
        std::puts("Component Gallery ViewModel tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Component Gallery ViewModel test failure: %s\n", error.what());
        return 1;
    }
}
