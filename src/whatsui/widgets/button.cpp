#include "wui/widgets.h"

#include <utility>

#include "wui/theme.h"

namespace wui {

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
    const auto textWidth = static_cast<float>(label_.size()) * 8.0f;
    return constraints.clamp({textWidth + 24.0f, 32.0f});
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
        background = current.colors.surfaceAlt;
        foreground = current.colors.text;
        break;
    case ButtonVariant::Primary:
    default:
        break;
    }
    if ((visualStates() & toMask(ControlVisualState::Pressed)) != 0) {
        background = scaleColor(background, 0.85f);
    } else if ((visualStates() & toMask(ControlVisualState::Hovered)) != 0) {
        background = scaleColor(background, 1.08f);
    }
    context.fillRoundRect(bounds(), current.radius.md, background);
    if (!label_.empty()) {
        context.drawText(label_, bounds().x + 12.0f, bounds().y + 20.0f, 14.0f, foreground);
    }
    ContainerNode::paint(context);
    clearDirty(DirtyFlag::Paint);
}

bool Button::onPointerEvent(const PointerEvent& event)
{
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
            const bool shouldClick = (visualStates() & toMask(ControlVisualState::Pressed)) != 0;
            setVisualState(ControlVisualState::Pressed, false);
            if (shouldClick && onClick_) {
                onClick_();
            }
            return true;
        }
        return false;
    case PointerAction::Enter:
    case PointerAction::Move:
        setVisualState(ControlVisualState::Hovered, true);
        return true;
    case PointerAction::Leave:
        setVisualState(ControlVisualState::Hovered, false);
        setVisualState(ControlVisualState::Pressed, false);
        return true;
    }

    return false;
}

} // namespace wui
