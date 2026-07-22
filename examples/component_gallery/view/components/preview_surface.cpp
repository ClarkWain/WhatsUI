#include "preview_surface.h"

#include <utility>

#include "wui/theme.h"
#include "wui/ui.h"

namespace whatsui::gallery::view::components {
namespace {

std::unique_ptr<wui::Node> buildPreviewHeading(const PreviewSurfaceConfig& config)
{
    using namespace wui::ui;
    const auto& colors = wui::theme().colors;
    auto heading = std::make_unique<wui::Row>();
    heading->setGap(12.0f);
    heading->setAlign(wui::Alignment::Center);
    heading->appendChild(Column()
        .gap(2.0f)
        .children(
            Text(config.title).size(14.0f).weight(600).color(colors.text),
            Text(config.caption).size(11.0f).color(colors.textMuted))
        .intoNode());
    heading->appendChild(Spacer().flex(1.0f).intoNode());
    if (config.showStatus && !config.statusLabel.empty()) {
        heading->appendChild(Badge(config.statusLabel)
            .appearance(wui::BadgeAppearance::Tint)
            .color(wui::BadgeColor::Success)
            .size(wui::BadgeSize::Small)
            .intoNode());
    }
    return heading;
}

std::unique_ptr<wui::Node> buildCanvas(
    float minHeight,
    std::unique_ptr<wui::Node> content)
{
    using namespace wui::ui;
    const auto& current = wui::theme();
    auto canvas = Box()
        .height(minHeight)
        .background(current.colors.surfaceAlt)
        .radius(current.radius.lg)
        .padding(24.0f)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center);
    if (content) {
        canvas = std::move(canvas).children(std::move(content));
    }
    return std::move(canvas).intoNode();
}

} // namespace

std::unique_ptr<wui::Node> buildPreviewSurface(
    PreviewSurfaceConfig config,
    std::unique_ptr<wui::Node> content)
{
    using namespace wui::ui;

    return Card()
        .appearance(wui::CardAppearance::Outline)
        .size(wui::CardSize::Medium)
        .children(
            buildPreviewHeading(config),
            buildCanvas(config.minHeight, std::move(content)))
        .intoNode();
}

} // namespace whatsui::gallery::view::components
