#include "wui/widgets.h"

#include <algorithm>
#include <utility>

#include "wui/text_input.h"
#include "wui/text_metrics.h"
#include "wui/theme.h"

namespace wui {
namespace {

const TextStyleToken& styleFor(const Theme& current, LabelSize size) noexcept
{
    switch (size) {
    case LabelSize::Small: return current.typography.caption1Strong;
    case LabelSize::Large: return current.typography.subtitle2;
    case LabelSize::Medium: default: return current.typography.body1Strong;
    }
}

} // namespace

Label::Label(std::string text) : text_(std::move(text)) {}
Label& Label::text(std::string text) { setText(std::move(text)); return *this; }
const std::string& Label::text() const noexcept { return text_; }
void Label::setText(std::string text) { text_ = std::move(text); markDirty(DirtyFlag::Layout); }
void Label::setSize(LabelSize size) noexcept { if (size_ != size) { size_ = size; markDirty(DirtyFlag::Layout); } }
LabelSize Label::size() const noexcept { return size_; }
void Label::setRequired(bool required) noexcept { if (required_ != required) { required_ = required; markDirty(DirtyFlag::Paint); } }
bool Label::isRequired() const noexcept { return required_; }
void Label::setForControl(TextInput* control) noexcept
{
    control_ = control;
    if (control_ != nullptr && !text_.empty()) control_->setAccessibleLabel(text_);
}
TextInput* Label::forControl() const noexcept { return control_; }

SizeF Label::measure(const Constraints& constraints) const
{
    const auto& style = styleFor(theme(), size_);
    float width = static_cast<float>(text_.size() + (required_ ? 1 : 0)) * (style.size * 0.56f);
    if (const TextMeasurer* measurer = textMeasurer()) {
        width = measurer->measureText(text_, style.size, style.weight).width;
        if (required_) width += theme().spacing.horizontal.xs + measurer->measureText("*", style.size, style.weight).width;
    }
    return constraints.clamp({width, style.lineHeight});
}

void Label::paint(PaintContext& context)
{
    const auto& current = theme();
    const auto& style = styleFor(current, size_);
    context.drawText(text_, bounds().x, context.centeredTextBottom(text_, bounds(), style.size, style.weight),
                     style.size, current.colors.neutralForeground1, style.weight, style.family);
    if (required_) {
        float width = static_cast<float>(text_.size()) * (style.size * 0.56f);
        if (const TextMeasurer* measurer = textMeasurer()) {
            width = measurer->measureText(text_, style.size, style.weight).width;
        }
        context.drawText("*", bounds().x + width + current.spacing.horizontal.xs,
                         context.centeredTextBottom("*", bounds(), style.size, style.weight), style.size,
                         current.colors.statusDanger, style.weight, style.family);
    }
    clearDirty(DirtyFlag::Paint);
}

EventResult Label::onPointerEvent(const PointerEvent& event, EventContext& context)
{
    if (context.phase() != EventPhase::Target || control_ == nullptr) return EventResult::Ignored;
    if (event.action == PointerAction::Down && event.button == MouseButton::Left) {
        context.requestFocus(control_);
        return EventResult::RequestFocus;
    }
    return EventResult::Ignored;
}

} // namespace wui
