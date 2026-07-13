#include "wui/basic_controls.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "wui/theme.h"
#include "wui/theme_extensions.h"

namespace wui {
namespace {

constexpr float kLabelGap = 8.0f;
constexpr float kIndicatorSize = 18.0f;
constexpr float kSwitchWidth = 40.0f;
constexpr float kSwitchHeight = 20.0f;
constexpr float kSliderHeight = 32.0f;
constexpr float kProgressHeight = 4.0f;
constexpr float kSliderThumbSize = 16.0f;

[[nodiscard]] float labelWidth(const std::string& label, const Theme& current) noexcept
{
    return static_cast<float>(label.size()) * (current.typography.body * 0.56f);
}

[[nodiscard]] bool isPrimary(const PointerEvent& event) noexcept
{
    return event.button == MouseButton::Left;
}

[[nodiscard]] bool isActivationKey(const KeyEvent& event) noexcept
{
    return event.action == KeyAction::Down &&
        (event.keyCode == 32 || event.keyCode == 13 || event.keyCode == 257);
}

void updateCommonPointerState(ControlNode& control, const PointerEvent& event) noexcept
{
    switch (event.action) {
    case PointerAction::Enter:
        control.setVisualState(ControlVisualState::Hovered, true);
        break;
    case PointerAction::Move:
        control.setVisualState(ControlVisualState::Hovered, control.bounds().contains(event.position));
        break;
    case PointerAction::Leave:
        control.setVisualState(ControlVisualState::Hovered, false);
        break;
    case PointerAction::Cancel:
        control.setVisualState(ControlVisualState::Pressed, false);
        break;
    default:
        break;
    }
}

[[nodiscard]] Color interactionFill(const Theme& current, bool active, ControlVisualStates states) noexcept
{
    StateProperty<Color> fill(active ? current.colors.accent : current.colors.surface);
    if (active) {
        fill.set(ControlVisualState::Hovered, current.colors.accentHover)
            .set(ControlVisualState::Pressed, current.colors.accentPressed);
    } else {
        fill.set(ControlVisualState::Hovered, current.colors.surfaceHover)
            .set(ControlVisualState::Pressed, current.colors.surfacePressed);
    }
    return fill.resolve(states);
}

void drawFocusRing(PaintContext& context, const RectF& bounds, const Theme& current, bool focused)
{
    if (!focused) return;
    const float inset = current.controls.focusInset;
    context.fillRoundRect({bounds.x - inset, bounds.y - inset,
                           bounds.width + inset * 2.0f, bounds.height + inset * 2.0f},
                          current.radius.md + inset, current.colors.focus);
}

} // namespace

Radio::Radio(std::string label, bool selected)
    : label_(std::move(label)), selected_(selected) {}

const std::string& Radio::label() const noexcept { return label_; }
Radio& Radio::label(std::string value) { setLabel(std::move(value)); return *this; }
void Radio::setLabel(std::string value) { label_ = std::move(value); markDirty(DirtyFlag::Layout); }
bool Radio::isSelected() const noexcept { return hasBinding_ ? binding_->get() : selected_; }
Radio& Radio::selected(bool value) { setSelected(value); return *this; }
void Radio::setSelected(bool value)
{
    if (hasBinding_) binding_->set(value);
    else if (selected_ != value) { selected_ = value; markDirty(DirtyFlag::Paint); }
}
Radio& Radio::bind(State<bool>& state)
{
    binding_.emplace(state);
    hasBinding_ = true;
    selected_ = state.get();
    const auto id = state.subscribe([this](bool value) { selected_ = value; markDirty(DirtyFlag::Paint); });
    addTeardown([&state, id] { state.unsubscribe(id); });
    markDirty(DirtyFlag::Paint);
    return *this;
}
Radio& Radio::onChange(ChangeHandler handler) { onChange_ = std::move(handler); return *this; }
SizeF Radio::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    return constraints.clamp({kIndicatorSize + (label_.empty() ? 0.0f : kLabelGap + labelWidth(label_, current)), 24.0f});
}
void Radio::paint(PaintContext& context)
{
    const auto& current = theme();
    const bool enabled = isEnabled();
    const bool selected = isSelected();
    const RectF indicator{bounds().x, bounds().y + (bounds().height - kIndicatorSize) * 0.5f, kIndicatorSize, kIndicatorSize};
    drawFocusRing(context, indicator, current, (visualStates() & toMask(ControlVisualState::Focused)) != 0);
    Color border = selected ? current.colors.accent : current.colors.borderStrong;
    Color fill = interactionFill(current, selected, visualStates());
    Color text = current.colors.text;
    if (!enabled) { border = current.colors.border; fill = current.colors.disabled; text = current.colors.textDisabled; }
    context.fillRoundRect(indicator, current.radius.pill, border);
    context.fillRoundRect({indicator.x + 1.0f, indicator.y + 1.0f, indicator.width - 2.0f, indicator.height - 2.0f}, current.radius.pill, fill);
    if (selected) {
        const float dot = 8.0f;
        context.fillRoundRect({indicator.x + (indicator.width - dot) * 0.5f, indicator.y + (indicator.height - dot) * 0.5f, dot, dot}, current.radius.pill, current.colors.onAccent);
    }
    if (!label_.empty()) context.drawText(label_, indicator.x + indicator.width + kLabelGap,
                                          context.centeredTextBottom(label_, bounds(), current.typography.body),
                                          current.typography.body, text);
    clearDirty(DirtyFlag::Paint);
}
void Radio::select()
{
    if (!isSelected()) {
        setSelected(true);
        if (onChange_) onChange_(true);
    }
}
bool Radio::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    updateCommonPointerState(*this, event);
    if (event.action == PointerAction::Down && isPrimary(event)) { setVisualState(ControlVisualState::Pressed, true); return true; }
    if (event.action == PointerAction::Up && isPrimary(event)) {
        const bool activate = (visualStates() & toMask(ControlVisualState::Pressed)) != 0 && bounds().contains(event.position);
        setVisualState(ControlVisualState::Pressed, false);
        if (activate) select();
        return true;
    }
    return event.action == PointerAction::Enter || event.action == PointerAction::Move || event.action == PointerAction::Leave || event.action == PointerAction::Cancel;
}
bool Radio::onKeyEvent(const KeyEvent& event) { if (!isEnabled() || !isActivationKey(event)) return false; select(); return true; }

