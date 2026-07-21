#include "wui/widgets.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "wui/icons.h"
#include "wui/text_metrics.h"
#include "wui/theme.h"
#include "wui/theme_extensions.h"

#include "button_visuals.h"

namespace wui {
namespace button_visuals {

[[nodiscard]] float buttonHeight(const Theme& current, ButtonSize size) noexcept
{
    switch (size) {
    case ButtonSize::Small: return 24.0f;
    case ButtonSize::Large: return 40.0f;
    case ButtonSize::Medium: default: return 32.0f;
    }
}

[[nodiscard]] float buttonRadius(const Theme& current, ButtonShape shape, float height) noexcept
{
    switch (shape) {
    case ButtonShape::Square: return current.radius.none;
    case ButtonShape::Circular: return height * 0.5f;
    case ButtonShape::Rounded: default: return current.radius.medium;
    }
}

[[nodiscard]] TextStyleToken controlTextStyle(const Theme& current,
                                              TextStyleToken style) noexcept
{
    if (!current.typography.familyControls.empty()) {
        style.family = current.typography.familyControls;
    }
    return style;
}

[[nodiscard]] TextStyleToken buttonTextStyle(const Theme& current,
                                             ButtonSize size) noexcept
{
    // Fluent Web Button: small = 12/16 Regular, medium = 14/20 Semibold,
    // large = 16/22 Semibold. The reference component uses classic Segoe UI.
    switch (size) {
    case ButtonSize::Small:
        return controlTextStyle(current, current.typography.caption1);
    case ButtonSize::Large:
        return controlTextStyle(current, current.typography.subtitle2);
    case ButtonSize::Medium:
    default:
        return controlTextStyle(current, current.typography.body1Strong);
    }
}

[[nodiscard]] constexpr Color transparent() noexcept
{
    return {0, 0, 0, 0};
}

// Figma wraps a Button label in a container with 2 DIP bottom padding.
// Centering that wrapper shifts the actual line box upward by 1 DIP.
[[nodiscard]] constexpr float buttonLabelOpticalOffset() noexcept
{
    return -1.0f;
}

[[nodiscard]] float buttonHorizontalPadding(const Theme& current,
                                            ButtonSize size) noexcept
{
    switch (size) {
    case ButtonSize::Small: return current.spacing.horizontal.s;
    case ButtonSize::Large: return current.spacing.horizontal.l;
    case ButtonSize::Medium:
    default: return current.spacing.horizontal.m;
    }
}

[[nodiscard]] float buttonContentGap(const Theme& current,
                                     ButtonSize size) noexcept
{
    return size == ButtonSize::Small ? current.spacing.horizontal.xs
                                     : current.spacing.horizontal.sNudge;
}

[[nodiscard]] IconSize buttonIconSize(ButtonSize size) noexcept
{
    switch (size) {
    case ButtonSize::Small: return IconSize::Size16;
    case ButtonSize::Large: return IconSize::Size24;
    case ButtonSize::Medium:
    default: return IconSize::Size20;
    }
}

[[nodiscard]] float iconLogicalSize(IconSize size) noexcept
{
    return static_cast<float>(static_cast<std::uint8_t>(size));
}

[[nodiscard]] float measuredButtonTextWidth(const std::string& label,
                                            const TextStyleToken& style)
{
    if (const TextMeasurer* measurer = textMeasurer()) {
        return measurer->measureText(label, style.size, style.weight,
                                     style.family).width;
    }
    return static_cast<float>(label.size()) * (style.size * 0.56f);
}

[[nodiscard]] ButtonVisual resolveButtonVisual(
    const Theme& current, ButtonAppearance appearance, bool disabled,
    bool selected, ControlVisualStates states) noexcept
{
    const bool pressed =
        (states & toMask(ControlVisualState::Pressed)) != 0;
    const bool hovered =
        (states & toMask(ControlVisualState::Hovered)) != 0;
    ButtonVisual visual{current.colors.neutralBackground1.rest,
                        current.colors.neutralForeground1,
                        current.colors.neutralStroke1, true,
                        current.stroke.thin};

    switch (appearance) {
    case ButtonAppearance::Primary:
        visual.background = current.colors.brandBackground.rest;
        visual.foreground = current.colors.onBrand;
        visual.hasBorder = false;
        break;
    case ButtonAppearance::Danger:
        visual.background = current.colors.dangerBackground.rest;
        visual.foreground = current.colors.onBrand;
        visual.hasBorder = false;
        break;
    case ButtonAppearance::Outline:
        visual.background = transparent();
        break;
    case ButtonAppearance::Subtle:
    case ButtonAppearance::Transparent:
        visual.background = transparent();
        visual.foreground = current.colors.neutralForeground2;
        visual.hasBorder = false;
        break;
    case ButtonAppearance::Secondary:
    default:
        break;
    }

    if (disabled) {
        visual.foreground = current.colors.neutralForegroundDisabled;
        visual.border = current.colors.neutralStrokeDisabled;
        if (appearance == ButtonAppearance::Primary ||
            appearance == ButtonAppearance::Danger ||
            appearance == ButtonAppearance::Secondary) {
            visual.background = current.colors.neutralBackgroundDisabled;
        } else {
            visual.background = transparent();
        }
        return visual;
    }

    if (pressed) {
        switch (appearance) {
        case ButtonAppearance::Primary:
            visual.background = current.colors.brandBackground.pressed;
            break;
        case ButtonAppearance::Danger:
            visual.background = current.colors.dangerBackground.pressed;
            break;
        case ButtonAppearance::Secondary:
            visual.background = current.colors.neutralBackground1.pressed;
            visual.border = current.colors.neutralStroke1Pressed;
            break;
        case ButtonAppearance::Outline:
            visual.border = current.colors.neutralStroke1Pressed;
            break;
        case ButtonAppearance::Subtle:
            visual.background = current.colors.neutralBackground1.pressed;
            visual.foreground = current.colors.neutralForeground1;
            break;
        case ButtonAppearance::Transparent:
            visual.foreground = current.colors.brandBackground.hover;
            break;
        }
        return visual;
    }

    if (hovered) {
        switch (appearance) {
        case ButtonAppearance::Primary:
            visual.background = current.colors.brandBackground.hover;
            break;
        case ButtonAppearance::Danger:
            visual.background = current.colors.dangerBackground.hover;
            break;
        case ButtonAppearance::Secondary:
            visual.background = current.colors.neutralBackground1.hover;
            visual.border = current.colors.neutralStroke1Hover;
            break;
        case ButtonAppearance::Outline:
            visual.border = current.colors.neutralStroke1Hover;
            break;
        case ButtonAppearance::Subtle:
            visual.background = current.colors.neutralBackground1.hover;
            visual.foreground = current.colors.neutralForeground1;
            break;
        case ButtonAppearance::Transparent:
            visual.foreground = current.colors.brandForeground1;
            break;
        }
        return visual;
    }

    if (selected) {
        switch (appearance) {
        case ButtonAppearance::Primary:
            visual.background = current.colors.brandBackground.selected;
            break;
        case ButtonAppearance::Danger:
            visual.background = current.colors.dangerBackground.selected;
            break;
        case ButtonAppearance::Secondary:
            visual.background = current.colors.neutralBackground1.selected;
            visual.border = current.colors.neutralStroke1Selected;
            break;
        case ButtonAppearance::Outline:
            visual.border = current.colors.neutralStroke1Selected;
            visual.borderWidth = current.stroke.thick;
            break;
        case ButtonAppearance::Subtle:
            visual.background = current.colors.neutralBackground1.selected;
            visual.foreground = current.colors.neutralForeground1;
            break;
        case ButtonAppearance::Transparent:
            visual.foreground = current.colors.brandForeground1;
            break;
        }
    }
    return visual;
}

void drawButtonContent(PaintContext& context, const RectF& bounds,
                       const std::string& label,
                       const std::optional<IconName>& icon,
                       IconStyle iconStyle, ButtonIconPosition iconPosition,
                       bool iconOnly, ButtonSize size, Color foreground,
                       const Theme& current)
{
    const RectF alignedBounds = context.snapRectEdges(bounds);
    const bool hasIcon = icon.has_value();
    const bool showText = !label.empty() && (!iconOnly || !hasIcon);
    const auto textStyle = buttonTextStyle(current, size);
    const float textWidth =
        showText ? measuredButtonTextWidth(label, textStyle) : 0.0f;
    const IconSize semanticIconSize = buttonIconSize(size);
    const float iconSize =
        hasIcon ? iconLogicalSize(semanticIconSize) : 0.0f;
    const float gap =
        hasIcon && showText ? buttonContentGap(current, size) : 0.0f;
    const float contentWidth = textWidth + iconSize + gap;
    float cursor =
        alignedBounds.x +
        std::max(0.0f, (alignedBounds.width - contentWidth) * 0.5f);

    const auto paintIcon = [&](float x) {
        if (!icon) return;
        const RectF iconBounds{x,
                               alignedBounds.y +
                                   (alignedBounds.height - iconSize) * 0.5f,
                               iconSize, iconSize};
        drawIcon(context, *icon, iconBounds, foreground, semanticIconSize,
                 iconStyle);
    };

    if (hasIcon && iconPosition == ButtonIconPosition::Before) {
        paintIcon(cursor);
        cursor += iconSize + gap;
    }
    if (showText) {
        context.drawText(
            label, cursor,
            context.centeredTextBottom(label, alignedBounds, textStyle.size,
                                       textStyle.weight, textStyle.family) +
                buttonLabelOpticalOffset(),
            textStyle.size, foreground, textStyle.weight, textStyle.family);
        cursor += textWidth + gap;
    }
    if (hasIcon && iconPosition == ButtonIconPosition::After) {
        paintIcon(cursor);
    }
}

void drawFocusRing(PaintContext& context, const RectF& bounds, const Theme& current, bool focused, float radius)
{
    if (!focused) return;
    const float inset = current.controls.focusInset;
    const float outerStroke =
        context.snapStrokeWidth(current.stroke.thick);
    const float innerStroke =
        context.snapStrokeWidth(current.stroke.thin);
    context.strokeRoundRect({bounds.x - inset, bounds.y - inset,
                             bounds.width + inset * 2.0f, bounds.height + inset * 2.0f},
                            radius + inset, outerStroke,
                            current.colors.strokeFocusOuter);
    const float innerInset =
        std::max(0.0f, inset - innerStroke * 0.5f);
    context.strokeRoundRect(
        {bounds.x - innerInset, bounds.y - innerInset,
         bounds.width + innerInset * 2.0f,
         bounds.height + innerInset * 2.0f},
        radius + innerInset, innerStroke,
        current.colors.strokeFocusInner);
}

ButtonVisual paintButtonSurface(
    PaintContext& context, const RectF& bounds, const Theme& current,
    ButtonAppearance appearance, bool disabled, bool selected,
    ControlVisualStates states, ButtonShape shape)
{
    const RectF alignedBounds = context.snapRectEdges(bounds);
    const bool focusVisible =
        !disabled &&
        (states & toMask(ControlVisualState::FocusVisible)) != 0;
    const auto visual = resolveButtonVisual(
        current, appearance, disabled, selected, states);
    const float radius =
        buttonRadius(current, shape, alignedBounds.height);
    drawFocusRing(context, alignedBounds, current, focusVisible, radius);
    if (visual.hasBorder) {
        context.fillStrokeRoundRect(
            alignedBounds, radius,
            context.snapStrokeWidth(visual.borderWidth),
            visual.background, visual.border);
    } else if (visual.background.a != 0) {
        context.fillRoundRect(alignedBounds, radius, visual.background);
    }
    return visual;
}

} // namespace button_visuals

namespace {

using namespace button_visuals;

[[nodiscard]] float checkboxLabelWidth(const std::string& label, const TextStyleToken& style)
{
    if (const auto* measurer = textMeasurer()) {
        return measurer->measureText(label, style.size, style.weight).width;
    }
    return static_cast<float>(label.size()) * style.size * 0.56f;
}

[[nodiscard]] std::vector<TextLayoutLine> checkboxLabelLines(
    const std::string& label, const TextStyleToken& style, float availableWidth)
{
    if (const auto* provider = dynamic_cast<const TextLayoutProvider*>(textMeasurer());
        provider && std::isfinite(availableWidth)) {
        auto lines = provider->layoutText(label, style.size, style.weight,
                                          std::max(1.0f, availableWidth),
                                          style.lineHeight, 0, false);
        if (!lines.empty()) return lines;
    }
    return {{label, 0, label.size(), checkboxLabelWidth(label, style), false}};
}

} // namespace

using namespace button_visuals;

Button::Button(std::string label)
    : label_(std::move(label))
{
}

const std::string& Button::label() const noexcept
{
    return label_;
}

Button& Button::label(std::string label)
{
    setLabel(std::move(label));
    return *this;
}

void Button::setLabel(std::string label)
{
    label_ = std::move(label);
    markDirty(DirtyFlag::Layout);
}

Button& Button::onClick(ClickHandler handler)
{
    onClick_ = std::move(handler);
    return *this;
}

void Button::setVariant(ButtonVariant variant) noexcept
{
    variant_ = variant;
    switch (variant) {
    case ButtonVariant::Primary: appearance_ = ButtonAppearance::Primary; break;
    case ButtonVariant::Secondary: appearance_ = ButtonAppearance::Secondary; break;
    // Legacy Ghost rendered as a bordered secondary action, so it maps to
    // Outline rather than the borderless Fluent Subtle appearance.
    case ButtonVariant::Ghost: appearance_ = ButtonAppearance::Outline; break;
    case ButtonVariant::Danger: appearance_ = ButtonAppearance::Danger; break;
    }
    markDirty(DirtyFlag::Paint);
}

ButtonVariant Button::variant() const noexcept
{
    return variant_;
}

void Button::setAppearance(ButtonAppearance appearance) noexcept
{
    appearance_ = appearance;
    switch (appearance) {
    case ButtonAppearance::Primary: variant_ = ButtonVariant::Primary; break;
    case ButtonAppearance::Danger: variant_ = ButtonVariant::Danger; break;
    case ButtonAppearance::Secondary: variant_ = ButtonVariant::Secondary; break;
    default: variant_ = ButtonVariant::Ghost; break;
    }
    markDirty(DirtyFlag::Paint);
}

ButtonAppearance Button::appearance() const noexcept { return appearance_; }

void Button::setSize(ButtonSize size) noexcept
{
    if (size_ == size) return;
    size_ = size;
    markDirty(DirtyFlag::Layout);
}

ButtonSize Button::size() const noexcept { return size_; }

void Button::setShape(ButtonShape shape) noexcept
{
    if (shape_ == shape) return;
    shape_ = shape;
    markDirty(DirtyFlag::Paint);
}

ButtonShape Button::shape() const noexcept { return shape_; }

Button& Button::icon(IconName value) noexcept
{
    setIcon(value);
    return *this;
}

Button& Button::iconStyle(IconStyle value) noexcept
{
    setIconStyle(value);
    return *this;
}

Button& Button::iconPosition(ButtonIconPosition value) noexcept
{
    setIconPosition(value);
    return *this;
}

Button& Button::iconOnly(bool value) noexcept
{
    setIconOnly(value);
    return *this;
}

Button& Button::clearIcon() noexcept
{
    setIcon(std::nullopt);
    return *this;
}

void Button::setIcon(std::optional<IconName> value) noexcept
{
    if (icon_ == value) return;
    icon_ = value;
    markDirty(DirtyFlag::Layout);
}

void Button::setIconStyle(IconStyle value) noexcept
{
    if (iconStyle_ == value) return;
    iconStyle_ = value;
    markDirty(DirtyFlag::Paint);
}

void Button::setIconPosition(ButtonIconPosition value) noexcept
{
    if (iconPosition_ == value) return;
    iconPosition_ = value;
    markDirty(DirtyFlag::Layout);
}

void Button::setIconOnly(bool value) noexcept
{
    if (iconOnly_ == value) return;
    iconOnly_ = value;
    markDirty(DirtyFlag::Layout);
}

std::optional<IconName> Button::icon() const noexcept { return icon_; }
IconStyle Button::iconStyle() const noexcept { return iconStyle_; }
ButtonIconPosition Button::iconPosition() const noexcept
{
    return iconPosition_;
}
bool Button::isIconOnly() const noexcept { return iconOnly_; }

SizeF Button::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    const auto& textStyle = buttonTextStyle(current, size_);
    const float height = buttonHeight(current, size_);
    if (iconOnly_ && icon_) {
        return constraints.clamp({height, height});
    }
    const float textWidth = measuredButtonTextWidth(label_, textStyle);
    const float iconWidth =
        icon_ ? iconLogicalSize(buttonIconSize(size_)) : 0.0f;
    const float gap =
        icon_ && !label_.empty() ? buttonContentGap(current, size_) : 0.0f;
    return constraints.clamp(
        {textWidth + iconWidth + gap +
             buttonHorizontalPadding(current, size_) * 2.0f,
         height});
}

