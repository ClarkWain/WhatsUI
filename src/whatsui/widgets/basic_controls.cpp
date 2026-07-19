#include "wui/basic_controls.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <memory>
#include <utility>

#include "wui/animation.h"
#include "wui/runtime.h"
#include "wui/text_metrics.h"
#include "wui/theme.h"
#include "wui/theme_extensions.h"

namespace wui {
namespace {

constexpr float kLabelGap = 8.0f;
constexpr float kIndicatorSize = 16.0f;
constexpr float kRadioHeight = 32.0f;
constexpr float kGroupGap = 4.0f;
constexpr float kHorizontalGroupGap = 12.0f;
constexpr float kDividerLabelGap = 12.0f;

[[nodiscard]] float switchWidth(SwitchSize size) noexcept
{
    return size == SwitchSize::Small ? 32.0f : 40.0f;
}

[[nodiscard]] float switchHeight(SwitchSize size) noexcept
{
    return size == SwitchSize::Small ? 16.0f : 20.0f;
}

[[nodiscard]] float switchControlHeight(SwitchSize size) noexcept
{
    return size == SwitchSize::Small ? 32.0f : 36.0f;
}

[[nodiscard]] float sliderThumbSize(SliderSize size) noexcept
{
    return size == SliderSize::Small ? 16.0f : 20.0f;
}

[[nodiscard]] float sliderTrackSize(SliderSize size) noexcept
{
    return size == SliderSize::Small ? 2.0f : 4.0f;
}

[[nodiscard]] float sliderCrossAxisSize(SliderSize size) noexcept
{
    return size == SliderSize::Small ? 24.0f : 32.0f;
}

[[nodiscard]] float progressHeight(ProgressBarThickness thickness) noexcept
{
    // Fluent's default medium bar is intentionally hairline-light; large is
    // the 4px emphasis variant.
    return thickness == ProgressBarThickness::Large ? 4.0f : 2.0f;
}

[[nodiscard]] float labelWidth(const std::string& label, const Theme& current) noexcept
{
    if (const auto* measurer = textMeasurer()) {
        return measurer
            ->measureText(label, current.typography.body1.size,
                          current.typography.body1.weight)
            .width;
    }
    return static_cast<float>(label.size()) *
           (current.typography.body1.size * 0.56f);
}

[[nodiscard]] bool isPrimary(const PointerEvent& event) noexcept
{
    return event.button == MouseButton::Left;
}

[[nodiscard]] bool isActivationKey(const KeyEvent& event) noexcept
{
    return event.action == KeyAction::Down && event.keyCode == 32;
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
    const ColorTokens::Interaction& ramp = active ? current.colors.brandBackground
                                                   : current.colors.neutralBackground1;
    StateProperty<Color> fill(ramp.rest);
    fill.set(ControlVisualState::Hovered, ramp.hover)
        .set(ControlVisualState::Pressed, ramp.pressed);
    return fill.resolve(states);
}

void drawFocusRing(PaintContext& context, const RectF& bounds, const Theme& current,
                   bool focused, float radius = -1.0f)
{
    if (!focused) return;
    if (radius < 0.0f) radius = current.radius.medium;
    const float inset = current.controls.focusInset;
    // A Fluent focus indicator is two *strokes*, not two opaque rounded
    // rectangles. Filling the inner layer makes the ring appear as a muddy
    // halo and can obscure the control's own disabled/selected treatment.
    context.strokeRoundRect({bounds.x - inset, bounds.y - inset,
                             bounds.width + inset * 2.0f, bounds.height + inset * 2.0f},
                            radius + inset, current.stroke.thick, current.colors.strokeFocusOuter);
    const float innerInset = std::max(0.0f, inset - current.stroke.thin * 0.5f);
    context.strokeRoundRect({bounds.x - innerInset, bounds.y - innerInset,
                             bounds.width + innerInset * 2.0f, bounds.height + innerInset * 2.0f},
                            radius + innerInset, current.stroke.thin, current.colors.strokeFocusInner);
}

[[nodiscard]] bool parseFloat(std::string_view text, float& result) noexcept
{
    if (text.empty()) return false;
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const auto parsed = std::from_chars(first, last, result);
    return parsed.ec == std::errc{} && parsed.ptr == last && std::isfinite(result);
}

} // namespace

Radio::Radio(std::string label, bool selected)
    : label_(std::move(label)), value_(label_), selected_(selected) {}

const std::string& Radio::label() const noexcept { return label_; }
Radio& Radio::label(std::string value) { setLabel(std::move(value)); return *this; }
void Radio::setLabel(std::string value) { label_ = std::move(value); markDirty(DirtyFlag::Layout); }
const std::string& Radio::value() const noexcept { return value_; }
Radio& Radio::value(std::string value) { setValue(std::move(value)); return *this; }
void Radio::setValue(std::string value) { value_ = std::move(value); markDirty(DirtyFlag::Layout); }
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
    subscription_.subscribe(state, [this](bool value) { selected_ = value; markDirty(DirtyFlag::Paint); });
    markDirty(DirtyFlag::Paint);
    return *this;
}
Radio& Radio::onChange(ChangeHandler handler) { onChange_ = std::move(handler); return *this; }
SizeF Radio::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    if (stackedLabel_ && !label_.empty()) {
        return constraints.clamp({std::max(kIndicatorSize, labelWidth(label_, current)),
                                  kIndicatorSize + current.spacing.vertical.xs + current.typography.body1.lineHeight});
    }
    return constraints.clamp({kIndicatorSize + (label_.empty() ? 0.0f : kLabelGap + labelWidth(label_, current)),
                              kRadioHeight});
}
void Radio::paint(PaintContext& context)
{
    const auto& current = theme();
    const bool enabled = isEnabled();
    const bool selected = isSelected();
    const RectF indicator{bounds().x, bounds().y + (bounds().height - kIndicatorSize) * 0.5f, kIndicatorSize, kIndicatorSize};
    drawFocusRing(context, indicator, current,
                  (visualStates() & toMask(ControlVisualState::Focused)) != 0,
                  current.radius.circular);
    StateProperty<Color> selectedFill{current.colors.brandBackground.rest};
    selectedFill.set(ControlVisualState::Hovered, current.colors.brandBackground.hover)
        .set(ControlVisualState::Pressed, current.colors.brandBackground.pressed);
    StateProperty<Color> unselectedBorder{current.colors.neutralStrokeAccessible};
    unselectedBorder.set(ControlVisualState::Hovered, current.colors.neutralStrokeAccessibleHover)
        .set(ControlVisualState::Pressed, current.colors.neutralStrokeAccessiblePressed);
    StateProperty<Color> unselectedFill{current.colors.neutralBackground1.rest};
    unselectedFill.set(ControlVisualState::Hovered, current.colors.neutralBackground1.hover)
        .set(ControlVisualState::Pressed, current.colors.neutralBackground1.pressed);
    Color border = selected ? selectedFill.resolve(visualStates()) : unselectedBorder.resolve(visualStates());
    Color fill = selected ? border : unselectedFill.resolve(visualStates());
    Color text = current.colors.neutralForeground1;
    if (!enabled) {
        border = selected ? current.colors.neutralForegroundDisabled : current.colors.neutralStrokeDisabled;
        fill = selected ? border : current.colors.neutralBackground1.rest;
        text = current.colors.neutralForegroundDisabled;
    }
    context.fillStrokeRoundRect(indicator, current.radius.circular,
                                current.stroke.thin, fill, border);
    if (selected) {
        // Selected Fluent radios are a brand disc with a white centre dot.
        // Keeping the brand in the centre (the old implementation) made the
        // selected state read as an unchecked ring at compact sizes.
        const float dot = 6.0f;
        context.fillRoundRect({indicator.x + (indicator.width - dot) * 0.5f,
                               indicator.y + (indicator.height - dot) * 0.5f, dot, dot},
                              current.radius.circular, current.colors.onBrand);
    }
    if (!label_.empty()) {
        if (stackedLabel_) {
            const RectF labelBox{bounds().x, indicator.y + indicator.height + current.spacing.vertical.xs,
                                 bounds().width, current.typography.body1.lineHeight};
            const float x = labelBox.x + std::max(0.0f, (labelBox.width - labelWidth(label_, current)) * 0.5f);
            context.drawText(label_, x,
                             context.centeredTextBottom(
                                 label_, labelBox,
                                 current.typography.body1.size,
                                 current.typography.body1.weight,
                                 current.typography.body1.family),
                             current.typography.body1.size, text,
                             current.typography.body1.weight,
                             current.typography.body1.family);
        } else {
            context.drawText(label_, indicator.x + indicator.width + kLabelGap,
                             context.centeredTextBottom(
                                 label_, bounds(),
                                 current.typography.body1.size,
                                 current.typography.body1.weight,
                                 current.typography.body1.family),
                             current.typography.body1.size, text,
                             current.typography.body1.weight,
                             current.typography.body1.family);
        }
    }
    clearDirty(DirtyFlag::Paint);
}
void Radio::setSelectedFromGroup(bool value)
{
    if (hasBinding_) binding_->set(value);
    else if (selected_ != value) { selected_ = value; markDirty(DirtyFlag::Paint); }
}
void Radio::setStackedLabel(bool value) noexcept
{
    if (stackedLabel_ != value) { stackedLabel_ = value; markDirty(DirtyFlag::Layout); }
}
void Radio::select()
{
    if (auto* group = dynamic_cast<RadioGroup*>(parent())) {
        group->selectRadio(*this);
        return;
    }
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
bool Radio::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || event.action != KeyAction::Down) return false;
    if (auto* group = dynamic_cast<RadioGroup*>(parent())) {
        if (event.keyCode == 37 || event.keyCode == 38) return group->moveSelection(*this, -1);
        if (event.keyCode == 39 || event.keyCode == 40) return group->moveSelection(*this, 1);
    }
    if (!isActivationKey(event)) return false;
    select();
    return true;
}
AccessibilityActionCapabilities Radio::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions;
    actions.toggle = true;
    return actions;
}
AccessibilityActionStatus Radio::performAccessibilityAction(AccessibilityActionKind kind,
                                                              std::string_view actionValue)
{
    (void)actionValue;
    if (kind != AccessibilityActionKind::Toggle) return AccessibilityActionStatus::NotSupported;
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    select();
    return AccessibilityActionStatus::Succeeded;
}