Switch::Switch(std::string label, bool on) : label_(std::move(label)), on_(on) {}
const std::string& Switch::label() const noexcept { return label_; }
Switch& Switch::label(std::string value) { setLabel(std::move(value)); return *this; }
void Switch::setLabel(std::string value) { label_ = std::move(value); markDirty(DirtyFlag::Layout); }
bool Switch::isOn() const noexcept { return hasBinding_ ? binding_->get() : on_; }
Switch& Switch::on(bool value) { setOn(value); return *this; }
void Switch::setOn(bool value)
{
    if (hasBinding_) binding_->set(value);
    else if (on_ != value) { on_ = value; markDirty(DirtyFlag::Paint); }
}
Switch& Switch::bind(State<bool>& state)
{
    binding_.emplace(state); hasBinding_ = true; on_ = state.get();
    const auto id = state.subscribe([this](bool value) { on_ = value; markDirty(DirtyFlag::Paint); });
    addTeardown([&state, id] { state.unsubscribe(id); }); markDirty(DirtyFlag::Paint); return *this;
}
Switch& Switch::onChange(ChangeHandler handler) { onChange_ = std::move(handler); return *this; }
SizeF Switch::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    return constraints.clamp({kSwitchWidth + (label_.empty() ? 0.0f : kLabelGap + labelWidth(label_, current)), 24.0f});
}
void Switch::paint(PaintContext& context)
{
    const auto& current = theme(); const bool enabled = isEnabled(); const bool on = isOn();
    const RectF track{bounds().x, bounds().y + (bounds().height - kSwitchHeight) * 0.5f, kSwitchWidth, kSwitchHeight};
    drawFocusRing(context, track, current, (visualStates() & toMask(ControlVisualState::Focused)) != 0);
    Color trackColor = interactionFill(current, on, visualStates());
    Color border = on ? current.colors.accent : current.colors.borderStrong;
    Color thumb = on ? current.colors.onAccent : current.colors.textMuted;
    Color text = current.colors.text;
    if (!enabled) { trackColor = current.colors.disabled; border = current.colors.border; thumb = current.colors.textDisabled; text = current.colors.textDisabled; }
    context.fillRoundRect(track, current.radius.pill, border);
    context.fillRoundRect({track.x + 1.0f, track.y + 1.0f, track.width - 2.0f, track.height - 2.0f}, current.radius.pill, trackColor);
    const float thumbSize = 14.0f;
    const float thumbX = on ? track.x + track.width - thumbSize - 3.0f : track.x + 3.0f;
    context.fillRoundRect({thumbX, track.y + (track.height - thumbSize) * 0.5f, thumbSize, thumbSize}, current.radius.pill, thumb);
    if (!label_.empty()) context.drawText(label_, track.x + track.width + kLabelGap,
                                          context.centeredTextBottom(label_, bounds(), current.typography.body),
                                          current.typography.body, text);
    clearDirty(DirtyFlag::Paint);
}
void Switch::toggle() { const bool value = !isOn(); setOn(value); if (onChange_) onChange_(value); }
bool Switch::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    updateCommonPointerState(*this, event);
    if (event.action == PointerAction::Down && isPrimary(event)) { setVisualState(ControlVisualState::Pressed, true); return true; }
    if (event.action == PointerAction::Up && isPrimary(event)) {
        const bool activate = (visualStates() & toMask(ControlVisualState::Pressed)) != 0 && bounds().contains(event.position);
        setVisualState(ControlVisualState::Pressed, false); if (activate) toggle(); return true;
    }
    return event.action == PointerAction::Enter || event.action == PointerAction::Move || event.action == PointerAction::Leave || event.action == PointerAction::Cancel;
}
bool Switch::onKeyEvent(const KeyEvent& event) { if (!isEnabled() || !isActivationKey(event)) return false; toggle(); return true; }

