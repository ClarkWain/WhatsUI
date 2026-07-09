#include "wui/widgets.h"

#include <algorithm>
#include <utility>

#include "wui/text_metrics.h"

namespace wui {

namespace {

SizeF measureSingleChild(const ContainerNode& node, const Constraints& constraints)
{
    const auto& children = node.children();
    if (children.empty()) {
        return constraints.clamp({0.0f, 0.0f});
    }
    return constraints.clamp(children.front()->measure(constraints));
}

} // namespace

Text::Text(std::string value)
    : value_(std::move(value))
{
}

const std::string& Text::value() const noexcept
{
    return value_;
}

Text& Text::value(std::string value)
{
    setValue(std::move(value));
    return *this;
}

void Text::setValue(std::string value)
{
    value_ = std::move(value);
    markDirty(DirtyFlag::Layout);
}

SizeF Text::measure(const Constraints& constraints) const
{
    if (const TextMeasurer* measurer = textMeasurer()) {
        const TextExtents extents = measurer->measureText(value_, fontSize_);
        return constraints.clamp({extents.width, extents.height});
    }
    const auto width = static_cast<float>(value_.size()) * (fontSize_ * 0.5f);
    const auto height = fontSize_ * 1.25f;
    return constraints.clamp({width, height});
}

void Text::paint(PaintContext& context)
{
    if (!value_.empty()) {
        float baseline = bounds().y + fontSize_ * 0.8f;
        if (const TextMeasurer* measurer = textMeasurer()) {
            baseline = bounds().y + measurer->measureText(value_, fontSize_).ascent;
        }
        context.drawText(value_, bounds().x, baseline, fontSize_, Color{32, 32, 32, 255});
    }
    clearDirty(DirtyFlag::Paint);
}

Spacer::Spacer(SizeF size) noexcept
    : size_(size)
{
}

SizeF Spacer::size() const noexcept
{
    return size_;
}

void Spacer::setSize(SizeF size) noexcept
{
    size_ = size;
    markDirty(DirtyFlag::Layout);
}

SizeF Spacer::measure(const Constraints& constraints) const
{
    return constraints.clamp(size_);
}

void Spacer::paint(PaintContext& context)
{
    (void)context;
    clearDirty(DirtyFlag::Paint);
}

Container& Container::child(std::unique_ptr<Node> child)
{
    appendChild(std::move(child));
    return *this;
}

SizeF Container::measure(const Constraints& constraints) const
{
    return measureSingleChild(*this, constraints);
}

void Container::layout(const RectF& bounds)
{
    Node::layout(bounds);
    const auto& childNodes = children();
    if (!childNodes.empty()) {
        childNodes.front()->layout(bounds);
    }
}

Row& Row::child(std::unique_ptr<Node> child)
{
    appendChild(std::move(child));
    return *this;
}

Row& Row::gap(float gap) noexcept
{
    setGap(gap);
    return *this;
}

void Row::setGap(float gap) noexcept
{
    gap_ = gap;
    markDirty(DirtyFlag::Layout);
}

float Row::gap() const noexcept
{
    return gap_;
}

Row& Row::padding(InsetsF padding) noexcept
{
    setPadding(padding);
    return *this;
}

void Row::setPadding(InsetsF padding) noexcept
{
    padding_ = padding;
    markDirty(DirtyFlag::Layout);
}

InsetsF Row::padding() const noexcept
{
    return padding_;
}

SizeF Row::measure(const Constraints& constraints) const
{
    float width = 0.0f;
    float height = 0.0f;

    const auto& childNodes = children();
    for (std::size_t index = 0; index < childNodes.size(); ++index) {
        const auto childSize = childNodes[index]->measure(constraints);
        width += childSize.width;
        height = std::max(height, childSize.height);
        if (index + 1 < childNodes.size()) {
            width += gap_;
        }
    }

    return constraints.clamp({width + padding_.horizontal(), height + padding_.vertical()});
}

void Row::layout(const RectF& bounds)
{
    Node::layout(bounds);

    const auto& childNodes = children();
    const float innerWidth = std::max(0.0f, bounds.width - padding_.horizontal());
    const float innerHeight = std::max(0.0f, bounds.height - padding_.vertical());
    float cursorX = bounds.x + padding_.left;
    const Constraints childConstraints{0.0f, innerWidth, 0.0f, innerHeight};
    for (const auto& child : childNodes) {
        const auto childSize = child->measure(childConstraints);
        child->layout({cursorX, bounds.y + padding_.top, childSize.width, childSize.height});
        cursorX += childSize.width + gap_;
    }
}

Column& Column::child(std::unique_ptr<Node> child)
{
    appendChild(std::move(child));
    return *this;
}

Column& Column::gap(float gap) noexcept
{
    setGap(gap);
    return *this;
}

void Column::setGap(float gap) noexcept
{
    gap_ = gap;
    markDirty(DirtyFlag::Layout);
}

float Column::gap() const noexcept
{
    return gap_;
}

Column& Column::padding(InsetsF padding) noexcept
{
    setPadding(padding);
    return *this;
}

void Column::setPadding(InsetsF padding) noexcept
{
    padding_ = padding;
    markDirty(DirtyFlag::Layout);
}

InsetsF Column::padding() const noexcept
{
    return padding_;
}

SizeF Column::measure(const Constraints& constraints) const
{
    float width = 0.0f;
    float height = 0.0f;

    const auto& childNodes = children();
    for (std::size_t index = 0; index < childNodes.size(); ++index) {
        const auto childSize = childNodes[index]->measure(constraints);
        width = std::max(width, childSize.width);
        height += childSize.height;
        if (index + 1 < childNodes.size()) {
            height += gap_;
        }
    }

    return constraints.clamp({width + padding_.horizontal(), height + padding_.vertical()});
}

void Column::layout(const RectF& bounds)
{
    Node::layout(bounds);

    const auto& childNodes = children();
    const float innerWidth = std::max(0.0f, bounds.width - padding_.horizontal());
    const float innerHeight = std::max(0.0f, bounds.height - padding_.vertical());
    float cursorY = bounds.y + padding_.top;
    const Constraints childConstraints{0.0f, innerWidth, 0.0f, innerHeight};
    for (const auto& child : childNodes) {
        const auto childSize = child->measure(childConstraints);
        child->layout({bounds.x + padding_.left, cursorY, childSize.width, childSize.height});
        cursorY += childSize.height + gap_;
    }
}

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

SizeF Button::measure(const Constraints& constraints) const
{
    const auto textWidth = static_cast<float>(label_.size()) * 8.0f;
    return constraints.clamp({textWidth + 24.0f, 32.0f});
}

void Button::paint(PaintContext& context)
{
    context.fillRoundRect(bounds(), 8.0f, Color{34, 114, 229, 255});
    if (!label_.empty()) {
        context.drawText(label_, bounds().x + 12.0f, bounds().y + 20.0f, 14.0f, Color{255, 255, 255, 255});
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