void Button::paint(PaintContext& context)
{
    const Theme& current = theme();
    const bool disabled = !isEnabled();
    const auto visual = paintButtonSurface(
        context, bounds(), current, appearance_, disabled, false,
        visualStates(), shape_);
    drawButtonContent(context, bounds(), label_, icon_, iconStyle_,
                      iconPosition_, iconOnly_, size_, visual.foreground,
                      current);
    ContainerNode::paint(context);
    clearDirty(DirtyFlag::Paint);
}

bool Button::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) {
        return false;
    }
    switch (event.action) {
    case PointerAction::Down:
        if (event.button == MouseButton::Left) {
            setVisualState(ControlVisualState::Pressed, true);
            setVisualState(ControlVisualState::Focused, true);
            return true;
        }
        return false;
    case PointerAction::Up:
        if (event.button == MouseButton::Left) {
            const bool shouldClick = (visualStates() & toMask(ControlVisualState::Pressed)) != 0 && bounds().contains(event.position);
            setVisualState(ControlVisualState::Pressed, false);
            if (shouldClick && onClick_) {
                onClick_();
            }
            return true;
        }
        return false;
    case PointerAction::Enter:
        setVisualState(ControlVisualState::Hovered, true);
        return true;
    case PointerAction::Move:
        setVisualState(ControlVisualState::Hovered, bounds().contains(event.position));
        return true;
    case PointerAction::Leave:
        setVisualState(ControlVisualState::Hovered, false);
        return true;
    case PointerAction::Cancel:
        setVisualState(ControlVisualState::Pressed, false);
        return true;
    }

    return false;
}