Radio& RadioGroup::addOption(std::string value, std::string label, bool enabled)
{
    auto option = std::make_unique<Radio>(std::move(label));
    option->setValue(std::move(value));
    option->setStackedLabel(layout_ == RadioGroupLayout::HorizontalStacked);
    option->optionEnabled_ = enabled;
    option->setEnabled(enabled && isEnabled());
    auto* result = option.get();
    appendChild(std::move(option));
    if (result->value() == this->value()) result->setSelectedFromGroup(true);
    markDirty(DirtyFlag::Layout);
    return *result;
}
const std::string& RadioGroup::name() const noexcept { return name_; }
RadioGroup& RadioGroup::name(std::string value) { setName(std::move(value)); return *this; }
void RadioGroup::setName(std::string value)
{
    if (name_ != value) { name_ = std::move(value); markDirty(DirtyFlag::Style); }
}
const std::string& RadioGroup::accessibleLabel() const noexcept { return accessibleLabel_; }
RadioGroup& RadioGroup::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void RadioGroup::setAccessibleLabel(std::string value)
{
    if (accessibleLabel_ != value) { accessibleLabel_ = std::move(value); markDirty(DirtyFlag::Style); }
}
const std::string& RadioGroup::value() const noexcept
{
    return hasBinding_ ? binding_->get() : value_;
}
RadioGroup& RadioGroup::value(std::string value) { setValue(std::move(value)); return *this; }
void RadioGroup::setValue(std::string value) { applyValue(value, false); }
Radio* RadioGroup::selectedRadio() noexcept
{
    for (const auto& child : children()) {
        if (auto* radio = dynamic_cast<Radio*>(child.get()); radio && radio->isSelected()) {
            return radio;
        }
    }
    return nullptr;
}
const Radio* RadioGroup::selectedRadio() const noexcept
{
    for (const auto& child : children()) {
        if (const auto* radio = dynamic_cast<const Radio*>(child.get()); radio && radio->isSelected()) {
            return radio;
        }
    }
    return nullptr;
}
RadioGroup& RadioGroup::bind(State<std::string>& state)
{
    binding_.emplace(state);
    hasBinding_ = true;
    value_ = state.get();
    subscription_.subscribe(state, [this](const std::string& value) { applyValue(value, false); });
    applyValue(value_, false);
    return *this;
}
RadioGroup& RadioGroup::onChange(ChangeHandler handler) { onChange_ = std::move(handler); return *this; }
RadioGroupLayout RadioGroup::groupLayout() const noexcept { return layout_; }
RadioGroup& RadioGroup::groupLayout(RadioGroupLayout value) noexcept { setGroupLayout(value); return *this; }
void RadioGroup::setGroupLayout(RadioGroupLayout value) noexcept
{
    if (layout_ == value) return;
    layout_ = value;
    const bool stacked = layout_ == RadioGroupLayout::HorizontalStacked;
    for (const auto& child : children()) {
        if (auto* radio = dynamic_cast<Radio*>(child.get())) radio->setStackedLabel(stacked);
    }
    markDirty(DirtyFlag::Layout);
}
bool RadioGroup::isRequired() const noexcept { return required_; }
RadioGroup& RadioGroup::required(bool value) noexcept { setRequired(value); return *this; }
void RadioGroup::setRequired(bool value) noexcept
{
    if (required_ != value) { required_ = value; markDirty(DirtyFlag::Style); }
}
void RadioGroup::setEnabled(bool enabled) noexcept
{
    ControlNode::setEnabled(enabled);
    syncChildStates();
}
SizeF RadioGroup::measure(const Constraints& constraints) const
{
    float width = 0.0f;
    float height = 0.0f;
    std::size_t count = 0;
    const bool horizontal = layout_ != RadioGroupLayout::Vertical;
    for (const auto& child : children()) {
        const auto* radio = dynamic_cast<const Radio*>(child.get());
        if (!radio) continue;
        const SizeF item = radio->measure({0.0f, constraints.maxWidth, 0.0f, constraints.maxHeight});
        if (horizontal) {
            width += item.width;
            height = std::max(height, item.height);
        } else {
            width = std::max(width, item.width);
            height += item.height;
        }
        ++count;
    }
    if (count > 1) {
        if (horizontal) width += kHorizontalGroupGap * static_cast<float>(count - 1);
        else height += kGroupGap * static_cast<float>(count - 1);
    }
    return constraints.clamp({width, height});
}
void RadioGroup::layout(const RectF& bounds)
{
    Node::layout(bounds);
    syncChildStates();
    const bool horizontal = layout_ != RadioGroupLayout::Vertical;
    float cursor = horizontal ? bounds.x : bounds.y;
    for (const auto& child : children()) {
        auto* radio = dynamic_cast<Radio*>(child.get());
        if (!radio) continue;
        radio->setStackedLabel(layout_ == RadioGroupLayout::HorizontalStacked);
        const SizeF item = radio->measure({0.0f, bounds.width, 0.0f, bounds.height});
        if (horizontal) {
            radio->layout({cursor, bounds.y, item.width, bounds.height});
            cursor += item.width + kHorizontalGroupGap;
        } else {
            radio->layout({bounds.x, cursor, bounds.width, item.height});
            cursor += item.height + kGroupGap;
        }
    }
    clearDirty(DirtyFlag::Layout);
}
void RadioGroup::paint(PaintContext& context)
{
    ContainerNode::paint(context);
    clearDirty(DirtyFlag::Paint);
}
void RadioGroup::selectRadio(Radio& radio)
{
    if (!isEnabled() || !radio.isEnabled()) return;
    const bool changed = value() != radio.value();
    applyValue(radio.value(), true);
    if (changed && radio.onChange_) radio.onChange_(true);
}
bool RadioGroup::moveSelection(Radio& from, int delta)
{
    if (!isEnabled() || delta == 0) return false;
    std::vector<Radio*> options;
    for (const auto& child : children()) {
        if (auto* radio = dynamic_cast<Radio*>(child.get()); radio && radio->isEnabled()) options.push_back(radio);
    }
    if (options.empty()) return false;
    const auto it = std::find(options.begin(), options.end(), &from);
    std::ptrdiff_t index = it == options.end() ? 0 : std::distance(options.begin(), it);
    const std::ptrdiff_t count = static_cast<std::ptrdiff_t>(options.size());
    index = (index + (delta < 0 ? -1 : 1) + count) % count;
    selectRadio(*options[static_cast<std::size_t>(index)]);
    return true;
}
void RadioGroup::applyValue(const std::string& value, bool notify)
{
    const bool changed = this->value() != value;
    if (hasBinding_ && binding_->get() != value) binding_->set(value);
    value_ = value;
    for (const auto& child : children()) {
        if (auto* radio = dynamic_cast<Radio*>(child.get())) {
            radio->setSelectedFromGroup(radio->value() == value);
        }
    }
    markDirty(DirtyFlag::Paint);
    if (notify && changed && onChange_) onChange_(value);
}
void RadioGroup::syncChildStates() noexcept
{
    for (const auto& child : children()) {
        if (auto* radio = dynamic_cast<Radio*>(child.get())) {
            radio->ControlNode::setEnabled(isEnabled() && radio->optionEnabled_);
        }
    }
}

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
    subscription_.subscribe(state, [this](bool value) { on_ = value; markDirty(DirtyFlag::Paint); });
    markDirty(DirtyFlag::Paint); return *this;
}
Switch& Switch::onChange(ChangeHandler handler) { onChange_ = std::move(handler); return *this; }
SwitchSize Switch::size() const noexcept { return size_; }
Switch& Switch::size(SwitchSize value) noexcept { setSize(value); return *this; }
void Switch::setSize(SwitchSize value) noexcept
{
    if (size_ != value) { size_ = value; markDirty(DirtyFlag::Layout); }
}
SwitchLabelPosition Switch::labelPosition() const noexcept { return labelPosition_; }
Switch& Switch::labelPosition(SwitchLabelPosition value) noexcept { setLabelPosition(value); return *this; }
void Switch::setLabelPosition(SwitchLabelPosition value) noexcept
{
    if (labelPosition_ != value) { labelPosition_ = value; markDirty(DirtyFlag::Layout); }
}
bool Switch::isRequired() const noexcept { return required_; }
Switch& Switch::required(bool value) noexcept { setRequired(value); return *this; }
void Switch::setRequired(bool value) noexcept
{
    if (required_ != value) { required_ = value; markDirty(DirtyFlag::Layout); }
}
SizeF Switch::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    const std::string displayLabel = required_ && !label_.empty() ? label_ + " *" : label_;
    const float trackWidth = switchWidth(size_);
    const float controlHeight = switchControlHeight(size_);
    if (displayLabel.empty()) return constraints.clamp({trackWidth, controlHeight});
    if (labelPosition_ == SwitchLabelPosition::Above) {
        return constraints.clamp({std::max(trackWidth, labelWidth(displayLabel, current)),
                                  controlHeight + current.typography.body1.lineHeight});
    }
    return constraints.clamp({trackWidth + kLabelGap + labelWidth(displayLabel, current), controlHeight});
}
void Switch::paint(PaintContext& context)
{
    const auto& current = theme(); const bool enabled = isEnabled(); const bool on = isOn();
    const std::string displayLabel = required_ && !label_.empty() ? label_ + " *" : label_;
    const float trackWidth = switchWidth(size_);
    const float trackHeight = switchHeight(size_);
    float trackX = bounds().x;
    float trackY = bounds().y + (bounds().height - trackHeight) * 0.5f;
    RectF labelBox = bounds();
    if (!displayLabel.empty() && labelPosition_ == SwitchLabelPosition::Before) {
        trackX = bounds().x + labelWidth(displayLabel, current) + kLabelGap;
        labelBox = {bounds().x, bounds().y, labelWidth(displayLabel, current), bounds().height};
    } else if (!displayLabel.empty() && labelPosition_ == SwitchLabelPosition::Above) {
        labelBox = {bounds().x, bounds().y, bounds().width, current.typography.body1.lineHeight};
        trackY = bounds().y + current.typography.body1.lineHeight +
                 (bounds().height - current.typography.body1.lineHeight - trackHeight) * 0.5f;
    } else if (!displayLabel.empty()) {
        labelBox = {trackX + trackWidth + kLabelGap, bounds().y,
                    std::max(0.0f, bounds().width - trackWidth - kLabelGap), bounds().height};
    }
    const RectF track{trackX, trackY, trackWidth, trackHeight};
    drawFocusRing(context, track, current,
                  (visualStates() & toMask(ControlVisualState::Focused)) != 0,
                  current.radius.circular);
    StateProperty<Color> activeTrack{current.colors.brandBackground.rest};
    activeTrack.set(ControlVisualState::Hovered, current.colors.brandBackground.hover)
        .set(ControlVisualState::Pressed, current.colors.brandBackground.pressed);
    StateProperty<Color> inactiveTrack{current.colors.neutralBackground1.rest};
    inactiveTrack.set(ControlVisualState::Hovered, current.colors.neutralBackground1.hover)
        .set(ControlVisualState::Pressed, current.colors.neutralBackground1.pressed);
    StateProperty<Color> inactiveBorder{current.colors.neutralStrokeAccessible};
    inactiveBorder.set(ControlVisualState::Hovered, current.colors.neutralStrokeAccessibleHover)
        .set(ControlVisualState::Pressed, current.colors.neutralStrokeAccessiblePressed);
    Color trackColor = on ? activeTrack.resolve(visualStates()) : inactiveTrack.resolve(visualStates());
    Color border = on ? trackColor : inactiveBorder.resolve(visualStates());
    Color thumb = on ? current.colors.onBrand : current.colors.neutralForeground3;
    Color text = current.colors.neutralForeground1;
    if (!enabled) {
        trackColor = on ? current.colors.neutralForegroundDisabled : current.colors.neutralBackground1.rest;
        border = on ? trackColor : current.colors.neutralStrokeDisabled;
        thumb = on ? current.colors.neutralBackground1.rest : current.colors.neutralForegroundDisabled;
        text = current.colors.neutralForegroundDisabled;
    }
    context.fillStrokeRoundRect(track, current.radius.circular,
                                current.stroke.thin, trackColor, border);
    float thumbSize = on && size_ == SwitchSize::Medium ? 18.0f : 14.0f;
    if ((visualStates() & toMask(ControlVisualState::Pressed)) != 0 && enabled) {
        thumbSize = std::min(trackHeight - current.stroke.thin * 2.0f, thumbSize + 2.0f);
    }
    const float inset = (trackHeight - thumbSize) * 0.5f;
    const float thumbX = on ? track.x + track.width - thumbSize - inset : track.x + inset;
    context.fillRoundRect({thumbX, track.y + (track.height - thumbSize) * 0.5f, thumbSize, thumbSize}, current.radius.circular, thumb);
    if (!displayLabel.empty()) {
        context.drawText(displayLabel, labelBox.x,
                         context.centeredTextBottom(
                             displayLabel, labelBox,
                             current.typography.body1.size,
                             required_ ? current.typography.weightSemibold
                                       : current.typography.body1.weight,
                             current.typography.body1.family),
                         current.typography.body1.size, text,
                         required_ ? current.typography.weightSemibold
                                   : current.typography.body1.weight,
                         current.typography.body1.family);
    }
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
AccessibilityActionCapabilities Switch::accessibilityActions() const noexcept { AccessibilityActionCapabilities actions; actions.toggle = true; return actions; }
AccessibilityActionStatus Switch::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value) { (void)value; if (kind != AccessibilityActionKind::Toggle) return AccessibilityActionStatus::NotSupported; if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled; toggle(); return AccessibilityActionStatus::Succeeded; }

