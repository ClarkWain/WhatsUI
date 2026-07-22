#include "about_page.h"

#include <array>
#include <string>
#include <utility>

#include "view/components/page_header.h"
#include "wui/theme.h"
#include "wui/ui.h"

namespace whatsui::gallery::view::pages {
namespace {

std::unique_ptr<wui::Node> buildIdentityCard()
{
    using namespace wui::ui;
    auto mark = Box()
        .width(64.0f)
        .height(64.0f)
        .radius(14.0f)
        .background(wui::theme().colors.accent)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .children(Icon(wui::IconName::TaskList)
            .size(wui::IconSize::Size24)
            .style(wui::IconStyle::Filled)
            .color(wui::theme().colors.onAccent));
    auto details = Column()
        .gap(5.0f)
        .children(
            Text("WhatsUI").size(24.0f).weight(600),
            Text("A retained-mode C++ UI toolkit with Fluent components and deterministic rendering.")
                .size(13.0f).lineHeight(20.0f).wrap().color(wui::theme().colors.textMuted),
            Row().gap(6.0f).children(
                Badge("0.1.0").appearance(wui::BadgeAppearance::Tint).color(wui::BadgeColor::Brand),
                Badge("C++17").appearance(wui::BadgeAppearance::Outline),
                Badge("Fluent 2").appearance(wui::BadgeAppearance::Outline)));
    return Card()
        .appearance(wui::CardAppearance::FilledAlternative)
        .children(
            Row()
                .gap(18.0f)
                .align(wui::Alignment::Center)
                .children(std::move(mark), std::move(details)))
        .intoNode();
}

std::unique_ptr<wui::Node> buildPrinciples()
{
    using namespace wui::ui;
    const std::array<std::pair<const char*, const char*>, 3> principles{{
        {"Layered", "Application, ViewModel, View, and framework concerns stay explicit."},
        {"Observable", "State and computed values keep UI behavior testable and predictable."},
        {"Verifiable", "Software snapshots and multi-DPI reviews make visual quality measurable."},
    }};
    auto row = std::make_unique<wui::Row>();
    row->setGap(12.0f);
    row->setAlign(wui::Alignment::Stretch);
    for (const auto& [title, description] : principles) {
        auto card = Card()
            .appearance(wui::CardAppearance::Outline)
            .children(
                Column().gap(8.0f).children(
                    Icon(wui::IconName::CheckmarkCircle).color(wui::theme().colors.accent),
                    Text(title).size(15.0f).weight(600),
                    Text(description).size(11.0f).lineHeight(17.0f).wrap().color(wui::theme().colors.textMuted)))
            .intoNode();
        card->setFlex(1.0f);
        row->appendChild(std::move(card));
    }
    return row;
}

std::unique_ptr<wui::Node> buildResources(const OpenLinkHandler& openLink)
{
    using namespace wui::ui;
    auto link = [&openLink](std::string label, std::string href) {
        auto item = Link(std::move(label)).href(href);
        if (openLink) {
            item = std::move(item).onClick([openLink, href = std::move(href)] { openLink(href); });
        }
        return std::move(item).intoNode();
    };
    return Card()
        .appearance(wui::CardAppearance::Outline)
        .children(
            Column()
                .gap(10.0f)
                .align(wui::Alignment::Start)
                .children(
                    Text("Resources").size(16.0f).weight(600),
                    Text("Link activation is delegated to the host application so platform policy stays outside the View.")
                        .size(11.0f).wrap().color(wui::theme().colors.textMuted),
                    link("Project README", "https://github.com/Team-Bass/WhatsUI"),
                    link("Architecture guide", "https://github.com/Team-Bass/WhatsUI/blob/main/WHATSUI_ARCHITECTURE.md"),
                    link("License", "https://github.com/Team-Bass/WhatsUI/blob/main/LICENSE")))
        .intoNode();
}

} // namespace

std::unique_ptr<wui::Node> buildAboutPage(OpenLinkHandler openLink)
{
    using namespace wui::ui;
    return ScrollView()
        .children(
            Column()
                .gap(20.0f)
                .padding({32.0f, 32.0f, 40.0f, 32.0f})
                .align(wui::Alignment::Stretch)
                .children(
                    view::components::buildPageHeader({"PROJECT", "About", "The ideas and engineering values behind this component gallery.", {}}),
                    buildIdentityCard(),
                    Text("Design principles").size(20.0f).weight(600),
                    buildPrinciples(),
                    buildResources(openLink)))
        .intoNode();
}

} // namespace whatsui::gallery::view::pages