AccessibilityActionCapabilities Button::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions;
    actions.invoke = static_cast<bool>(onClick_);
    return actions;
}

AccessibilityActionStatus Button::performAccessibilityAction(
    AccessibilityActionKind kind, std::string_view value)
{
    (void)value;
    if (kind != AccessibilityActionKind::Invoke) return AccessibilityActionStatus::NotSupported;
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    if (!onClick_) return AccessibilityActionStatus::NotSupported;
    onClick_();
    return AccessibilityActionStatus::Succeeded;
}

namespace {
float compoundHeight(ButtonSize size) noexcept
{
    switch (size) { case ButtonSize::Small: return 40.0f; case ButtonSize::Large: return 64.0f; default: return 52.0f; }
}
}

CompoundButton::CompoundButton(std::string label, std::string secondaryContent)
    : label_(std::move(label)), secondaryContent_(std::move(secondaryContent)) {}
const std::string& CompoundButton::label() const noexcept { return label_; }
const std::string& CompoundButton::secondaryContent() const noexcept { return secondaryContent_; }
CompoundButton& CompoundButton::label(std::string value) { setLabel(std::move(value)); return *this; }
CompoundButton& CompoundButton::secondaryContent(std::string value) { setSecondaryContent(std::move(value)); return *this; }
void CompoundButton::setLabel(std::string value) { label_ = std::move(value); markDirty(DirtyFlag::Layout); }
void CompoundButton::setSecondaryContent(std::string value) { secondaryContent_ = std::move(value); markDirty(DirtyFlag::Layout); }
CompoundButton& CompoundButton::onClick(ClickHandler handler) { onClick_ = std::move(handler); return *this; }
void CompoundButton::setAppearance(ButtonAppearance value) noexcept { appearance_ = value; markDirty(DirtyFlag::Paint); }
ButtonAppearance CompoundButton::appearance() const noexcept { return appearance_; }
void CompoundButton::setSize(ButtonSize value) noexcept { if (size_ != value) { size_ = value; markDirty(DirtyFlag::Layout); } }
ButtonSize CompoundButton::size() const noexcept { return size_; }
void CompoundButton::setShape(ButtonShape value) noexcept { if (shape_ != value) { shape_ = value; markDirty(DirtyFlag::Paint); } }
ButtonShape CompoundButton::shape() const noexcept { return shape_; }