Slider::Slider(float minimum, float maximum, float value) { setRange(minimum, maximum); value_ = normalizedAndSnapped(value); }
float Slider::minimum() const noexcept { return minimum_; }
float Slider::maximum() const noexcept { return maximum_; }
void Slider::setRange(float minimum, float maximum)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum)) { minimum = 0.0f; maximum = 100.0f; }
    if (maximum < minimum) std::swap(minimum, maximum);
    minimum_ = minimum; maximum_ = maximum; setValue(value_); markDirty(DirtyFlag::Layout);
}
float Slider::value() const noexcept { return value_; }
Slider& Slider::value(float value) { setValue(value); return *this; }
void Slider::setValue(float value)
{
    const float next = normalizedAndSnapped(value);
    if (hasBinding_) {
        if (binding_->get() != next) binding_->set(next);
        if (value_ != next) { value_ = next; markDirty(DirtyFlag::Paint); }
    }
    else if (value_ != next) { value_ = next; markDirty(DirtyFlag::Paint); }
}
float Slider::step() const noexcept { return step_; }
Slider& Slider::step(float value) { setStep(value); return *this; }
void Slider::setStep(float value) { step_ = std::isfinite(value) && value > 0.0f ? value : 0.0f; setValue(this->value()); }
Slider& Slider::bind(State<float>& state)
{
    binding_.emplace(state); hasBinding_ = true; value_ = normalizedAndSnapped(state.get());
    subscription_.subscribe(state, [this](float value) {
        const float canonical = normalizedAndSnapped(value);
        value_ = canonical;
        markDirty(DirtyFlag::Paint);
        if (hasBinding_ && binding_->get() != canonical) binding_->set(canonical);
    });
    setValue(value_); return *this;
}
Slider& Slider::onChange(ChangeHandler handler) { onChange_ = std::move(handler); return *this; }
const std::string& Slider::accessibleLabel() const noexcept { return accessibleLabel_; }
Slider& Slider::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void Slider::setAccessibleLabel(std::string value)
{
    if (accessibleLabel_ != value) { accessibleLabel_ = std::move(value); markDirty(DirtyFlag::Style); }
}
SliderSize Slider::size() const noexcept { return size_; }
Slider& Slider::size(SliderSize value) noexcept { setSize(value); return *this; }
void Slider::setSize(SliderSize value) noexcept
{
    if (size_ != value) { size_ = value; markDirty(DirtyFlag::Layout); }
}
SliderOrientation Slider::orientation() const noexcept { return orientation_; }
Slider& Slider::orientation(SliderOrientation value) noexcept { setOrientation(value); return *this; }
void Slider::setOrientation(SliderOrientation value) noexcept
{
    if (orientation_ != value) { orientation_ = value; markDirty(DirtyFlag::Layout); }
}
SizeF Slider::measure(const Constraints& constraints) const
{
    const float cross = sliderCrossAxisSize(size_);
    return orientation_ == SliderOrientation::Horizontal
        ? constraints.clamp({160.0f, cross})
        : constraints.clamp({cross, 160.0f});
}
float Slider::normalizedValue() const noexcept { const float span = maximum_ - minimum_; return span > 0.0f ? std::clamp((value() - minimum_) / span, 0.0f, 1.0f) : 0.0f; }
float Slider::normalizedAndSnapped(float value) const noexcept
{
    if (!std::isfinite(value)) value = minimum_;
    value = std::clamp(value, minimum_, maximum_);
    if (step_ > 0.0f) value = minimum_ + std::round((value - minimum_) / step_) * step_;
    return std::clamp(value, minimum_, maximum_);
}
void Slider::setValueFromPointer(float coordinate)
{
    const float thumbSize = sliderThumbSize(size_);
    const float extent = orientation_ == SliderOrientation::Horizontal ? bounds().width : bounds().height;
    const float origin = orientation_ == SliderOrientation::Horizontal ? bounds().x : bounds().y;
    const float usable = std::max(1.0f, extent - thumbSize);
    float fraction = std::clamp((coordinate - origin - thumbSize * 0.5f) / usable, 0.0f, 1.0f);
    if (orientation_ == SliderOrientation::Vertical) fraction = 1.0f - fraction;
    const float before = value();
    setValue(minimum_ + fraction * (maximum_ - minimum_));
    if (value() != before && onChange_) onChange_(value());
}
void Slider::paint(PaintContext& context)
{
    const auto& current = theme(); const bool enabled = isEnabled();
    const float baseThumbSize = sliderThumbSize(size_);
    const float trackSize = sliderTrackSize(size_);
    const float fraction = normalizedValue();
    RectF track{};
    RectF active{};
    float thumbCenterX = 0.0f;
    float thumbCenterY = 0.0f;
    if (orientation_ == SliderOrientation::Horizontal) {
        track = {bounds().x + baseThumbSize * 0.5f,
                 bounds().y + (bounds().height - trackSize) * 0.5f,
                 std::max(0.0f, bounds().width - baseThumbSize), trackSize};
        thumbCenterX = track.x + track.width * fraction;
        thumbCenterY = track.y + track.height * 0.5f;
        active = {track.x, track.y, track.width * fraction, track.height};
    } else {
        track = {bounds().x + (bounds().width - trackSize) * 0.5f,
                 bounds().y + baseThumbSize * 0.5f, trackSize,
                 std::max(0.0f, bounds().height - baseThumbSize)};
        thumbCenterX = track.x + track.width * 0.5f;
        thumbCenterY = track.y + track.height * (1.0f - fraction);
        active = {track.x, thumbCenterY, track.width,
                  std::max(0.0f, track.y + track.height - thumbCenterY)};
    }
    StateProperty<Color> interactionColor{current.colors.brandBackground.rest};
    interactionColor.set(ControlVisualState::Hovered, current.colors.brandBackground.hover)
        .set(ControlVisualState::Pressed, current.colors.brandBackground.pressed);
    Color fill = interactionColor.resolve(visualStates());
    Color thumb = fill;
    const Color inactiveTrack = enabled ? current.colors.neutralStroke1 : current.colors.neutralStrokeDisabled;
    if (!enabled) { fill = current.colors.neutralForegroundDisabled; thumb = current.colors.neutralForegroundDisabled; }
    context.fillRoundRect(track, current.radius.circular, inactiveTrack);
    context.fillRoundRect(active, current.radius.circular, fill);
    const float thumbSize = (visualStates() & toMask(ControlVisualState::Pressed)) != 0
        ? baseThumbSize + 2.0f : baseThumbSize;
    const RectF thumbBounds{thumbCenterX - thumbSize * 0.5f, thumbCenterY - thumbSize * 0.5f,
                            thumbSize, thumbSize};
    drawFocusRing(context, thumbBounds, current,
                  (visualStates() & toMask(ControlVisualState::Focused)) != 0,
                  current.radius.circular);
    context.fillRoundRect(thumbBounds, current.radius.circular, thumb);
    if (enabled) {
        // A thin white inset is the visual anchor that distinguishes a
        // Fluent slider thumb from a generic flat coloured disc.
        const float ringInset = current.stroke.thin;
        context.strokeRoundRect({thumbBounds.x + ringInset, thumbBounds.y + ringInset,
                                 std::max(0.0f, thumbBounds.width - ringInset * 2.0f),
                                 std::max(0.0f, thumbBounds.height - ringInset * 2.0f)},
                                current.radius.circular, current.stroke.thin, current.colors.onBrand);
    }
    clearDirty(DirtyFlag::Paint);
}
bool Slider::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    updateCommonPointerState(*this, event);
    const float coordinate = orientation_ == SliderOrientation::Horizontal ? event.position.x : event.position.y;
    if (event.action == PointerAction::Down && isPrimary(event)) { setVisualState(ControlVisualState::Pressed, true); setValueFromPointer(coordinate); return true; }
    if (event.action == PointerAction::Move && (visualStates() & toMask(ControlVisualState::Pressed)) != 0) { setValueFromPointer(coordinate); return true; }
    if (event.action == PointerAction::Up && isPrimary(event)) { if ((visualStates() & toMask(ControlVisualState::Pressed)) != 0) setValueFromPointer(coordinate); setVisualState(ControlVisualState::Pressed, false); return true; }
    return event.action == PointerAction::Enter || event.action == PointerAction::Move || event.action == PointerAction::Leave || event.action == PointerAction::Cancel;
}
bool Slider::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || event.action != KeyAction::Down) return false;
    float next = value(); const float increment = step_ > 0.0f ? step_ : std::max((maximum_ - minimum_) / 100.0f, 1.0f);
    switch (event.keyCode) {
    case 37: next -= increment; break;
    case 39: next += increment; break;
    case 38: next += increment; break;
    case 40: next -= increment; break;
    case 36: next = minimum_; break;
    case 35: next = maximum_; break;
    default: return false;
    }
    const float before = value(); setValue(next); if (value() != before && onChange_) onChange_(value()); return true;
}
AccessibilityActionCapabilities Slider::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions;
    actions.setValue = true;
    return actions;
}
AccessibilityActionStatus Slider::performAccessibilityAction(AccessibilityActionKind kind,
                                                               std::string_view actionValue)
{
    if (kind != AccessibilityActionKind::SetValue) return AccessibilityActionStatus::NotSupported;
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    float requested = 0.0f;
    if (!parseFloat(actionValue, requested)) return AccessibilityActionStatus::InvalidValue;
    if (requested < minimum_ || requested > maximum_) return AccessibilityActionStatus::InvalidValue;
    const float before = value();
    setValue(requested);
    if (value() != before && onChange_) onChange_(value());
    return AccessibilityActionStatus::Succeeded;
}

