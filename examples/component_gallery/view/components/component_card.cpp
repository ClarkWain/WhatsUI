#include "component_card.h"

#include <utility>

#include "wui/theme.h"
#include "wui/declarative.h"

namespace whatsui::gallery::view::components {
namespace {

std::unique_ptr<wui::Node> buildHeaderMedia(wui::IconName icon)
{
    using namespace wui;
    const auto& colors = wui::theme().colors;
    return Box()
        .width(32.0f)
        .height(32.0f)
        .radius(6.0f)
        .background(colors.surfaceAlt)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .children(Icon(icon).size(wui::IconSize::Size20).color(colors.accent))
        .build();
}

std::unique_ptr<wui::Node> buildCategoryBadge(std::string category)
{
    using namespace wui;
    if (category.empty()) {
        return nullptr;
    }
    return Badge(std::move(category))
        .appearance(wui::BadgeAppearance::Tint)
        .color(wui::BadgeColor::Brand)
        .size(wui::BadgeSize::Small)
        .build();
}

std::unique_ptr<wui::Node> buildPreview(
    float height,
    std::unique_ptr<wui::Node> content)
{
    using namespace wui;
    const auto& current = wui::theme();
    auto surface = Box()
        .background(current.colors.surfaceAlt)
        .radius(current.radius.md)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center);
    if (content) {
        surface = std::move(surface).children(std::move(content));
    }
    return CardPreview()
        .height(height)
        .children(std::move(surface))
        .build();
}

std::unique_ptr<wui::Node> buildFooter(ComponentCardConfig config)
{
    using namespace wui;
    if (!config.onOpen) {
        return CardFooter()
            .children(Text("Preview only - detail coming")
                .size(11.0f)
                .color(wui::theme().colors.textMuted))
            .build();
    }
    return CardFooter()
        .children(
            Row()
                .align(wui::Alignment::Center)
                .children(
                    Spacer().flex(1.0f),
                    Button(std::move(config.actionLabel))
                        .appearance(wui::ButtonAppearance::Subtle)
                        .icon(wui::IconName::ChevronRight)
                        .iconPosition(wui::ButtonIconPosition::After)
                        .onClick(std::move(config.onOpen))))
        .build();
}

} // namespace

std::unique_ptr<wui::Node> buildComponentCard(
    ComponentCardConfig config,
    std::unique_ptr<wui::Node> preview)
{
    using namespace wui;
    const float previewHeight = config.previewHeight;
    auto header = CardHeader(config.title, config.description)
        .media(buildHeaderMedia(config.icon));
    auto badge = buildCategoryBadge(config.category);
    if (badge) {
        header = std::move(header).action(std::move(badge));
    }

    return Card()
        .appearance(wui::CardAppearance::Outline)
        .size(wui::CardSize::Medium)
        .children(
            std::move(header),
            buildPreview(previewHeight, std::move(preview)),
            buildFooter(std::move(config)))
        .build();
}

} // namespace whatsui::gallery::view::components