Slider::Slider(float minimum, float maximum, float value) { setRange(minimum, maximum); value_ = normalizedAndSnapped(value); }
float Slider::minimum() const noexcept { return minimum_; }
float Slider::maximum() const noexcept { return maximum_; }
void Slider::setRange(float minimum, float maximum)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum)) { minimum = 0.0f; maximum = 100.0f; }
    if (maximum < minimum) std::swap(minimum, maximum);
    minimum_ = minimum; maximum_ = maximum; setValue(value_); markDirty(DirtyFlag::Layout);
}
float Slider::value() const noexcept { return hasBinding_ ? binding_->get() : value_; }
Slider& Slider::value(float value) { setValue(value); return *this; }
void Slider::setValue(float value)
{
    const float next = normalizedAndSnapped(value);
    if (hasBinding_) binding_->set(next);
    else if (value_ != next) { value_ = next; markDirty(DirtyFlag::Paint); }
}
float Slider::step() const noexcept { return step_; }
Slider& Slider::step(float value) { setStep(value); return *this; }
void Slider::setStep(float value) { step_ = std::isfinite(value) && value > 0.0f ? value : 0.0f; setValue(value_); }
Slider& Slider::bind(State<float>& state)
{
    binding_.emplace(state); hasBinding_ = true; value_ = normalizedAndSnapped(state.get());
    const auto id = state.subscribe([this](float value) { value_ = normalizedAndSnapped(value); markDirty(DirtyFlag::Paint); });
    addTeardown([&state, id] { state.unsubscribe(id); }); setValue(value_); return *this;
}
Slider& Slider::onChange(ChangeHandler handler) { onChange_ = std::move(handler); return *this; }
SizeF Slider::measure(const Constraints& constraints) const { return constraints.clamp({160.0f, kSliderHeight}); }
float Slider::normalizedValue() const noexcept { const float span = maximum_ - minimum_; return span > 0.0f ? std::clamp((value() - minimum_) / span, 0.0f, 1.0f) : 0.0f; }
float Slider::normalizedAndSnapped(float value) const noexcept
{
    if (!std::isfinite(value)) value = minimum_;
    value = std::clamp(value, minimum_, maximum_);
    if (step_ > 0.0f) value = minimum_ + std::round((value - minimum_) / step_) * step_;
    return std::clamp(value, minimum_, maximum_);
}
void Slider::setValueFromPointer(float x)
{
    const float usable = std::max(1.0f, bounds().width - kSliderThumbSize);
    setValue(minimum_ + std::clamp((x - bounds().x - kSliderThumbSize * 0.5f) / usable, 0.0f, 1.0f) * (maximum_ - minimum_));
    if (onChange_) onChange_(value());
}
void Slider::paint(PaintContext& context)
{
    const auto& current = theme(); const bool enabled = isEnabled();
    const float trackY = bounds().y + (bounds().height - 4.0f) * 0.5f;
    const RectF track{bounds().x + kSliderThumbSize * 0.5f, trackY, std::max(0.0f, bounds().width - kSliderThumbSize), 4.0f};
    const float fraction = normalizedValue(); const float thumbX = track.x + track.width * fraction;
    drawFocusRing(context, {bounds().x, bounds().y + 2.0f, bounds().width, bounds().height - 4.0f}, current,
                  (visualStates() & toMask(ControlVisualState::Focused)) != 0);
    StateProperty<Color> interactionColor{current.colors.accent};
    interactionColor.set(ControlVisualState::Hovered, current.colors.accentHover)
        .set(ControlVisualState::Pressed, current.colors.accentPressed);
    Color fill = interactionColor.resolve(visualStates());
    Color thumb = fill;
    if (!enabled) { fill = current.colors.textDisabled; thumb = current.colors.textDisabled; }
    context.fillRoundRect(track, current.radius.pill, current.colors.border);
    context.fillRoundRect({track.x, track.y, track.width * fraction, track.height}, current.radius.pill, fill);
    const float thumbSize = (visualStates() & toMask(ControlVisualState::Pressed)) != 0 ? kSliderThumbSize + 2.0f : kSliderThumbSize;
    context.fillRoundRect({thumbX - thumbSize * 0.5f, bounds().y + (bounds().height - thumbSize) * 0.5f, thumbSize, thumbSize}, current.radius.pill, thumb);
    clearDirty(DirtyFlag::Paint);
}
bool Slider::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    updateCommonPointerState(*this, event);
    if (event.action == PointerAction::Down && isPrimary(event)) { setVisualState(ControlVisualState::Pressed, true); setValueFromPointer(event.position.x); return true; }
    if (event.action == PointerAction::Move && (visualStates() & toMask(ControlVisualState::Pressed)) != 0) { setValueFromPointer(event.position.x); return true; }
    if (event.action == PointerAction::Up && isPrimary(event)) { if ((visualStates() & toMask(ControlVisualState::Pressed)) != 0) setValueFromPointer(event.position.x); setVisualState(ControlVisualState::Pressed, false); return true; }
    return event.action == PointerAction::Enter || event.action == PointerAction::Move || event.action == PointerAction::Leave || event.action == PointerAction::Cancel;
}
bool Slider::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || event.action != KeyAction::Down) return false;
    float next = value(); const float increment = step_ > 0.0f ? step_ : std::max((maximum_ - minimum_) / 100.0f, 1.0f);
    switch (event.keyCode) {
    case 37: case 40: next -= increment; break;
    case 38: case 39: next += increment; break;
    case 36: next = minimum_; break;
    case 35: next = maximum_; break;
    default: return false;
    }
    const float before = value(); setValue(next); if (value() != before && onChange_) onChange_(value()); return true;
}

