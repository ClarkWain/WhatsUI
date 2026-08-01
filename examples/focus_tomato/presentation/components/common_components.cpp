#include "common_components.h"

#include "../focus_style.h"
#include "wui/declarative.h"

namespace whatsui::focus_tomato::presentation {

std::unique_ptr<wui::Node> buildFixedImage(
    const wui::ImageSource& source,
    float width,
    float height,
    std::string alt,
    bool circular,
    bool decorative)
{
    auto image = std::make_unique<wui::ImageNode>(source);
    image->setFit(wui::ImageFit::Cover);
    image->setBlock(true);
    image->setShape(circular ? wui::ImageShape::Circular
                             : wui::ImageShape::Square);
    image->setAlt(std::move(alt));
    image->setDecorative(decorative);
    return wui::Box()
        .width(width)
        .height(height)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .children(std::move(image))
        .build();
}

std::unique_ptr<wui::Node> buildWindowBar(
    float width, std::string title, const FocusAssets& assets)
{
    using namespace wui;
    return Box()
        .background(style::surface)
        .width(width)
        .height(56.0f)
        .padding({18.0f, 0.0f, 18.0f, 0.0f})
        .children(
            Row()
                .align(wui::Alignment::Center)
                .gap(10.0f)
                .children(
                    buildFixedImage(assets.brandTomato, 18.0f, 18.0f,
                                    "FocusTomato", true),
                    Text(std::move(title))
                        .style(style::text(12.0f, 500, 18.0f))
                        .color(style::textPrimary),
                    Spacer().flex(1.0f),
                    Text("—    □    ×")
                        .style(style::text(13.0f, 400, 18.0f))
                        .color(style::textSecondary)
                )
        )
        .build();
}

std::unique_ptr<wui::Node> buildPill(
    std::string label, bool selected, std::function<void()> onClick)
{
    using namespace wui;
    const std::string accessibleLabel = "筛选：" + label;
    auto pill = Box()
        .background(selected ? style::accent : style::surface)
        .radius(999.0f)
        .padding({14.0f, 8.0f, 14.0f, 8.0f})
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .children(
            Text(std::move(label))
                .style(style::text(12.0f, 500, 17.0f))
                .color(selected ? style::surface : style::textSecondary)
        );
    if (onClick) {
        return std::move(pill)
            .hoverBackground(selected ? style::accent : wui::Color{250, 246, 240, 255})
            .pressedBackground(selected ? wui::Color{220, 76, 65, 255}
                                        : style::border)
            .accessibleRole(wui::AccessibilityRole::Button)
            .accessibleLabel(accessibleLabel)
            .onClick(std::move(onClick))
            .build();
    }
    return std::move(pill).build();
}

std::unique_ptr<wui::Node> buildPrimaryTextButton(
    std::string label, std::function<void()> onClick)
{
    using namespace wui;
    const std::string accessibleLabel = label;
    return Box()
        .background(style::actionPrimary)
        .hoverBackground(style::actionPrimaryHover)
        .pressedBackground(style::actionPrimaryPressed)
        .radius(999.0f)
        .padding({16.0f, 8.0f, 16.0f, 8.0f})
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .accessibleRole(wui::AccessibilityRole::Button)
        .accessibleLabel(accessibleLabel)
        .onClick(std::move(onClick))
        .children(
            Text(std::move(label))
                .style(style::text(13.0f, 500, 20.0f))
                .color(style::surface)
        )
        .build();
}

std::unique_ptr<wui::Node> buildSecondaryTextButton(
    std::string label, std::function<void()> onClick)
{
    using namespace wui;
    const std::string accessibleLabel = label;
    return Box()
        .background(style::surface)
        .hoverBackground(wui::Color{250, 246, 240, 255})
        .pressedBackground(style::border)
        .radius(999.0f)
        .padding({16.0f, 8.0f, 16.0f, 8.0f})
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .accessibleRole(wui::AccessibilityRole::Button)
        .accessibleLabel(accessibleLabel)
        .onClick(std::move(onClick))
        .children(
            Text(std::move(label))
                .style(style::text(13.0f, 500, 20.0f))
                .color(style::textPrimary)
        )
        .build();
}

std::unique_ptr<wui::Node> buildMetricCard(
    float width,
    std::string label,
    std::string value,
    std::string unit)
{
    using namespace wui;
    return Box()
        .background(style::surface)
        .radius(16.0f)
        .width(width)
        .height(120.0f)
        .padding(18.0f)
        .children(
            Column()
                .gap(8.0f)
                .align(wui::Alignment::Start)
                .children(
                    Text(std::move(label))
                        .style(style::text(12.0f, 400, 17.0f))
                        .color(style::textSecondary),
                    Text(std::move(value))
                        .style(style::text(28.0f, 700, 34.0f))
                        .color(style::accent),
                    Text(std::move(unit))
                        .style(style::text(11.0f, 400, 16.0f))
                        .color(style::textMuted)
                )
        )
        .build();
}

std::unique_ptr<wui::Node> buildIconControl(
    const wui::ImageSource& icon,
    float controlSize,
    float iconSize,
    bool primary,
    std::string accessibleLabel,
    std::function<void()> onClick)
{
    using namespace wui;
    return Box()
        .background(primary ? style::actionPrimary : style::surface)
        .hoverBackground(primary ? style::actionPrimaryHover
                                 : wui::Color{250, 246, 240, 255})
        .pressedBackground(primary ? style::actionPrimaryPressed : style::border)
        .radius(999.0f)
        .width(controlSize)
        .height(controlSize)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .accessibleRole(wui::AccessibilityRole::Button)
        .accessibleLabel(std::move(accessibleLabel))
        .onClick(std::move(onClick))
        .children(buildFixedImage(icon, iconSize, iconSize, {}, false, true))
        .build();
}

std::unique_ptr<wui::Node> buildGlyphControl(
    std::string glyph,
    float controlSize,
    float glyphSize,
    bool primary,
    std::string accessibleLabel,
    std::function<void()> onClick)
{
    using namespace wui;
    return Box()
        .background(primary ? style::actionPrimary : style::surface)
        .hoverBackground(primary ? style::actionPrimaryHover
                                 : wui::Color{250, 246, 240, 255})
        .pressedBackground(primary ? style::actionPrimaryPressed : style::border)
        .radius(999.0f)
        .width(controlSize)
        .height(controlSize)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .accessibleRole(wui::AccessibilityRole::Button)
        .accessibleLabel(std::move(accessibleLabel))
        .onClick(std::move(onClick))
        .children(
            Text(std::move(glyph))
                .style(style::text(glyphSize, 700, glyphSize))
                .color(primary ? style::surface : style::accent)
        )
        .build();
}

} // namespace whatsui::focus_tomato::presentation
