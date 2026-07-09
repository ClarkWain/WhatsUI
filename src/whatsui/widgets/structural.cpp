#include "wui/structural.h"

#include <algorithm>
#include <vector>

namespace wui {

void IfNode::setFactory(Factory factory)
{
    factory_ = std::move(factory);
    reconcile();
}

void IfNode::setVisible(bool visible)
{
    if (visible_ == visible) {
        return;
    }
    visible_ = visible;
    reconcile();
}

void IfNode::reconcile()
{
    if (visible_) {
        if (children().empty() && factory_) {
            if (auto child = factory_()) {
                appendChild(std::move(child));
            }
        }
    } else {
        while (!children().empty()) {
            removeChild(children().size() - 1);
        }
    }
}

SizeF IfNode::measure(const Constraints& constraints) const
{
    const auto& childNodes = children();
    if (childNodes.empty()) {
        return constraints.clamp({0.0f, 0.0f});
    }
    return constraints.clamp(childNodes.front()->measure(constraints));
}

void IfNode::layout(const RectF& bounds)
{
    Node::layout(bounds);
    const auto& childNodes = children();
    if (!childNodes.empty()) {
        childNodes.front()->layout(bounds);
    }
}

// --- ForEachNode ---

ForEachNode& ForEachNode::direction(ForEachDirection dir) noexcept
{
    setDirection(dir);
    return *this;
}

void ForEachNode::setDirection(ForEachDirection dir) noexcept
{
    direction_ = dir;
    markDirty(DirtyFlag::Layout);
}

ForEachDirection ForEachNode::direction() const noexcept
{
    return direction_;
}

ForEachNode& ForEachNode::gap(float gap) noexcept
{
    setGap(gap);
    return *this;
}

void ForEachNode::setGap(float gap) noexcept
{
    gap_ = gap;
    markDirty(DirtyFlag::Layout);
}

float ForEachNode::gap() const noexcept
{
    return gap_;
}

ForEachNode& ForEachNode::padding(InsetsF padding) noexcept
{
    setPadding(padding);
    return *this;
}

void ForEachNode::setPadding(InsetsF padding) noexcept
{
    padding_ = padding;
    markDirty(DirtyFlag::Layout);
}

InsetsF ForEachNode::padding() const noexcept
{
    return padding_;
}

ForEachNode& ForEachNode::align(Alignment align) noexcept
{
    setAlign(align);
    return *this;
}

void ForEachNode::setAlign(Alignment align) noexcept
{
    align_ = align;
    markDirty(DirtyFlag::Layout);
}

Alignment ForEachNode::align() const noexcept
{
    return align_;
}

SizeF ForEachNode::measure(const Constraints& constraints) const
{
    const auto& childNodes = children();
    float mainSize = 0.0f;
    float crossSize = 0.0f;

    for (std::size_t i = 0; i < childNodes.size(); ++i) {
        const auto childSize = childNodes[i]->measure(constraints);
        if (direction_ == ForEachDirection::Vertical) {
            mainSize += childSize.height;
            crossSize = std::max(crossSize, childSize.width);
        } else {
            mainSize += childSize.width;
            crossSize = std::max(crossSize, childSize.height);
        }
        if (i + 1 < childNodes.size()) {
            mainSize += gap_;
        }
    }

    if (direction_ == ForEachDirection::Vertical) {
        return constraints.clamp({crossSize + padding_.horizontal(), mainSize + padding_.vertical()});
    }
    return constraints.clamp({mainSize + padding_.horizontal(), crossSize + padding_.vertical()});
}

void ForEachNode::layout(const RectF& bounds)
{
    Node::layout(bounds);

    const auto& childNodes = children();
    const float innerWidth = std::max(0.0f, bounds.width - padding_.horizontal());
    const float innerHeight = std::max(0.0f, bounds.height - padding_.vertical());
    const Constraints loose{0.0f, innerWidth, 0.0f, innerHeight};

    if (direction_ == ForEachDirection::Vertical) {
        // Vertical layout (Column-like)
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
    } else {
        // Horizontal layout (Row-like)
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
}

} // namespace wui
