#include "common_components.h"

#include "../focus_style.h"
#include "../focus_view_model.h"
#include "wui/declarative.h"

namespace whatsui::focus_tomato::presentation {
namespace {

wui::Box buildCaptionControl(
    std::string glyph,
    std::string accessibleLabel,
    std::function<void()> onClick,
    bool destructive = false)
{
    using namespace wui;
    return Box()
        .background(style::surface)
        .hoverBackground(
            destructive ? style::actionPrimary
                        : wui::Color{250, 246, 240, 255})
        .pressedBackground(
            destructive ? style::actionPrimaryPressed : style::border)
        .width(46.0f)
        .height(kFocusWindowBarHeight)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .accessibleRole(wui::AccessibilityRole::Button)
        .accessibleLabel(std::move(accessibleLabel))
        .onClick(std::move(onClick))
        .children(
            Text(std::move(glyph))
                .style(style::text(13.0f, 500, 18.0f))
                .color(style::textSecondary)
        );
}

} // namespace

wui::Box buildWindowBar(
    float width,
    std::string title,
    const FocusAssets& assets,
    WindowBarActions actions,
    bool allowMaximize)
{
    using namespace wui;
    View maximizeControl = allowMaximize
        ? View(buildCaptionControl(
                "□", "最大化或还原窗口",
                std::move(actions.toggleMaximized)))
        : View(Box().width(0.0f).height(kFocusWindowBarHeight));
    return Box()
        .background(style::surface)
        .width(width)
        .height(kFocusWindowBarHeight)
        .children(
            Row()
                .align(wui::Alignment::Center)
                .children(
                    Box()
                        .padding({16.0f, 0.0f, 10.0f, 0.0f})
                        .height(kFocusWindowBarHeight)
                        .contentAlign(
                            wui::Alignment::Center,
                            wui::Alignment::Center)
                        .children(
                            buildFixedImage(
                                assets.brandTomato,
                                18.0f,
                                18.0f,
                                "FocusTomato",
                                true)
                        ),
                    Text(std::move(title))
                        .style(style::text(12.0f, 500, 18.0f))
                        .color(style::textPrimary),
                    Spacer().flex(1.0f),
                    buildCaptionControl(
                        "—", "最小化窗口",
                        std::move(actions.minimize)),
                    std::move(maximizeControl),
                    buildCaptionControl(
                        "×", "关闭窗口",
                        std::move(actions.close), true)
                )
        );
}

wui::Box buildFixedImage(
    const wui::ImageSource& source,
    float width,
    float height,
    std::string alt,
    bool circular,
    bool decorative)
{
    using namespace wui;
    return wui::Box()
        .width(width)
        .height(height)
        .contentAlign(wui::Alignment::Center, wui::Alignment::Center)
        .children(
            Image(source)
                .fit(wui::ImageFit::Cover)
                .block()
                .shape(circular ? wui::ImageShape::Circular
                                : wui::ImageShape::Square)
                .alt(std::move(alt))
                .decorative(decorative)
        );
}

wui::Box buildPill(
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
            .onClick(std::move(onClick));
    }
    return std::move(pill);
}

wui::Box buildPrimaryTextButton(
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
        );
}

wui::Box buildSecondaryTextButton(
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
        );
}

wui::Box buildPageNavigationAction(
    float width,
    std::string label,
    std::string automationId,
    std::function<void()> onClick)
{
    using namespace wui;
    const std::string accessibleLabel = label;
    return Box()
        .width(width)
        .height(52.0f)
        .padding({28.0f, 10.0f, 28.0f, 6.0f})
        .children(
            Row()
                .align(wui::Alignment::Center)
                .children(
                    Box()
                        .automationId(std::move(automationId))
                        .background(style::surface)
                        .hoverBackground(wui::Color{250, 246, 240, 255})
                        .pressedBackground(style::border)
                        .radius(999.0f)
                        .padding({12.0f, 7.0f, 12.0f, 7.0f})
                        .accessibleRole(wui::AccessibilityRole::Button)
                        .accessibleLabel(accessibleLabel)
                        .onClick(std::move(onClick))
                        .children(
                            Text(std::move(label))
                                .style(style::text(12.0f, 500, 18.0f))
                                .color(style::textSecondary)
                        ),
                    Spacer().flex(1.0f)
                )
        );
}

wui::Box buildMetricCard(
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
        );
}

wui::If buildOperationBanner(FocusViewModel& viewModel, float width)
{
    using namespace wui;
    State<std::string> message = viewModel.operationMessage();
    return If(viewModel.hasOperationMessage()).then(
        [message, width]() mutable {
            return Box()
                .background(wui::Color{255, 235, 232, 255})
                .radius(12.0f)
                .width(width)
                .padding({14.0f, 10.0f, 14.0f, 10.0f})
                .accessibleRole(wui::AccessibilityRole::Alert)
                .children(
                    Text()
                        .bind(message)
                        .style(style::text(12.0f, 500, 18.0f))
                        .color(style::actionPrimary)
                );
        });
}

wui::Box buildIconControl(
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
        .children(buildFixedImage(icon, iconSize, iconSize, {}, false, true));
}

wui::Box buildGlyphControl(
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
        );
}

} // namespace whatsui::focus_tomato::presentation
