#include "preview_surface.h"

#include <utility>

#include "responsive_layouts.h"
#include "wui/theme.h"
#include "wui/declarative.h"

namespace whatsui::gallery::view::components {
namespace {

std::unique_ptr<wui::Node> buildPreviewHeading(const PreviewSurfaceConfig& config)
{
    using namespace wui;
    const auto& colors = wui::theme().colors;
    // A status badge must never compete with the explanatory copy for a
    // 267-DIP viewport.  The reusable responsive row remains a normal Fluent
    // heading on desktop and stacks its semantic pieces in compact layouts.
    auto heading = std::make_unique<ResponsiveRow>();
    heading->gap(12.0f).align(wui::Alignment::Center);
    heading->appendChild(Column()
        .gap(2.0f)
        .children(
            Text(config.title).size(14.0f).weight(600).color(colors.text),
            Text(config.caption).size(11.0f).color(colors.textMuted))
        .build());
    heading->appendChild(Spacer().flex(1.0f).build());
    if (config.showStatus && !config.statusLabel.empty()) {
        heading->appendChild(Badge(config.statusLabel)
            .appearance(wui::BadgeAppearance::Tint)
            .color(wui::BadgeColor::Success)
            .size(wui::BadgeSize::Small)
            .build());
    }
    return heading;
}

std::unique_ptr<wui::Node> buildCanvas(
    float minHeight,
    std::unique_ptr<wui::Node> content)
{
    using namespace wui;
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
    return std::move(canvas).build();
}

} // namespace

std::unique_ptr<wui::Node> buildPreviewSurface(
    PreviewSurfaceConfig config,
    std::unique_ptr<wui::Node> content)
{
    using namespace wui;

    return Card()
        .appearance(wui::CardAppearance::Outline)
        .size(wui::CardSize::Medium)
        .children(
            buildPreviewHeading(config),
            buildCanvas(config.minHeight, std::move(content)))
        .build();
}

} // namespace whatsui::gallery::view::components