SizeF CompoundButton::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    const auto textWidth = [](const std::string& text, const TextStyleToken& style) {
        if (const auto* measurer = textMeasurer()) {
            return measurer->measureText(text, style.size, style.weight,
                                         style.family).width;
        }
        return static_cast<float>(text.size()) * style.size * 0.56f;
    };
    const auto titleStyle =
        controlTextStyle(current, current.typography.body1Strong);
    const auto descriptionStyle =
        controlTextStyle(current, current.typography.caption1);
    const float width = std::max(textWidth(label_, titleStyle),
                                 textWidth(secondaryContent_, descriptionStyle));
    const float padding = buttonHorizontalPadding(current, size_);
    return constraints.clamp({width + padding * 2.0f, compoundHeight(size_)});
}

void CompoundButton::paint(PaintContext& context)
{
    const auto& current = theme();
    const bool disabled = !isEnabled();
    const auto visual = paintButtonSurface(
        context, bounds(), current, appearance_, disabled, false,
        visualStates(), shape_);
    Color primary = visual.foreground;
    Color secondary =
        disabled ? current.colors.neutralForegroundDisabled
        : appearance_ == ButtonAppearance::Primary ||
                appearance_ == ButtonAppearance::Danger
            ? visual.foreground
            : current.colors.neutralForeground3;
    const float padding = buttonHorizontalPadding(current, size_);
    const auto titleStyle =
        controlTextStyle(current, current.typography.body1Strong);
    const auto descriptionStyle =
        controlTextStyle(current, current.typography.caption1);
    const float titleHeight = titleStyle.lineHeight;
    const float descriptionHeight =
        secondaryContent_.empty() ? 0.0f : descriptionStyle.lineHeight;
    const float blockHeight = titleHeight + descriptionHeight;
    const float startY = bounds().y + std::max(0.0f, (bounds().height - blockHeight) * 0.5f);
    const RectF titleBox{bounds().x + padding, startY,
                         std::max(0.0f, bounds().width - padding * 2.0f),
                         titleHeight};
    context.drawText(label_, titleBox.x,
                     context.centeredTextBottom(label_, titleBox,
                                                titleStyle.size, titleStyle.weight,
                                                titleStyle.family),
                     titleStyle.size, primary, titleStyle.weight, titleStyle.family);
    if (!secondaryContent_.empty()) {
        const RectF box{titleBox.x, startY + titleHeight, titleBox.width,
                        descriptionHeight};
        context.drawText(secondaryContent_, box.x,
                         context.centeredTextBottom(secondaryContent_, box,
                                                    descriptionStyle.size,
                                                    descriptionStyle.weight,
                                                    descriptionStyle.family),
                         descriptionStyle.size, secondary, descriptionStyle.weight,
                         descriptionStyle.family);
    }
    clearDirty(DirtyFlag::Paint);
}

