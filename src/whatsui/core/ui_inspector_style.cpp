#include "ui_inspector_style.h"

#include "wui/basic_controls.h"
#include "wui/theme.h"
#include "wui/widgets.h"

namespace wui::detail {
namespace {

UiInspectorEntry::ResolvedStyle makeStyle(
    const std::string& role,
    bool enabled)
{
    UiInspectorEntry::ResolvedStyle style;
    style.role = role;
    style.enabled = enabled;
    return style;
}

struct VisualStateSnapshot {
    bool enabled{true};
    bool hovered{false};
    bool pressed{false};
};

VisualStateSnapshot visualStateFor(const Node& node) noexcept
{
    const auto states = [&node] {
        if (const auto* control = dynamic_cast<const ControlNode*>(&node)) {
            return control->visualStates();
        }
        return ControlVisualStates{toMask(ControlVisualState::None)};
    }();

    return {
        (states & toMask(ControlVisualState::Disabled)) == 0,
        (states & toMask(ControlVisualState::Hovered)) != 0,
        (states & toMask(ControlVisualState::Pressed)) != 0,
    };
}

UiInspectorEntry::ResolvedStyle resolveButtonStyle(
    const ButtonNode& button,
    const Theme& current,
    const VisualStateSnapshot& state)
{
    auto style = makeStyle("Button", state.enabled);
    ColorTokens::Interaction ramp = current.colors.neutralBackground1;
    Color background = ramp.rest;
    Color foreground = current.colors.neutralForeground1;
    bool hasBorder = true;

    switch (button.appearance()) {
    case ButtonAppearance::Primary:
        ramp = current.colors.brandBackground;
        background = ramp.rest;
        foreground = current.colors.onBrand;
        hasBorder = false;
        break;
    case ButtonAppearance::Danger:
        ramp = current.colors.dangerBackground;
        background = ramp.rest;
        foreground = current.colors.onBrand;
        hasBorder = false;
        break;
    case ButtonAppearance::Subtle:
    case ButtonAppearance::Transparent:
        background = {0, 0, 0, 0};
        hasBorder = false;
        break;
    case ButtonAppearance::Outline:
    case ButtonAppearance::Secondary:
    default:
        break;
    }

    if (hasBorder) style.border = current.colors.neutralStroke1;
    if (!state.enabled) {
        background = current.colors.neutralBackground1.rest;
        foreground = current.colors.neutralForegroundDisabled;
        style.border.reset();
    } else if (state.pressed) {
        background = ramp.pressed;
    } else if (state.hovered) {
        background = ramp.hover;
    }

    style.foreground = foreground;
    style.background = background;
    style.cornerRadius = current.radius.medium;
    style.controlExtent = current.controls.height;
    return style;
}

UiInspectorEntry::ResolvedStyle resolveCheckboxStyle(
    const CheckboxNode& checkbox,
    const Theme& current,
    const VisualStateSnapshot& state)
{
    auto style = makeStyle("Checkbox", state.enabled);
    const bool checked = checkbox.state() == CheckboxState::Checked;
    const bool mixed = checkbox.state() == CheckboxState::Mixed;
    Color box{0, 0, 0, 0};
    Color border = current.colors.neutralStrokeAccessible;
    Color foreground = current.colors.neutralForeground3;

    if (checked) {
        box = current.colors.compoundBrandBackground.rest;
        border = box;
        foreground = current.colors.neutralForeground1;
    } else if (mixed) {
        border = current.colors.compoundBrandStroke.rest;
        foreground = current.colors.neutralForeground1;
    }

    if (!state.enabled) {
        box = Color{0, 0, 0, 0};
        border = current.colors.neutralStrokeDisabled;
        foreground = current.colors.neutralForegroundDisabled;
    } else if (state.pressed) {
        if (checked) {
            box = border = current.colors.compoundBrandBackground.pressed;
        } else if (mixed) {
            border = current.colors.compoundBrandStroke.pressed;
        } else {
            border = current.colors.neutralStrokeAccessiblePressed;
        }
        if (!checked && !mixed) {
            foreground = current.colors.neutralForeground1;
        }
    } else if (state.hovered) {
        if (checked) {
            box = border = current.colors.compoundBrandBackground.hover;
        } else if (mixed) {
            border = current.colors.compoundBrandStroke.hover;
        } else {
            border = current.colors.neutralStrokeAccessibleHover;
        }
        if (!checked && !mixed) {
            foreground = current.colors.neutralForeground2;
        }
    }

    style.background = box;
    style.border = border;
    style.foreground = foreground;
    style.cornerRadius = current.radius.small;
    style.controlExtent = current.controls.checkboxSize;
    return style;
}

UiInspectorEntry::ResolvedStyle resolveRadioStyle(
    const RadioNode& radio,
    const Theme& current,
    const VisualStateSnapshot& state)
{
    auto style = makeStyle("Radio", state.enabled);
    const bool selected = radio.isSelected();
    Color border = selected
        ? current.colors.compoundBrandStroke.rest
        : current.colors.neutralStrokeAccessible;
    Color foreground = current.colors.neutralForeground3;

    if (!state.enabled) {
        border = current.colors.neutralStrokeDisabled;
        foreground = current.colors.neutralForegroundDisabled;
    } else if (state.pressed) {
        border = selected
            ? current.colors.compoundBrandStroke.pressed
            : current.colors.neutralStrokeAccessiblePressed;
        foreground = current.colors.neutralForeground1;
    } else if (state.hovered) {
        border = selected
            ? current.colors.compoundBrandStroke.hover
            : current.colors.neutralStrokeAccessibleHover;
        foreground = current.colors.neutralForeground2;
    } else if (selected) {
        foreground = current.colors.neutralForeground1;
    }

    // RadioNode leaves its centre and annular gap transparent.
    style.background = Color{0, 0, 0, 0};
    style.border = border;
    style.foreground = foreground;
    style.cornerRadius = current.radius.circular;
    style.controlExtent = current.controls.checkboxSize;
    return style;
}

UiInspectorEntry::ResolvedStyle resolveSwitchStyle(
    const SwitchNode& toggle,
    const Theme& current,
    const VisualStateSnapshot& state)
{
    auto style = makeStyle("Switch", state.enabled);
    const ColorTokens::Interaction& ramp = toggle.isOn()
        ? current.colors.brandBackground
        : current.colors.neutralBackground1;
    Color fill = ramp.rest;
    Color border = toggle.isOn()
        ? current.colors.brandBackground.rest
        : current.colors.neutralStrokeAccessible;

    if (!state.enabled) {
        fill = current.colors.neutralBackground1.rest;
        border = current.colors.neutralStroke1;
    } else if (state.pressed) {
        fill = ramp.pressed;
    } else if (state.hovered) {
        fill = ramp.hover;
    }

    style.background = fill;
    style.border = border;
    style.foreground = !state.enabled
        ? current.colors.neutralForegroundDisabled
        : toggle.isOn()
            ? current.colors.onBrand
            : current.colors.neutralForeground3;
    style.cornerRadius = current.radius.circular;
    style.controlExtent = current.controls.compactHeight;
    return style;
}

UiInspectorEntry::ResolvedStyle resolveSliderStyle(
    const Theme& current,
    const VisualStateSnapshot& state)
{
    auto style = makeStyle("Slider", state.enabled);
    style.background = !state.enabled
        ? current.colors.neutralForegroundDisabled
        : state.pressed
            ? current.colors.brandBackground.pressed
            : state.hovered
                ? current.colors.brandBackground.hover
                : current.colors.brandBackground.rest;
    style.border = current.colors.neutralStroke1;
    style.cornerRadius = current.radius.circular;
    style.controlExtent = current.controls.height;
    return style;
}

UiInspectorEntry::ResolvedStyle resolveProgressStyle(const Theme& current)
{
    auto style = makeStyle("ProgressBar", true);
    style.background = current.colors.brandBackground.rest;
    style.border = current.colors.neutralStroke1;
    style.cornerRadius = current.radius.circular;
    style.controlExtent = 4.0f;
    return style;
}

UiInspectorEntry::ResolvedStyle resolveDividerStyle(
    const DividerNode& divider,
    const Theme& current)
{
    auto style = makeStyle("Divider", true);
    style.background = current.colors.neutralStroke1;
    style.controlExtent = divider.thickness();
    return style;
}

} // namespace

std::optional<UiInspectorEntry::ResolvedStyle> resolveInspectorStyle(
    const Node& node)
{
    const Theme& current = theme();
    const VisualStateSnapshot state = visualStateFor(node);

    if (const auto* button = dynamic_cast<const ButtonNode*>(&node)) {
        return resolveButtonStyle(*button, current, state);
    }
    if (const auto* checkbox = dynamic_cast<const CheckboxNode*>(&node)) {
        return resolveCheckboxStyle(*checkbox, current, state);
    }
    if (const auto* radio = dynamic_cast<const RadioNode*>(&node)) {
        return resolveRadioStyle(*radio, current, state);
    }
    if (const auto* toggle = dynamic_cast<const SwitchNode*>(&node)) {
        return resolveSwitchStyle(*toggle, current, state);
    }
    if (dynamic_cast<const SliderNode*>(&node) != nullptr) {
        return resolveSliderStyle(current, state);
    }
    if (dynamic_cast<const ProgressBarNode*>(&node) != nullptr) {
        return resolveProgressStyle(current);
    }
    if (const auto* divider = dynamic_cast<const DividerNode*>(&node)) {
        return resolveDividerStyle(*divider, current);
    }
    return std::nullopt;
}

} // namespace wui::detail
