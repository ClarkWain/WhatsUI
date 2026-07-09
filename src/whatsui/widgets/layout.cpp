#include "wui/widgets.h"

#include <algorithm>
#include <utility>
#include <vector>

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

Row& Row::align(Alignment align) noexcept
{
    setAlign(align);
    return *this;
}

void Row::setAlign(Alignment align) noexcept
{
    align_ = align;
    markDirty(DirtyFlag::Layout);
}

Alignment Row::align() const noexcept
{
    return align_;
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
    const Constraints loose{0.0f, innerWidth, 0.0f, innerHeight};

    // Pass 1: measure fixed children; accumulate width and total flex weight.
    std::vector<SizeF> sizes(childNodes.size());
    float fixedWidth = 0.0f;
    float totalFlex = 0.0f;
    for (std::size_t i = 0; i < childNodes.size(); ++i) {
        if (childNodes[i]->flex() > 0.0f) {
            totalFlex += childNodes[i]->flex();
        } else {
            sizes[i] = childNodes[i]->measure(loose);
            fixedWidth += sizes[i].width;
        }
        if (i + 1 < childNodes.size()) {
            fixedWidth += gap_;
        }
    }
    const float remaining = std::max(0.0f, innerWidth - fixedWidth);

    // Pass 2: size flex children from the remainder, then place with cross align.
    float cursorX = bounds.x + padding_.left;
    for (std::size_t i = 0; i < childNodes.size(); ++i) {
        Node* child = childNodes[i].get();
        SizeF childSize = sizes[i];
        if (child->flex() > 0.0f) {
            const float allocated = totalFlex > 0.0f ? remaining * (child->flex() / totalFlex) : 0.0f;
            childSize = child->measure(Constraints{0.0f, allocated, 0.0f, innerHeight});
            childSize.width = allocated;
        }
        float childY = bounds.y + padding_.top;
        switch (align_) {
        case Alignment::Center:
            childY += (innerHeight - childSize.height) * 0.5f;
            break;
        case Alignment::End:
            childY += innerHeight - childSize.height;
            break;
        case Alignment::Stretch:
            childSize.height = innerHeight;
            break;
        case Alignment::Start:
        default:
            break;
        }
        child->layout({cursorX, childY, childSize.width, childSize.height});
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

Column& Column::align(Alignment align) noexcept
{
    setAlign(align);
    return *this;
}

void Column::setAlign(Alignment align) noexcept
{
    align_ = align;
    markDirty(DirtyFlag::Layout);
}

Alignment Column::align() const noexcept
{
    return align_;
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
    const Constraints loose{0.0f, innerWidth, 0.0f, innerHeight};

    // Pass 1: measure fixed children; accumulate height and total flex weight.
    std::vector<SizeF> sizes(childNodes.size());
    float fixedHeight = 0.0f;
    float totalFlex = 0.0f;
    for (std::size_t i = 0; i < childNodes.size(); ++i) {
        if (childNodes[i]->flex() > 0.0f) {
            totalFlex += childNodes[i]->flex();
        } else {
            sizes[i] = childNodes[i]->measure(loose);
            fixedHeight += sizes[i].height;
        }
        if (i + 1 < childNodes.size()) {
            fixedHeight += gap_;
        }
    }
    const float remaining = std::max(0.0f, innerHeight - fixedHeight);

    // Pass 2: size flex children from the remainder, then place with cross align.
    float cursorY = bounds.y + padding_.top;
    for (std::size_t i = 0; i < childNodes.size(); ++i) {
        Node* child = childNodes[i].get();
        SizeF childSize = sizes[i];
        if (child->flex() > 0.0f) {
            const float allocated = totalFlex > 0.0f ? remaining * (child->flex() / totalFlex) : 0.0f;
            childSize = child->measure(Constraints{0.0f, innerWidth, 0.0f, allocated});
            childSize.height = allocated;
        }
        float childX = bounds.x + padding_.left;
        switch (align_) {
        case Alignment::Center:
            childX += (innerWidth - childSize.width) * 0.5f;
            break;
        case Alignment::End:
            childX += innerWidth - childSize.width;
            break;
        case Alignment::Stretch:
            childSize.width = innerWidth;
            break;
        case Alignment::Start:
        default:
            break;
        }
        child->layout({childX, cursorY, childSize.width, childSize.height});
        cursorY += childSize.height + gap_;
    }
}

} // namespace wui