bool CompoundButton::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    switch (event.action) {
    case PointerAction::Down: if (event.button == MouseButton::Left) { setVisualState(ControlVisualState::Pressed,true); setVisualState(ControlVisualState::Focused,true); return true; } return false;
    case PointerAction::Up: if (event.button == MouseButton::Left) { const bool invoke = (visualStates() & toMask(ControlVisualState::Pressed)) && bounds().contains(event.position); setVisualState(ControlVisualState::Pressed,false); if (invoke && onClick_) onClick_(); return true; } return false;
    case PointerAction::Enter: setVisualState(ControlVisualState::Hovered,true); return true;
    case PointerAction::Move: setVisualState(ControlVisualState::Hovered,bounds().contains(event.position)); return true;
    case PointerAction::Leave: setVisualState(ControlVisualState::Hovered,false); return true;
    case PointerAction::Cancel: setVisualState(ControlVisualState::Pressed,false); return true;
    default: return false;
    }
}
AccessibilityActionCapabilities CompoundButton::accessibilityActions() const noexcept { AccessibilityActionCapabilities a; a.invoke = static_cast<bool>(onClick_); return a; }
AccessibilityActionStatus CompoundButton::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value)
{ (void)value; if (kind != AccessibilityActionKind::Invoke) return AccessibilityActionStatus::NotSupported; if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled; if (!onClick_) return AccessibilityActionStatus::NotSupported; onClick_(); return AccessibilityActionStatus::Succeeded; }

ToggleButton::ToggleButton(std::string label, bool checked)
    : label_(std::move(label)), checked_(checked)
{
}

const std::string& ToggleButton::label() const noexcept { return label_; }
ToggleButton& ToggleButton::label(std::string value) { setLabel(std::move(value)); return *this; }
void ToggleButton::setLabel(std::string value) { label_ = std::move(value); markDirty(DirtyFlag::Layout); }
bool ToggleButton::isChecked() const noexcept { return hasBinding_ ? binding_->get() : checked_; }
ToggleButton& ToggleButton::checked(bool value) { setChecked(value); return *this; }
void ToggleButton::setChecked(bool value)
{
    if (hasBinding_) binding_->set(value);
    else if (checked_ != value) { checked_ = value; markDirty(DirtyFlag::Paint); }
}
ToggleButton& ToggleButton::bind(State<bool>& state)
{
    binding_.emplace(state);
    hasBinding_ = true;
    checked_ = state.get();
    const auto id = state.subscribe([this](bool value) { checked_ = value; markDirty(DirtyFlag::Paint); });
    addTeardown([&state, id] { state.unsubscribe(id); });
    markDirty(DirtyFlag::Paint);
    return *this;
}
ToggleButton& ToggleButton::onChange(ChangeHandler handler) { onChange_ = std::move(handler); return *this; }
void ToggleButton::setSize(ButtonSize value) noexcept { if (size_ != value) { size_ = value; markDirty(DirtyFlag::Layout); } }
ButtonSize ToggleButton::size() const noexcept { return size_; }
void ToggleButton::setShape(ButtonShape value) noexcept { if (shape_ != value) { shape_ = value; markDirty(DirtyFlag::Paint); } }
ButtonShape ToggleButton::shape() const noexcept { return shape_; }
void ToggleButton::setAppearance(ButtonAppearance value) noexcept
{
    if (appearance_ == value) return;
    appearance_ = value;
    markDirty(DirtyFlag::Paint);
}
ButtonAppearance ToggleButton::appearance() const noexcept
{
    return appearance_;
}
ToggleButton& ToggleButton::icon(IconName value) noexcept
{
    setIcon(value);
    return *this;
}
ToggleButton& ToggleButton::iconStyle(IconStyle value) noexcept
{
    setIconStyle(value);
    return *this;
}
ToggleButton& ToggleButton::iconPosition(ButtonIconPosition value) noexcept
{
    setIconPosition(value);
    return *this;
}
ToggleButton& ToggleButton::iconOnly(bool value) noexcept
{
    setIconOnly(value);
    return *this;
}
ToggleButton& ToggleButton::clearIcon() noexcept
{
    setIcon(std::nullopt);
    return *this;
}
void ToggleButton::setIcon(std::optional<IconName> value) noexcept
{
    if (icon_ == value) return;
    icon_ = value;
    markDirty(DirtyFlag::Layout);
}
void ToggleButton::setIconStyle(IconStyle value) noexcept
{
    if (iconStyle_ == value) return;
    iconStyle_ = value;
    markDirty(DirtyFlag::Paint);
}
void ToggleButton::setIconPosition(ButtonIconPosition value) noexcept
{
    if (iconPosition_ == value) return;
    iconPosition_ = value;
    markDirty(DirtyFlag::Layout);
}
void ToggleButton::setIconOnly(bool value) noexcept
{
    if (iconOnly_ == value) return;
    iconOnly_ = value;
    markDirty(DirtyFlag::Layout);
}
std::optional<IconName> ToggleButton::icon() const noexcept { return icon_; }
IconStyle ToggleButton::iconStyle() const noexcept { return iconStyle_; }
ButtonIconPosition ToggleButton::iconPosition() const noexcept
{
    return iconPosition_;
}
bool ToggleButton::isIconOnly() const noexcept { return iconOnly_; }

SizeF ToggleButton::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    const auto& textStyle = buttonTextStyle(current, size_);
    const float height = buttonHeight(current, size_);
    if (iconOnly_ && icon_) {
        return constraints.clamp({height, height});
    }
    const float textWidth = measuredButtonTextWidth(label_, textStyle);
    const float iconWidth =
        icon_ ? iconLogicalSize(buttonIconSize(size_)) : 0.0f;
    const float gap =
        icon_ && !label_.empty() ? buttonContentGap(current, size_) : 0.0f;
    return constraints.clamp(
        {textWidth + iconWidth + gap +
             buttonHorizontalPadding(current, size_) * 2.0f,
         height});
}

