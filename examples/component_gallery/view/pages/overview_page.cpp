#include "overview_page.h"

#include <array>
#include <string>
#include <utility>

#include "view/components/component_card.h"
#include "view/components/page_header.h"
#include "view/components/responsive_overview_hero.h"
#include "view/components/responsive_column_pair.h"
#include "wui/theme.h"
#include "wui/ui.h"

using namespace wui::ui;

namespace whatsui::gallery::view::pages {
namespace {

struct CategoryFeature {
    ComponentCategory category;
    const char* title;
    const char* description;
    wui::IconName icon;
};

constexpr std::array<CategoryFeature, 4> kFeatures{{
    {ComponentCategory::Controls, "Controls", "Buttons, toggles, sliders, and selection controls.", wui::IconName::CheckmarkCircle},
    {ComponentCategory::Inputs, "Inputs", "Text, search, date, and structured input experiences.", wui::IconName::Edit},
    {ComponentCategory::Navigation, "Navigation", "Tabs, breadcrumbs, toolbars, and routed pages.", wui::IconName::ChevronRight},
    {ComponentCategory::Feedback, "Feedback", "Progress, status, messages, and transient feedback.", wui::IconName::Info},
}};

std::unique_ptr<wui::Node> buildHero(GalleryViewModel& gallery, const NavigateHandler& navigate)
{
    const auto& current = wui::theme();
    return Box()
        .background(current.colors.surface)
        .radius(current.radius.xLarge)
        .padding(32.0f)
        .children(
            Row()
            .gap(28.0f)
            .align(wui::Alignment::Center)
            .children(
                Column()
                .gap(14.0f)
                .align(wui::Alignment::Start)
                .children(
                    Badge("FLUENT 2 FOR C++")
                        .appearance(wui::BadgeAppearance::Tint)
                        .color(wui::BadgeColor::Brand),
                    Text("Build polished native interfaces faster.")
                        .size(32.0f)
                        .lineHeight(40.0f)
                        .weight(600)
                        .wrap()
                        .color(current.colors.text),
                    Text("Explore production-ready WhatsUI components, states, tokens, and developer tooling in one live gallery.")
                        .size(14.0f)
                        .lineHeight(22.0f)
                        .wrap()
                        .color(current.colors.textMuted),
                    Row()
                    .gap(8.0f)
                    .children(
                        Button("Browse all components")
                            .appearance(wui::ButtonAppearance::Primary)
                            .icon(wui::IconName::ChevronRight)
                            .iconPosition(wui::ButtonIconPosition::After)
                            .onClick([&gallery, navigate] {
                                gallery.clearFilters();
                                if (navigate) navigate(GalleryRoute::AllComponents);
                            }),
                        Button("Visual QA")
                            .appearance(wui::ButtonAppearance::Secondary)
                            .onClick([navigate] {
                                if (navigate) navigate(GalleryRoute::VisualQa);
                            })
                    )
                ),

                Box()
                .width(220.0f)
                .height(180.0f)
                .radius(current.radius.xxLarge)
                .background(current.colors.surfaceAlt)
                .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                .children(
                    Column()
                    .gap(10.0f)
                    .align(wui::Alignment::Center)
                    .children(
                        Icon(wui::IconName::TaskList)
                            .size(wui::IconSize::Size24)
                            .style(wui::IconStyle::Filled)
                            .color(current.colors.accent),
                        Text("Live components").size(13.0f).weight(600),
                        Badge("INTERACTIVE PREVIEW")
                            .appearance(wui::BadgeAppearance::Tint)
                            .color(wui::BadgeColor::Brand)
                    )
                )
            )
        )
        .intoNode();
}

std::unique_ptr<wui::Node> buildCategoryPreview(wui::IconName icon)
{
    return Row()
        .gap(10.0f)
        .align(wui::Alignment::Center)
        .children(
            Icon(icon).size(wui::IconSize::Size20).color(wui::theme().colors.accent),
            Text("Category preview").size(12.0f).weight(600)
        )
        .intoNode();
}

std::unique_ptr<wui::Node> buildCategories(GalleryViewModel& gallery, const NavigateHandler& navigate)
{
    auto rows = std::make_unique<wui::Column>();
    rows->setGap(12.0f);
    rows->setAlign(wui::Alignment::Stretch);

    for (std::size_t index = 0; index < kFeatures.size(); index += 2) {
        auto row = std::make_unique<view::components::ResponsiveColumnPair>();
        row->setGap(12.0f);
        row->setAlign(wui::Alignment::Stretch);
        for (std::size_t offset = 0; offset < 2; ++offset) {
            const auto feature = kFeatures[index + offset];
            view::components::ComponentCardConfig config;
            config.title = feature.title;
            config.description = feature.description;
            config.category = "Category";
            config.icon = feature.icon;
            config.onOpen = [&gallery, navigate, category = feature.category] {
                gallery.selectCategory(category);
                if (navigate) navigate(GalleryRoute::AllComponents);
            };
            auto card = view::components::buildComponentCard(
                std::move(config), buildCategoryPreview(feature.icon));
            card->setFlex(1.0f);
            row->appendChild(std::move(card));
        }
        rows->appendChild(std::move(row));
    }
    return rows;
}

std::unique_ptr<wui::Node> buildQaSummary(const ComponentCatalog& catalog, const NavigateHandler& navigate)
{
    const auto& current = wui::theme();
    const std::string componentCount = std::to_string(catalog.components().size());
    return Card()
        .appearance(wui::CardAppearance::FilledAlternative)
        .children(
            Row()
            .gap(20.0f)
            .align(wui::Alignment::Center)
            .children(
                Column()
                .gap(4.0f)
                .children(
                    Text("Visual quality sample").size(16.0f).weight(600),
                    Text(componentCount + " catalog entries · 4 review targets · 2 render backends")
                        .size(12.0f)
                        .color(current.colors.textMuted)
                ),
                Spacer().flex(1.0f),
                Badge("SAMPLE DATA")
                    .appearance(wui::BadgeAppearance::Tint)
                    .color(wui::BadgeColor::Warning),
                Button("Open QA")
                    .appearance(wui::ButtonAppearance::Subtle)
                    .onClick([navigate] { if (navigate) navigate(GalleryRoute::VisualQa); })
            )
        )
        .intoNode();
}

} // namespace

std::unique_ptr<wui::Node> buildOverviewPage(
    const ComponentCatalog& catalog,
    GalleryViewModel& gallery,
    NavigateHandler navigate)
{
    return ScrollView()
        .children(
            Column()
            .gap(24.0f)
            .padding({32.0f, 32.0f, 40.0f, 32.0f})
            .align(wui::Alignment::Stretch)
            .children(
                view::components::buildPageHeader({"GALLERY", "Overview", "A living catalog of WhatsUI components and quality evidence.", {}}),
                view::components::buildResponsiveOverviewHero(gallery, navigate),
                Text("Explore by category").size(20.0f).weight(600),
                buildCategories(gallery, navigate),
                buildQaSummary(catalog, navigate)
            )
        )
        .intoNode();
}

} // namespace whatsui::gallery::view::pages