ProgressBar::ProgressBar(float minimum, float maximum, std::optional<float> value)
{
    setRange(minimum, maximum);
    if (value.has_value()) {
        value_ = clampedValue(*value);
        indeterminate_ = false;
    } else {
        value_ = minimum_;
        indeterminate_ = true;
    }
}
ProgressBar::~ProgressBar() { stopIndeterminateAnimation(); }
float ProgressBar::minimum() const noexcept { return minimum_; }
float ProgressBar::maximum() const noexcept { return maximum_; }
void ProgressBar::setRange(float minimum, float maximum)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum)) { minimum = 0.0f; maximum = 100.0f; }
    if (maximum < minimum) std::swap(minimum, maximum);
    minimum_ = minimum;
    maximum_ = maximum;
    // Changing the range adjusts any retained numeric value but must not
    // silently change whether the operation is indeterminate.
    value_ = clampedValue(value_);
    if (hasBinding_ && binding_->get() != value_) binding_->set(value_);
    markDirty(DirtyFlag::Layout);
}
float ProgressBar::value() const noexcept { return value_; }
ProgressBar& ProgressBar::value(float value) { setValue(value); return *this; }
void ProgressBar::setValue(float value)
{
    const float next = clampedValue(value);
    // Providing a value is the public transition from Fluent's default
    // indeterminate operation to a determinate operation. Callers that need
    // to keep a hidden staging value can explicitly switch back afterwards.
    setIndeterminate(false);
    if (hasBinding_) {
        if (binding_->get() != next) binding_->set(next);
        if (value_ != next) { value_ = next; markDirty(DirtyFlag::Paint); }
    } else if (value_ != next) { value_ = next; markDirty(DirtyFlag::Paint); }
}
ProgressBar& ProgressBar::bind(State<float>& state)
{
    binding_.emplace(state); hasBinding_ = true; value_ = clampedValue(state.get());
    setIndeterminate(false);
    subscription_.subscribe(state, [this](float value) {
        const float canonical = clampedValue(value);
        value_ = canonical;
        markDirty(DirtyFlag::Paint);
        if (hasBinding_ && binding_->get() != canonical) binding_->set(canonical);
    });
    setValue(value_); return *this;
}
const std::string& ProgressBar::accessibleLabel() const noexcept { return accessibleLabel_; }
ProgressBar& ProgressBar::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void ProgressBar::setAccessibleLabel(std::string value)
{
    if (accessibleLabel_ != value) { accessibleLabel_ = std::move(value); markDirty(DirtyFlag::Style); }
}
bool ProgressBar::isIndeterminate() const noexcept { return indeterminate_; }
std::optional<float> ProgressBar::determinateValue() const noexcept
{
    if (indeterminate_) return std::nullopt;
    return value();
}
ProgressBar& ProgressBar::indeterminate(bool value) noexcept { setIndeterminate(value); return *this; }
void ProgressBar::setIndeterminate(bool value) noexcept
{
    if (indeterminate_ == value) return;
    indeterminate_ = value;
    if (indeterminate_) startIndeterminateAnimation();
    else stopIndeterminateAnimation();
    markDirty(DirtyFlag::Paint);
}
ProgressBarColor ProgressBar::color() const noexcept { return color_; }
ProgressBar& ProgressBar::color(ProgressBarColor value) noexcept { setColor(value); return *this; }
void ProgressBar::setColor(ProgressBarColor value) noexcept
{
    if (color_ != value) { color_ = value; markDirty(DirtyFlag::Paint); }
}
ProgressBarShape ProgressBar::shape() const noexcept { return shape_; }
ProgressBar& ProgressBar::shape(ProgressBarShape value) noexcept { setShape(value); return *this; }
void ProgressBar::setShape(ProgressBarShape value) noexcept
{
    if (shape_ != value) { shape_ = value; markDirty(DirtyFlag::Paint); }
}
ProgressBarThickness ProgressBar::thickness() const noexcept { return thickness_; }
ProgressBar& ProgressBar::thickness(ProgressBarThickness value) noexcept { setThickness(value); return *this; }
void ProgressBar::setThickness(ProgressBarThickness value) noexcept
{
    if (thickness_ != value) { thickness_ = value; markDirty(DirtyFlag::Layout); }
}
bool ProgressBar::isMotionEnabled() const noexcept { return motionEnabled_; }
ProgressBar& ProgressBar::motionEnabled(bool value) noexcept { setMotionEnabled(value); return *this; }
void ProgressBar::setMotionEnabled(bool value) noexcept
{
    if (motionEnabled_ == value) return;
    motionEnabled_ = value;
    if (motionEnabled_) startIndeterminateAnimation();
    else stopIndeterminateAnimation();
    markDirty(DirtyFlag::Paint);
}
SizeF ProgressBar::measure(const Constraints& constraints) const
{
    return constraints.clamp({160.0f, progressHeight(thickness_)});
}
float ProgressBar::normalizedValue() const noexcept { const float span = maximum_ - minimum_; return span > 0.0f ? std::clamp((value() - minimum_) / span, 0.0f, 1.0f) : 0.0f; }
float ProgressBar::clampedValue(float value) const noexcept { return std::clamp(std::isfinite(value) ? value : minimum_, minimum_, maximum_); }
void ProgressBar::paint(PaintContext& context)
{
    const auto& current = theme();
    const float height = progressHeight(thickness_);
    const RectF track{bounds().x, bounds().y + (bounds().height - height) * 0.5f, bounds().width, height};
    const float radius = shape_ == ProgressBarShape::Rounded ? current.radius.circular : current.radius.none;
    // Fluent's indeterminate bar communicates ongoing activity with the
    // brand token, regardless of a determinate status-color choice.
    Color barColor = current.colors.brandBackground.rest;
    if (!indeterminate_) {
        switch (color_) {
        case ProgressBarColor::Error: barColor = current.colors.statusDanger; break;
        case ProgressBarColor::Warning: barColor = current.colors.statusWarning; break;
        case ProgressBarColor::Success: barColor = current.colors.statusSuccess; break;
        case ProgressBarColor::Brand: default: break;
        }
    }
    context.fillRoundRect(track, radius, current.colors.neutralStroke1);
    if (indeterminate_) {
        const float segmentWidth = track.width * 0.35f;
        const float phase = motionEnabled_ ? indeterminatePhase_ : 0.5f;
        const float x = track.x - segmentWidth + (track.width + segmentWidth) * phase;
        const int checkpoint = context.save();
        context.clipRect(track);
        context.fillRoundRect({x, track.y, segmentWidth, track.height}, radius, barColor);
        context.restoreTo(checkpoint);
    } else {
        context.fillRoundRect({track.x, track.y, track.width * normalizedValue(), track.height}, radius, barColor);
    }
    clearDirty(DirtyFlag::Paint);
}
AccessibilityActionCapabilities ProgressBar::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions;
    actions.valueReadOnly = true;
    return actions;
}
void ProgressBar::onAttach() noexcept { startIndeterminateAnimation(); }
void ProgressBar::onDetach() noexcept { stopIndeterminateAnimation(); }
void ProgressBar::startIndeterminateAnimation() noexcept
{
    if (!isAttached() || !indeterminate_ || !motionEnabled_ || animationId_.has_value()) return;
    animationId_ = Ticker::instance().add(
        Animation(1.6f, [this](float phase) {
            indeterminatePhase_ = phase;
            markDirty(DirtyFlag::Paint);
        }, easing::easeInOutCubic).repeat(-1));
}
void ProgressBar::stopIndeterminateAnimation() noexcept
{
    if (!animationId_.has_value()) return;
    Ticker::instance().cancel(*animationId_);
    animationId_.reset();
}