void ToggleButton::paint(PaintContext& context)
{
    const auto& current = theme();
    const bool enabled = isEnabled();
    const bool selected = isChecked();
    const auto visual = paintButtonSurface(
        context, bounds(), current, appearance_, !enabled, selected,
        visualStates(), shape_);
    drawButtonContent(context, bounds(), label_, icon_,
                      selected ? IconStyle::Filled : iconStyle_,
                      iconPosition_, iconOnly_, size_, visual.foreground,
                      current);
    clearDirty(DirtyFlag::Paint);
}

void ToggleButton::toggle()
{
    const bool value = !isChecked();
    setChecked(value);
    if (onChange_) onChange_(value);
}

bool ToggleButton::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    switch (event.action) {
    case PointerAction::Down:
        if (event.button != MouseButton::Left) return false;
        setVisualState(ControlVisualState::Pressed, true);
        setVisualState(ControlVisualState::Focused, true);
        return true;
    case PointerAction::Up:
        if (event.button != MouseButton::Left) return false;
        { const bool activate = (visualStates() & toMask(ControlVisualState::Pressed)) != 0 && bounds().contains(event.position);
          setVisualState(ControlVisualState::Pressed, false); if (activate) toggle(); return true; }
    case PointerAction::Enter: setVisualState(ControlVisualState::Hovered, true); return true;
    case PointerAction::Move: setVisualState(ControlVisualState::Hovered, bounds().contains(event.position)); return true;
    case PointerAction::Leave: setVisualState(ControlVisualState::Hovered, false); return true;
    case PointerAction::Cancel: setVisualState(ControlVisualState::Pressed, false); return true;
    default: return false;
    }
}

bool ToggleButton::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || event.action != KeyAction::Down || (event.keyCode != 32 && event.keyCode != 13)) return false;
    toggle();
    return true;
}

AccessibilityActionCapabilities ToggleButton::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions;
    actions.toggle = true;
    return actions;
}

AccessibilityActionStatus ToggleButton::performAccessibilityAction(
    AccessibilityActionKind kind, std::string_view value)
{
    (void)value;
    if (kind != AccessibilityActionKind::Toggle) return AccessibilityActionStatus::NotSupported;
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    toggle();
    return AccessibilityActionStatus::Succeeded;
}

Checkbox::Checkbox(std::string label, bool checked)
    : label_(std::move(label))
    , checked_(checked)
{
}

const std::string& Checkbox::label() const noexcept { return label_; }
Checkbox& Checkbox::label(std::string label) { setLabel(std::move(label)); return *this; }
void Checkbox::setLabel(std::string label) { label_ = std::move(label); markDirty(DirtyFlag::Layout); }
const std::string& Checkbox::accessibleLabel() const noexcept { return accessibleLabel_; }
Checkbox& Checkbox::accessibleLabel(std::string label) { setAccessibleLabel(std::move(label)); return *this; }
void Checkbox::setAccessibleLabel(std::string label) { accessibleLabel_ = std::move(label); }
bool Checkbox::isChecked() const noexcept { return hasBinding_ ? binding_->get() : checked_; }
bool Checkbox::isMixed() const noexcept { return mixed_; }
CheckboxState Checkbox::state() const noexcept
{
    if (mixed_) return CheckboxState::Mixed;
    return isChecked() ? CheckboxState::Checked : CheckboxState::Unchecked;
}
Checkbox& Checkbox::checked(bool value) { setChecked(value); return *this; }
void Checkbox::setChecked(bool value)
{
    const bool wasMixed = mixed_;
    mixed_ = false;
    if (hasBinding_) {
        binding_->set(value);
    } else if (checked_ != value) {
        checked_ = value;
        markDirty(DirtyFlag::Paint);
    }
    if (wasMixed) markDirty(DirtyFlag::Paint);
}

Checkbox& Checkbox::mixed(bool value) { setMixed(value); return *this; }
void Checkbox::setMixed(bool value)
{
    if (mixed_ == value) return;
    mixed_ = value;
    if (value) {
        if (hasBinding_) binding_->set(false);
        else checked_ = false;
    }
    markDirty(DirtyFlag::Paint);
}

Checkbox& Checkbox::checkState(CheckboxState value) { setCheckState(value); return *this; }
void Checkbox::setCheckState(CheckboxState value)
{
    if (value == CheckboxState::Mixed) {
        setMixed(true);
    } else {
        setChecked(value == CheckboxState::Checked);
    }
}

Checkbox& Checkbox::bind(State<bool>& state)
{
    binding_.emplace(state);
    hasBinding_ = true;
    checked_ = state.get();
    mixed_ = false;
    subscription_.subscribe(state, [this](bool value) {
        checked_ = value;
        mixed_ = false;
        markDirty(DirtyFlag::Paint);
    });
    markDirty(DirtyFlag::Paint);
    return *this;
}

