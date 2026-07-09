#include "wui/widgets.h"

#include <algorithm>
#include <utility>

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

} // namespace wui