Divider::Divider(DividerOrientation orientation) : orientation_(orientation) {}
DividerOrientation Divider::orientation() const noexcept { return orientation_; }
void Divider::setOrientation(DividerOrientation orientation) noexcept { orientation_ = orientation; markDirty(DirtyFlag::Layout); }
float Divider::thickness() const noexcept { return thickness_; }
void Divider::setThickness(float thickness) noexcept { thickness_ = std::max(1.0f, std::isfinite(thickness) ? thickness : 1.0f); markDirty(DirtyFlag::Layout); }
const std::string& Divider::content() const noexcept { return content_; }
Divider& Divider::content(std::string value) { setContent(std::move(value)); return *this; }
void Divider::setContent(std::string value)
{
    if (content_ != value) { content_ = std::move(value); markDirty(DirtyFlag::Layout); }
}
DividerAppearance Divider::appearance() const noexcept { return appearance_; }
Divider& Divider::appearance(DividerAppearance value) noexcept { setAppearance(value); return *this; }
void Divider::setAppearance(DividerAppearance value) noexcept
{
    if (appearance_ != value) { appearance_ = value; markDirty(DirtyFlag::Paint); }
}
DividerContentAlignment Divider::contentAlignment() const noexcept { return contentAlignment_; }
Divider& Divider::contentAlignment(DividerContentAlignment value) noexcept { setContentAlignment(value); return *this; }
void Divider::setContentAlignment(DividerContentAlignment value) noexcept
{
    if (contentAlignment_ != value) { contentAlignment_ = value; markDirty(DirtyFlag::Paint); }
}
bool Divider::isInset() const noexcept { return inset_; }
Divider& Divider::inset(bool value) noexcept { setInset(value); return *this; }
void Divider::setInset(bool value) noexcept
{
    if (inset_ != value) { inset_ = value; markDirty(DirtyFlag::Paint); }
}
SizeF Divider::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    if (orientation_ == DividerOrientation::Horizontal) {
        const float height = content_.empty() ? thickness_ : current.typography.body1.lineHeight;
        return constraints.clamp({0.0f, height});
    }
    const float width = content_.empty() ? thickness_
        : labelWidth(content_, current) + kDividerLabelGap * 2.0f;
    return constraints.clamp({width, 0.0f});
}
void Divider::paint(PaintContext& context)
{
    const auto& current = theme();
    Color lineColor = current.colors.neutralStroke1;
    Color textColor = current.colors.neutralForeground2;
    switch (appearance_) {
    case DividerAppearance::Subtle:
        lineColor.a = 128;
        textColor = current.colors.neutralForeground3;
        break;
    case DividerAppearance::Brand:
        lineColor = current.colors.brandBackground.rest;
        textColor = current.colors.brandForeground1;
        break;
    case DividerAppearance::Strong:
        lineColor = current.colors.neutralStrokeAccessible;
        textColor = current.colors.neutralForeground1;
        break;
    case DividerAppearance::Default: default: break;
    }
    const float inset = inset_ ? current.spacing.horizontal.m : 0.0f;
    if (orientation_ == DividerOrientation::Horizontal) {
        const float start = bounds().x + inset;
        const float end = std::max(start, bounds().x + bounds().width - inset);
        const float y = bounds().y + (bounds().height - thickness_) * 0.5f;
        if (content_.empty()) {
            context.fillRect({start, y, end - start, thickness_}, lineColor);
        } else {
            const float textWidth = labelWidth(content_, current);
            const float available = std::max(0.0f, end - start - textWidth - kDividerLabelGap * 2.0f);
            float before = available * 0.5f;
            if (contentAlignment_ == DividerContentAlignment::Start) before = 0.0f;
            else if (contentAlignment_ == DividerContentAlignment::End) before = available;
            const float labelX = start + before + kDividerLabelGap;
            if (before > 0.0f) context.fillRect({start, y, before, thickness_}, lineColor);
            const float afterX = labelX + textWidth + kDividerLabelGap;
            if (end > afterX) context.fillRect({afterX, y, end - afterX, thickness_}, lineColor);
            context.drawText(content_, labelX,
                             context.centeredTextBottom(
                                 content_, bounds(),
                                 current.typography.body1.size,
                                 current.typography.body1.weight,
                                 current.typography.body1.family),
                             current.typography.body1.size, textColor,
                             current.typography.body1.weight,
                             current.typography.body1.family);
        }
    } else {
        const float start = bounds().y + inset;
        const float end = std::max(start, bounds().y + bounds().height - inset);
        const float x = bounds().x + (bounds().width - thickness_) * 0.5f;
        if (content_.empty()) {
            context.fillRect({x, start, thickness_, end - start}, lineColor);
        } else {
            const float labelHeight = current.typography.body1.lineHeight;
            const float available = std::max(0.0f, end - start - labelHeight - kDividerLabelGap * 2.0f);
            float before = available * 0.5f;
            if (contentAlignment_ == DividerContentAlignment::Start) before = 0.0f;
            else if (contentAlignment_ == DividerContentAlignment::End) before = available;
            if (before > 0.0f) context.fillRect({x, start, thickness_, before}, lineColor);
            const float labelY = start + before + kDividerLabelGap;
            const RectF labelBox{bounds().x, labelY, bounds().width, labelHeight};
            const float labelX = bounds().x + std::max(0.0f, (bounds().width - labelWidth(content_, current)) * 0.5f);
            context.drawText(content_, labelX,
                             context.centeredTextBottom(
                                 content_, labelBox,
                                 current.typography.body1.size,
                                 current.typography.body1.weight,
                                 current.typography.body1.family),
                             current.typography.body1.size, textColor,
                             current.typography.body1.weight,
                             current.typography.body1.family);
            const float afterY = labelY + labelHeight + kDividerLabelGap;
            if (end > afterY) context.fillRect({x, afterY, thickness_, end - afterY}, lineColor);
        }
    }
    clearDirty(DirtyFlag::Paint);
}

} // namespace wui