Checkbox& Checkbox::onChange(ChangeHandler handler) { onChange_ = std::move(handler); return *this; }
Checkbox& Checkbox::onStateChange(StateChangeHandler handler) { onStateChange_ = std::move(handler); return *this; }
Checkbox& Checkbox::size(CheckboxSize value) noexcept { setSize(value); return *this; }
void Checkbox::setSize(CheckboxSize value) noexcept
{
    if (size_ == value) return;
    size_ = value;
    markDirty(DirtyFlag::Layout);
}
CheckboxSize Checkbox::size() const noexcept { return size_; }
Checkbox& Checkbox::shape(CheckboxShape value) noexcept { setShape(value); return *this; }
void Checkbox::setShape(CheckboxShape value) noexcept
{
    if (shape_ == value) return;
    shape_ = value;
    markDirty(DirtyFlag::Paint);
}
CheckboxShape Checkbox::shape() const noexcept { return shape_; }
Checkbox& Checkbox::labelPosition(CheckboxLabelPosition value) noexcept { setLabelPosition(value); return *this; }
void Checkbox::setLabelPosition(CheckboxLabelPosition value) noexcept
{
    if (labelPosition_ == value) return;
    labelPosition_ = value;
    markDirty(DirtyFlag::Layout);
}
CheckboxLabelPosition Checkbox::labelPosition() const noexcept { return labelPosition_; }
Checkbox& Checkbox::required(bool value) noexcept { setRequired(value); return *this; }
void Checkbox::setRequired(bool value) noexcept
{
    if (required_ == value) return;
    required_ = value;
    markDirty(DirtyFlag::Layout);
}
bool Checkbox::isRequired() const noexcept { return required_; }
SizeF Checkbox::measure(const Constraints& constraints) const
{
    const Theme& current = theme();
    const float hitSize = size_ == CheckboxSize::Large ? 36.0f : 32.0f;
    const TextStyleToken& style = size_ == CheckboxSize::Large
        ? current.typography.body2
        : current.typography.body1;
    const float labelWidth = checkboxLabelWidth(label_, style);
    const float requiredWidth = required_ && !label_.empty()
        ? current.spacing.horizontal.xs + checkboxLabelWidth("*", style)
        : 0.0f;
    if (label_.empty()) return constraints.clamp({hitSize, hitSize});
    const float availableLabelWidth = std::isfinite(constraints.maxWidth)
        ? std::max(1.0f, constraints.maxWidth - hitSize - current.spacing.horizontal.xs - requiredWidth)
        : std::numeric_limits<float>::infinity();
    const auto lines = checkboxLabelLines(label_, style, availableLabelWidth);
    float widest = 0.0f;
    for (const auto& line : lines) widest = std::max(widest, line.width);
    const float contentWidth = hitSize + current.spacing.horizontal.xs +
        std::min(labelWidth, widest) + requiredWidth;
    const float contentHeight = std::max(hitSize, static_cast<float>(lines.size()) * style.lineHeight);
    return constraints.clamp({contentWidth, contentHeight});
}

void Checkbox::paint(PaintContext& context)
{
    const Theme& current = theme();
    const bool enabled = isEnabled();
    const bool focusVisible =
        enabled
        && (visualStates() & toMask(ControlVisualState::FocusVisible)) != 0;
    const CheckboxState checkState = state();
    const bool checked = checkState == CheckboxState::Checked;
    const bool mixed = checkState == CheckboxState::Mixed;
    const bool showsMark = checked || mixed;
    StateProperty<Color> checkedFace{current.colors.compoundBrandBackground.rest};
    checkedFace.set(ControlVisualState::Hovered,
                    current.colors.compoundBrandBackground.hover)
        .set(ControlVisualState::Pressed,
             current.colors.compoundBrandBackground.pressed);
    StateProperty<Color> mixedBorder{current.colors.compoundBrandStroke.rest};
    mixedBorder.set(ControlVisualState::Hovered,
                    current.colors.compoundBrandStroke.hover)
        .set(ControlVisualState::Pressed,
             current.colors.compoundBrandStroke.pressed);
    StateProperty<Color> mixedMark{current.colors.compoundBrandForeground1.rest};
    mixedMark.set(ControlVisualState::Hovered,
                  current.colors.compoundBrandForeground1.hover)
        .set(ControlVisualState::Pressed,
             current.colors.compoundBrandForeground1.pressed);
    StateProperty<Color> uncheckedBorder{current.colors.neutralStrokeAccessible};
    uncheckedBorder.set(ControlVisualState::Hovered,
                        current.colors.neutralStrokeAccessibleHover)
        .set(ControlVisualState::Pressed,
             current.colors.neutralStrokeAccessiblePressed);
    StateProperty<Color> uncheckedText{current.colors.neutralForeground3};
    uncheckedText.set(ControlVisualState::Hovered,
                      current.colors.neutralForeground2)
        .set(ControlVisualState::Pressed,
             current.colors.neutralForeground1);

    Color box{0, 0, 0, 0};
    Color border = uncheckedBorder.resolve(visualStates());
    Color mark = current.colors.onBrand;
    Color text = uncheckedText.resolve(visualStates());
    if (checked) {
        box = checkedFace.resolve(visualStates());
        border = box;
        text = current.colors.neutralForeground1;
    } else if (mixed) {
        border = mixedBorder.resolve(visualStates());
        mark = mixedMark.resolve(visualStates());
        text = current.colors.neutralForeground1;
    }
    if (!enabled) {
        box = Color{0, 0, 0, 0};
        border = current.colors.neutralStrokeDisabled;
        mark = current.colors.neutralForegroundDisabled;
        text = current.colors.neutralForegroundDisabled;
    }
    const float hitSize = size_ == CheckboxSize::Large ? 36.0f : 32.0f;
    const float indicatorSize = size_ == CheckboxSize::Large ? 20.0f : 16.0f;
    const TextStyleToken& labelStyle = size_ == CheckboxSize::Large
        ? current.typography.body2
        : current.typography.body1;
    const float requiredLabelWidth = required_ && !label_.empty()
        ? current.spacing.horizontal.xs + checkboxLabelWidth("*", labelStyle)
        : 0.0f;
    const float availableLabelWidth = std::max(1.0f, bounds().width - hitSize -
        current.spacing.horizontal.xs - requiredLabelWidth);
    const auto labelLines = label_.empty()
        ? std::vector<TextLayoutLine>{}
        : checkboxLabelLines(label_, labelStyle, availableLabelWidth);
    const float textBlockHeight = static_cast<float>(labelLines.size()) * labelStyle.lineHeight;
    const float textTop = bounds().y + std::max(0.0f, (bounds().height - textBlockHeight) * 0.5f);
    const float leadingX = labelPosition_ == CheckboxLabelPosition::After
        ? bounds().x
        : bounds().x + std::max(0.0f, bounds().width - hitSize);
    const float indicatorY = labelLines.size() > 1
        ? textTop + (labelStyle.lineHeight - indicatorSize) * 0.5f
        : bounds().y + (bounds().height - indicatorSize) * 0.5f;
    const RectF indicator{leadingX + (hitSize - indicatorSize) * 0.5f,
                          indicatorY,
                          indicatorSize, indicatorSize};
    const RectF renderedIndicator = context.snapRectEdges(indicator);
    const float radius = shape_ == CheckboxShape::Circular
        ? std::min(renderedIndicator.width, renderedIndicator.height) * 0.5f
        : current.radius.small;
    // Fluent exposes a root focus-within outline, but only in keyboard
    // focus-visible modality. Pointer activation retains logical focus with
    // no persistent black frame around either the indicator or label.
    drawFocusRing(context, bounds(), current, focusVisible, current.radius.small);
    if (checked && enabled) {
        context.fillRoundRect(renderedIndicator, radius, box);
    } else {
        // Unchecked, mixed, and disabled indicators all retain a transparent
        // centre with a real one-DIP stroke.
        const float indicatorStroke =
            context.snapStrokeWidth(current.stroke.thin);
        const float halfStroke = indicatorStroke * 0.5f;
        context.strokeRoundRect({renderedIndicator.x + halfStroke,
                                 renderedIndicator.y + halfStroke,
                                 renderedIndicator.width - indicatorStroke,
                                 renderedIndicator.height - indicatorStroke},
                                std::max(0.0f, radius - halfStroke),
                                indicatorStroke, border);
    }
    if (showsMark) {
        const IconSize markSize =
            size_ == CheckboxSize::Large ? IconSize::Size16 : IconSize::Size12;
        if (mixed) {
            drawIcon(context,
                     shape_ == CheckboxShape::Circular
                         ? IconName::Circle
                         : IconName::Square,
                     renderedIndicator, mark, markSize, IconStyle::Filled);
        } else {
            drawIcon(context, IconName::Checkmark, renderedIndicator, mark,
                     markSize, IconStyle::Filled);
        }
    }
    if (!label_.empty()) {
        float drawnWidth = 0.0f;
        for (const auto& line : labelLines) drawnWidth = std::max(drawnWidth, line.width);
        const float labelX = labelPosition_ == CheckboxLabelPosition::After
            ? bounds().x + hitSize + current.spacing.horizontal.xs
            : bounds().x + std::max(0.0f, bounds().width - hitSize -
                                      current.spacing.horizontal.xs - requiredLabelWidth - drawnWidth);
        for (std::size_t index = 0; index < labelLines.size(); ++index) {
            const RectF lineBox{labelX, textTop + static_cast<float>(index) * labelStyle.lineHeight,
                                availableLabelWidth, labelStyle.lineHeight};
            context.drawText(labelLines[index].text, labelX,
                             context.centeredTextBottom(labelLines[index].text, lineBox,
                                                        labelStyle.size, labelStyle.weight),
                             labelStyle.size, text, labelStyle.weight, labelStyle.family);
        }
        if (required_) {
            const RectF lastLineBox{labelX,
                                    textTop + static_cast<float>(labelLines.size() - 1) * labelStyle.lineHeight,
                                    availableLabelWidth, labelStyle.lineHeight};
            context.drawText("*", labelX + labelLines.back().width + current.spacing.horizontal.xs,
                             context.centeredTextBottom("*", lastLineBox,
                                                        labelStyle.size, labelStyle.weight),
                             labelStyle.size, current.colors.statusDanger,
                             labelStyle.weight, labelStyle.family);
        }
    }
    clearDirty(DirtyFlag::Paint);
}

