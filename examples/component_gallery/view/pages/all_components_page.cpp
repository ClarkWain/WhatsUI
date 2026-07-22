#include "all_components_page.h"

#include <array>
#include <string>
#include <utility>

#include "view/components/component_card.h"
#include "view/components/page_header.h"
#include "wui/theme.h"
#include "wui/ui.h"

namespace whatsui::gallery::view::pages {
namespace {

wui::IconName iconFor(ComponentIcon icon)
{
    switch (icon) {
    case ComponentIcon::Input: return wui::IconName::Edit;
    case ComponentIcon::Feedback: return wui::IconName::Info;
    case ComponentIcon::Navigation: return wui::IconName::ChevronRight;
    case ComponentIcon::Data: return wui::IconName::TaskList;
    case ComponentIcon::Layout: return wui::IconName::Square;
    case ComponentIcon::Person: return wui::IconName::Circle;
    case ComponentIcon::Calendar: return wui::IconName::Calendar;
    case ComponentIcon::Overlay: return wui::IconName::MoreHorizontal;
    case ComponentIcon::Component: return wui::IconName::CheckmarkCircle;
    }
    return wui::IconName::Square;
}

std::unique_ptr<wui::Node> buildDescriptorPreview(const ComponentDescriptor& descriptor)
{
    using namespace wui::ui;
    if (descriptor.id == "textarea") {
        return TextArea("Write a short note...").rows(2).intoNode();
    }
    if (descriptor.id == "table") {
        return Table({{"name", "Name", 92.0f}, {"status", "Status", 92.0f}})
            .rows({{"one", {"Button", "Stable"}}, {"two", {"Avatar", "Stable"}}})
            .maxVisibleRows(2)
            .accessibleLabel("Component status preview")
            .intoNode();
    }
    if (descriptor.id == "avatar") {
        return Row().gap(8.0f).align(wui::Alignment::Center).children(
            Avatar("Ada Lovelace", wui::AvatarSize::Size40).initials("AL"),
            Text("Ada Lovelace").size(12.0f)).intoNode();
    }
    if (descriptor.id == "progress-bar") {
        return Column().gap(8.0f).align(wui::Alignment::Stretch).children(
            Text("Uploading · 68%").size(11.0f),
            ProgressBar(0.0f, 100.0f, 68.0f).accessibleLabel("Upload progress"))
            .intoNode();
    }
    return Row()
        .gap(10.0f)
        .align(wui::Alignment::Center)
        .children(
            Icon(iconFor(descriptor.icon)).size(wui::IconSize::Size20),
            Text(descriptor.name).size(12.0f).weight(600))
        .intoNode();
}

std::unique_ptr<wui::Node> buildComponent(
    const ComponentDescriptor& descriptor,
    const OpenComponentHandler& onOpen)
{
    view::components::ComponentCardConfig config;
    config.title = descriptor.name;
    config.description = descriptor.summary;
    config.category = std::string(componentCategoryName(descriptor.category));
    config.icon = iconFor(descriptor.icon);
    if (onOpen && descriptor.id == "button") {
        config.onOpen = [onOpen, id = descriptor.id] { onOpen(id); };
    }
    return view::components::buildComponentCard(
        std::move(config), buildDescriptorPreview(descriptor));
}

std::unique_ptr<wui::Node> buildFilters(GalleryViewModel& viewModel)
{
    using namespace wui::ui;
    constexpr std::array<ComponentCategory, 6> categories{{
        ComponentCategory::All,
        ComponentCategory::Controls,
        ComponentCategory::Inputs,
        ComponentCategory::Feedback,
        ComponentCategory::Navigation,
        ComponentCategory::DataDisplay,
    }};

    auto categoryRow = std::make_unique<wui::Row>();
    auto categoryButtons = std::make_shared<std::vector<std::pair<ComponentCategory, wui::Button*>>>();
    categoryRow->setGap(6.0f);
    categoryRow->setAlign(wui::Alignment::Center);
    for (const auto category : categories) {
        const bool selected = viewModel.selectedCategory().get() == category;
        auto button = std::make_unique<wui::Button>(std::string(componentCategoryName(category)));
        button->setAppearance(selected ? wui::ButtonAppearance::Primary : wui::ButtonAppearance::Subtle);
        auto* raw = button.get();
        categoryButtons->push_back({category, raw});
        button->onClick([&viewModel, category, categoryButtons] {
            viewModel.selectCategory(category);
            for (const auto& [value, item] : *categoryButtons) {
                item->setAppearance(value == category
                    ? wui::ButtonAppearance::Primary
                    : wui::ButtonAppearance::Subtle);
            }
        });
        categoryRow->appendChild(std::move(button));
    }

    return Column()
        .gap(10.0f)
        .align(wui::Alignment::Stretch)
        .children(
            SearchField("Search components")
                .query(viewModel.searchQuery().get())
                .onChange([&viewModel](const std::string& value) {
                    viewModel.setSearchQuery(value);
                }),
            std::move(categoryRow))
        .intoNode();
}

std::unique_ptr<wui::Node> buildResults(GalleryViewModel& viewModel, OpenComponentHandler onOpen)
{
    using namespace wui::ui;
    return Column()
        .gap(12.0f)
        .align(wui::Alignment::Stretch)
        .children(
            Text().bind(viewModel.resultCount(), [](std::size_t count) {
                return std::to_string(count) + (count == 1 ? " component" : " components");
            }).size(12.0f).color(wui::theme().colors.textMuted),
            KeyedForEach<ComponentDescriptor>(
                viewModel.visibleComponents(),
                [](const ComponentDescriptor& descriptor) { return descriptor.id; },
                [onOpen = std::move(onOpen)](const ComponentDescriptor& descriptor) {
                    return buildComponent(descriptor, onOpen);
                })
                .gap(12.0f)
                .align(wui::Alignment::Stretch))
        .intoNode();
}

} // namespace

std::unique_ptr<wui::Node> buildAllComponentsPage(
    GalleryViewModel& viewModel,
    OpenComponentHandler onOpenComponent)
{
    using namespace wui::ui;
    return ScrollView()
        .children(
            Column()
                .gap(20.0f)
                .padding({32.0f, 32.0f, 40.0f, 32.0f})
                .align(wui::Alignment::Stretch)
                .children(
                    view::components::buildPageHeader({"CATALOG", "All components", "Search and filter the complete Fluent component set.", {}}),
                    buildFilters(viewModel),
                    buildResults(viewModel, std::move(onOpenComponent))))
        .intoNode();
}

} // namespace whatsui::gallery::view::pages
