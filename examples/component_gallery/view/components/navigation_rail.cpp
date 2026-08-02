#include "navigation_rail.h"

#include <cstdint>
#include <utility>

#include "wui/theme.h"
#include "wui/declarative.h"
#include "wui/widgets.h"

namespace whatsui::gallery::view::components {
namespace {

std::unique_ptr<wui::Node> buildBrand(const NavigationRailConfig& config)
{
    using namespace wui;
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
                .color(colors.onAccent)
            ),
            Column()
            .gap(1.0f)
            .children(
                Text(config.productName).size(15.0f).weight(600).color(colors.text),
                Text(config.productCaption).size(11.0f).color(colors.textMuted)
            )
        )
        .build();
}

// Sidebar rows are Box surfaces with an InteractionArea attached: Column
// stretches them to the rail's full width, .onClick handles keyboard/mouse
// activation and a11y invoke, and hoverBackground/pressedBackground drive the
// full-width hover strip that Fluent NavigationView shows on real Windows.
std::unique_ptr<wui::Node> buildRailItem(
    NavigationRailItem item,
    bool selected,
    const NavigationRailSelectHandler& onSelect)
{
    using namespace wui;
    const auto& colors = wui::theme().colors;

    const auto withAlpha = [](wui::Color color, std::uint8_t alpha) noexcept {
        color.a = alpha;
        return color;
    };

    const wui::Color hoverFill = selected ? withAlpha(colors.accent, 220)
                                          : withAlpha(colors.text, 20);
    const wui::Color pressedFill = selected ? withAlpha(colors.accent, 200)
                                            : withAlpha(colors.text, 32);
    const wui::Color textColor = selected ? colors.onAccent : colors.text;
    const wui::Color background = selected ? colors.accent
                                           : wui::Color{0, 0, 0, 0};

    auto box = Box()
        .height(40.0f)
        .radius(6.0f)
        .background(background)
        .hoverBackground(hoverFill)
        .pressedBackground(pressedFill)
        // InsetsF order is {left, top, right, bottom}. 12 DIP on each side
        // gives icons the same leading gutter Fluent NavigationView ships
        // with; vertical space is handled by the fixed 40 DIP row height.
        .padding(wui::InsetsF{12.0f, 0.0f, 12.0f, 0.0f})
        .contentAlign(wui::Alignment::Start, wui::Alignment::Center)
        .accessibleRole(wui::AccessibilityRole::Button)
        .accessibleLabel(item.label)
        .children(
            Row()
                .gap(12.0f)
                .align(wui::Alignment::Center)
                .children(
                    Icon(item.icon)
                        .size(wui::IconSize::Size20)
                        .style(selected ? wui::IconStyle::Filled
                                        : wui::IconStyle::Regular)
                        .color(textColor),
                    Text(item.label)
                        .size(14.0f)
                        .weight(selected ? 600 : 500)
                        .color(textColor)));

    if (onSelect) {
        const std::string id = std::move(item.id);
        box = std::move(box).onClick([onSelect, id] { onSelect(id); });
    }
    return std::move(box).build();
}

std::unique_ptr<wui::Node> buildItems(
    const NavigationRailConfig& config,
    const NavigationRailSelectHandler& onSelect)
{
    auto items = std::make_unique<wui::ColumnNode>();
    items->setGap(4.0f);
    items->setAlign(wui::Alignment::Stretch);

    for (const auto& item : config.items) {
        items->appendChild(buildRailItem(item, item.id == config.selectedId, onSelect));
    }
    return items;
}

} // namespace

std::unique_ptr<wui::Node> buildNavigationRail(
    NavigationRailConfig config,
    NavigationRailSelectHandler onSelect)
{
    using namespace wui;
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
        .build();
}

} // namespace whatsui::gallery::view::components
