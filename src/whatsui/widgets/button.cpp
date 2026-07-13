#include "wui/widgets.h"

#include <utility>

#include "wui/theme.h"

namespace wui {
namespace {

void drawFocusRing(PaintContext& context, const RectF& bounds, const Theme& current, bool focused)
{
    if (!focused) return;
    const float inset = current.controls.focusInset;
    context.fillRoundRect({bounds.x - inset, bounds.y - inset,
                           bounds.width + inset * 2.0f, bounds.height + inset * 2.0f},
                          current.radius.md + inset, current.colors.focus);
}

} // namespace

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
    markDirty(DirtyFlag::Paint);
}

ButtonVariant Button::variant() const noexcept
{
    return variant_;
}

SizeF Button::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    const auto textWidth = static_cast<float>(label_.size()) * (current.typography.body * 0.56f);
    return constraints.clamp({textWidth + current.controls.horizontalPadding * 2.0f, current.controls.height});
}

void Button::paint(PaintContext& context)
{
    const Theme& current = theme();
    Color background = current.colors.accent;
    Color foreground = current.colors.onAccent;
    switch (variant_) {
    case ButtonVariant::Danger:
        background = current.colors.danger;
        break;
    case ButtonVariant::Ghost:
        background = current.colors.surface;
        foreground = current.colors.text;
        break;
    case ButtonVariant::Primary:
    default:
        break;
    }
    const bool ghost = variant_ == ButtonVariant::Ghost;
    const bool disabled = !isEnabled();
    const bool focused = !disabled && (visualStates() & toMask(ControlVisualState::Focused)) != 0;
    if (disabled) {
        background = current.colors.disabled;
        foreground = current.colors.textDisabled;
    } else if ((visualStates() & toMask(ControlVisualState::Pressed)) != 0) {
        background = ghost ? current.colors.surfacePressed : (variant_ == ButtonVariant::Danger ? scaleColor(current.colors.danger, 0.84f) : current.colors.accentPressed);
    } else if ((visualStates() & toMask(ControlVisualState::Hovered)) != 0) {
        background = ghost ? current.colors.surfaceHover : (variant_ == ButtonVariant::Danger ? scaleColor(current.colors.danger, 0.92f) : current.colors.accentHover);
    }
    // A two-pass rounded rectangle provides a crisp one-pixel Fluent-style
    // stroke without exposing backend-specific stroke APIs.
    drawFocusRing(context, bounds(), current, focused);
    if (ghost && !disabled) context.fillRoundRect(bounds(), current.radius.md, current.colors.border);
    context.fillRoundRect(bounds(), current.radius.md, background);
    if (!label_.empty()) {
        context.drawText(label_, bounds().x + current.controls.horizontalPadding,
                         context.centeredTextBottom(label_, bounds(), current.typography.body),
                         current.typography.body, foreground);
    }
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

Checkbox::Checkbox(std::string label, bool checked)
    : label_(std::move(label))
    , checked_(checked)
{
}

const std::string& Checkbox::label() const noexcept { return label_; }
Checkbox& Checkbox::label(std::string label) { setLabel(std::move(label)); return *this; }
void Checkbox::setLabel(std::string label) { label_ = std::move(label); markDirty(DirtyFlag::Layout); }
bool Checkbox::isChecked() const noexcept { return hasBinding_ ? binding_->get() : checked_; }
Checkbox& Checkbox::checked(bool value) { setChecked(value); return *this; }
void Checkbox::setChecked(bool value)
{
    if (hasBinding_) {
        binding_->set(value);
    } else if (checked_ != value) {
        checked_ = value;
        markDirty(DirtyFlag::Paint);
    }
}

Checkbox& Checkbox::bind(State<bool>& state)
{
    binding_.emplace(state);
    hasBinding_ = true;
    checked_ = state.get();
    const auto id = state.subscribe([this](bool value) {
        checked_ = value;
        markDirty(DirtyFlag::Paint);
    });
    addTeardown([&state, id] { state.unsubscribe(id); });
    markDirty(DirtyFlag::Paint);
    return *this;
}

Checkbox& Checkbox::onChange(ChangeHandler handler) { onChange_ = std::move(handler); return *this; }
SizeF Checkbox::measure(const Constraints& constraints) const
{
    return constraints.clamp({20.0f + (label_.empty() ? 0.0f : 8.0f + static_cast<float>(label_.size()) * 8.0f), 24.0f});
}

void Checkbox::paint(PaintContext& context)
{
    const Theme& current = theme();
    const bool enabled = isEnabled();
    const bool focused = enabled && (visualStates() & toMask(ControlVisualState::Focused)) != 0;
    Color box = isChecked() ? current.colors.accent : current.colors.surface;
    Color border = isChecked() ? current.colors.accent : current.colors.borderStrong;
    Color text = current.colors.text;
    if (!enabled) {
        box = current.colors.disabled;
        border = current.colors.border;
        text = current.colors.textMuted;
    } else if ((visualStates() & toMask(ControlVisualState::Pressed)) != 0) {
        box = isChecked() ? current.colors.accentPressed : current.colors.surfacePressed;
    } else if ((visualStates() & toMask(ControlVisualState::Hovered)) != 0) {
        box = isChecked() ? current.colors.accentHover : current.colors.surfaceHover;
    }
    const float indicatorSize = current.controls.checkboxSize;
    const RectF indicator{bounds().x, bounds().y + (bounds().height - indicatorSize) / 2.0f, indicatorSize, indicatorSize};
    drawFocusRing(context, bounds(), current, focused);
    context.fillRoundRect(indicator, current.radius.sm, border);
    context.fillRoundRect({indicator.x + 1.0f, indicator.y + 1.0f, indicatorSize - 2.0f, indicatorSize - 2.0f}, current.radius.sm, box);
    if (isChecked()) {
        // A geometric checkmark keeps this compact control independent of the
        // text glyph atlas. That matters when a completed ForEach branch is
        // mounted mid-frame: the Software renderer batches text after widget
        // paint, whereas the selection indicator must be immediately stable.
        const Color mark = current.colors.onAccent;
        context.fillRect({indicator.x + 4.0f, indicator.y + 10.0f, 3.0f, 1.0f}, mark);
        context.fillRect({indicator.x + 6.0f, indicator.y + 9.0f, 2.0f, 2.0f}, mark);
        context.fillRect({indicator.x + 8.0f, indicator.y + 7.0f, 2.0f, 3.0f}, mark);
        context.fillRect({indicator.x + 9.0f, indicator.y + 6.0f, 6.0f, 2.0f}, mark);
    }
    if (!label_.empty()) {
        context.drawText(label_, bounds().x + indicatorSize + current.spacing.sm,
                         context.centeredTextBottom(label_, bounds(), current.typography.body),
                         current.typography.body, text);
    }
    clearDirty(DirtyFlag::Paint);
}

void Checkbox::toggle()
{
    const bool value = !isChecked();
    setChecked(value);
    if (onChange_) { onChange_(value); }
}

bool Checkbox::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    switch (event.action) {
    case PointerAction::Down:
        if (event.button == MouseButton::Left) { setVisualState(ControlVisualState::Pressed, true); return true; }
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
    if (!isEnabled() || event.action != KeyAction::Down || (event.keyCode != 32 && event.keyCode != 13)) return false;
    toggle();
    return true;
}

} // namespace wui
