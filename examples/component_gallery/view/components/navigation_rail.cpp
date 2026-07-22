#include "navigation_rail.h"

#include <utility>

#include "wui/theme.h"
#include "wui/ui.h"
#include "wui/widgets.h"

namespace whatsui::gallery::view::components {
namespace {

std::unique_ptr<wui::Node> buildBrand(const NavigationRailConfig& config)
{
    using namespace wui::ui;
    const auto& colors = wui::theme().colors;

    return Row()
        .gap(12.0f)
        .align(wui::Alignment::Center)
        .children(
            Box()
                .width(36.0f)
                .height(36.0f)
                .radius(8.0f)
                .background(colors.accent)
                .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
                .children(Icon(wui::IconName::TaskList)
                    .size(wui::IconSize::Size20)
                    .style(wui::IconStyle::Filled)
                    .color(colors.onAccent)),
            Column()
                .gap(1.0f)
                .children(
                    Text(config.productName).size(15.0f).weight(600).color(colors.text),
                    Text(config.productCaption).size(11.0f).color(colors.textMuted)))
        .intoNode();
}

std::unique_ptr<wui::Node> buildRailButton(
    NavigationRailItem item,
    bool selected,
    const NavigationRailSelectHandler& onSelect)
{
    using namespace wui::ui;

    auto button = Button(std::move(item.label))
        .appearance(selected ? wui::ButtonAppearance::Primary : wui::ButtonAppearance::Subtle)
        .icon(item.icon)
        .iconPosition(wui::ButtonIconPosition::Before);

    if (onSelect) {
        const std::string id = std::move(item.id);
        button = std::move(button).onClick([onSelect, id] { onSelect(id); });
    }
    return std::move(button).intoNode();
}

std::unique_ptr<wui::Node> buildItems(
    const NavigationRailConfig& config,
    const NavigationRailSelectHandler& onSelect)
{
    auto items = std::make_unique<wui::Column>();
    items->setGap(4.0f);
    items->setAlign(wui::Alignment::Stretch);

    for (const auto& item : config.items) {
        items->appendChild(buildRailButton(item, item.id == config.selectedId, onSelect));
    }
    return items;
}

} // namespace

std::unique_ptr<wui::Node> buildNavigationRail(
    NavigationRailConfig config,
    NavigationRailSelectHandler onSelect)
{
    using namespace wui::ui;
    const auto& current = wui::theme();

    return Box()
        .width(config.width)
        .background(current.colors.surface)
        .padding({20.0f, 20.0f, 16.0f, 20.0f})
        .children(
            Column()
                .gap(24.0f)
                .align(wui::Alignment::Stretch)
                .children(
                    buildBrand(config),
                    buildItems(config, onSelect),
                    Spacer().flex(1.0f),
                    Text("Built with WhatsUI")
                        .size(10.0f)
                        .color(current.colors.textMuted)))
        .intoNode();
}

} // namespace whatsui::gallery::view::components