void Checkbox::toggle()
{
    // The native input clears `indeterminate` before dispatching change and
    // exposes currentTarget.checked=true. Match that Mixed -> Checked contract.
    const bool value = state() != CheckboxState::Checked;
    setCheckState(value ? CheckboxState::Checked : CheckboxState::Unchecked);
    if (onChange_) { onChange_(value); }
    if (onStateChange_) { onStateChange_(state()); }
}

bool Checkbox::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    switch (event.action) {
    case PointerAction::Down:
        if (event.button == MouseButton::Left) {
            setVisualState(ControlVisualState::Pressed, true);
            setVisualState(ControlVisualState::Focused, true);
            return true;
        }
        return false;
    case PointerAction::Up:
        if (event.button == MouseButton::Left) {
            const bool activate = (visualStates() & toMask(ControlVisualState::Pressed)) != 0 && bounds().contains(event.position);
            setVisualState(ControlVisualState::Pressed, false);
            if (activate) toggle();
            return true;
        }
        return false;
    case PointerAction::Enter: setVisualState(ControlVisualState::Hovered, true); return true;
    case PointerAction::Move: setVisualState(ControlVisualState::Hovered, bounds().contains(event.position)); return true;
    case PointerAction::Leave: setVisualState(ControlVisualState::Hovered, false); return true;
    case PointerAction::Cancel: setVisualState(ControlVisualState::Pressed, false); return true;
    default: return false;
    }
}

bool Checkbox::onKeyEvent(const KeyEvent& event)
{
    // Matches the native checkbox contract: Space toggles; Enter is left to
    // form submission/default-button handling and must not activate Checkbox.
    if (!isEnabled() || event.action != KeyAction::Down || event.keyCode != 32) return false;
    toggle();
    return true;
}

AccessibilityActionCapabilities Checkbox::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions;
    actions.toggle = true;
    return actions;
}

AccessibilityActionStatus Checkbox::performAccessibilityAction(
    AccessibilityActionKind kind, std::string_view value)
{
    (void)value;
    if (kind != AccessibilityActionKind::Toggle) return AccessibilityActionStatus::NotSupported;
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    toggle();
    return AccessibilityActionStatus::Succeeded;
}

} // namespace wui