ProgressBar::ProgressBar(float minimum, float maximum, float value) { setRange(minimum, maximum); value_ = clampedValue(value); }
float ProgressBar::minimum() const noexcept { return minimum_; }
float ProgressBar::maximum() const noexcept { return maximum_; }
void ProgressBar::setRange(float minimum, float maximum)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum)) { minimum = 0.0f; maximum = 100.0f; }
    if (maximum < minimum) std::swap(minimum, maximum); minimum_ = minimum; maximum_ = maximum; setValue(value_); markDirty(DirtyFlag::Layout);
}
float ProgressBar::value() const noexcept { return hasBinding_ ? binding_->get() : value_; }
ProgressBar& ProgressBar::value(float value) { setValue(value); return *this; }
void ProgressBar::setValue(float value)
{
    const float next = clampedValue(value); if (hasBinding_) binding_->set(next); else if (value_ != next) { value_ = next; markDirty(DirtyFlag::Paint); }
}
ProgressBar& ProgressBar::bind(State<float>& state)
{
    binding_.emplace(state); hasBinding_ = true; value_ = clampedValue(state.get());
    const auto id = state.subscribe([this](float value) { value_ = clampedValue(value); markDirty(DirtyFlag::Paint); });
    addTeardown([&state, id] { state.unsubscribe(id); }); setValue(value_); return *this;
}
SizeF ProgressBar::measure(const Constraints& constraints) const { return constraints.clamp({160.0f, kProgressHeight}); }
float ProgressBar::normalizedValue() const noexcept { const float span = maximum_ - minimum_; return span > 0.0f ? std::clamp((value() - minimum_) / span, 0.0f, 1.0f) : 0.0f; }
float ProgressBar::clampedValue(float value) const noexcept { return std::clamp(std::isfinite(value) ? value : minimum_, minimum_, maximum_); }
void ProgressBar::paint(PaintContext& context)
{
    const auto& current = theme(); const RectF track{bounds().x, bounds().y + (bounds().height - kProgressHeight) * 0.5f, bounds().width, kProgressHeight};
    context.fillRoundRect(track, current.radius.pill, current.colors.border);
    context.fillRoundRect({track.x, track.y, track.width * normalizedValue(), track.height}, current.radius.pill, current.colors.accent);
    clearDirty(DirtyFlag::Paint);
}

Divider::Divider(DividerOrientation orientation) : orientation_(orientation) {}
DividerOrientation Divider::orientation() const noexcept { return orientation_; }
void Divider::setOrientation(DividerOrientation orientation) noexcept { orientation_ = orientation; markDirty(DirtyFlag::Layout); }
float Divider::thickness() const noexcept { return thickness_; }
void Divider::setThickness(float thickness) noexcept { thickness_ = std::max(1.0f, std::isfinite(thickness) ? thickness : 1.0f); markDirty(DirtyFlag::Layout); }
SizeF Divider::measure(const Constraints& constraints) const
{
    return orientation_ == DividerOrientation::Horizontal ? constraints.clamp({0.0f, thickness_}) : constraints.clamp({thickness_, 0.0f});
}
void Divider::paint(PaintContext& context)
{
    const auto& current = theme();
    const RectF line = orientation_ == DividerOrientation::Horizontal
        ? RectF{bounds().x, bounds().y + (bounds().height - thickness_) * 0.5f, bounds().width, thickness_}
        : RectF{bounds().x + (bounds().width - thickness_) * 0.5f, bounds().y, thickness_, bounds().height};
    context.fillRect(line, current.colors.border); clearDirty(DirtyFlag::Paint);
}

} // namespace wui
