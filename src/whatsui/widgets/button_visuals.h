#pragma once

#include <optional>
#include <string>

#include "wui/icons.h"
#include "wui/theme.h"
#include "wui/widgets.h"

namespace wui::button_visuals {

struct ButtonVisual {
    Color background{};
    Color foreground{};
    Color border{};
    bool hasBorder{false};
    float borderWidth{1.0f};
};

[[nodiscard]] float buttonHeight(const Theme& current,
                                 ButtonSize size) noexcept;
[[nodiscard]] float buttonRadius(const Theme& current, ButtonShape shape,
                                 float height) noexcept;
[[nodiscard]] TextStyleToken buttonTextStyle(const Theme& current,
                                             ButtonSize size) noexcept;
[[nodiscard]] float buttonHorizontalPadding(const Theme& current,
                                            ButtonSize size) noexcept;
[[nodiscard]] float measuredButtonTextWidth(
    const std::string& label, const TextStyleToken& style);

[[nodiscard]] ButtonVisual resolveButtonVisual(
    const Theme& current, ButtonAppearance appearance, bool disabled,
    bool selected, ControlVisualStates states) noexcept;

// Paints focus, fill, and stroke through the single Fluent Button state
// resolver. Action-family controls use the returned foreground for all label
// and icon content so their combined states cannot drift apart.
[[nodiscard]] ButtonVisual paintButtonSurface(
    PaintContext& context, const RectF& bounds, const Theme& current,
    ButtonAppearance appearance, bool disabled, bool selected,
    ControlVisualStates states,
    ButtonShape shape = ButtonShape::Rounded);

void drawButtonContent(PaintContext& context, const RectF& bounds,
                       const std::string& label,
                       const std::optional<IconName>& icon,
                       IconStyle iconStyle,
                       ButtonIconPosition iconPosition, bool iconOnly,
                       ButtonSize size, Color foreground,
                       const Theme& current);

} // namespace wui::button_visuals
