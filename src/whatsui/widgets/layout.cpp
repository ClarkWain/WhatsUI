#include "wui/widgets.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace wui {

Container& Container::child(std::unique_ptr<Node> child)
{
    appendChild(std::move(child));
    return *this;
}

void Container::setBackground(Color color) noexcept
{
    background_ = color;
    markDirty(DirtyFlag::Paint);
}

void Container::setRadius(float radius) noexcept
{
    radius_ = radius;
    markDirty(DirtyFlag::Paint);
}

void Container::setPadding(InsetsF padding) noexcept
{
    padding_ = padding;
    markDirty(DirtyFlag::Layout);
}

void Container::setContentAlignment(Alignment horizontal, Alignment vertical) noexcept
{
    horizontalAlignment_ = horizontal;
    verticalAlignment_ = vertical;
    markDirty(DirtyFlag::Layout);
}

void Container::setWidth(float width) noexcept
{
    width_ = std::max(0.0f, width);
    markDirty(DirtyFlag::Layout);
}

void Container::clearWidth() noexcept
{
    width_.reset();
    markDirty(DirtyFlag::Layout);
}

void Container::setHeight(float height) noexcept
{
    height_ = std::max(0.0f, height);
    markDirty(DirtyFlag::Layout);
}

void Container::clearHeight() noexcept
{
    height_.reset();
    markDirty(DirtyFlag::Layout);
}

SizeF Container::measure(const Constraints& constraints) const
{
    const Constraints innerConstraints = constraints.deflate(padding_);
    SizeF content{};
    for (const auto& child : children()) {
        const auto childSize = child->measure(innerConstraints);
        content.width = std::max(content.width, childSize.width);
        content.height = std::max(content.height, childSize.height);
    }
    SizeF measured{content.width + padding_.horizontal(), content.height + padding_.vertical()};
    if (width_) {
        measured.width = *width_;
    }
    if (height_) {
        measured.height = *height_;
    }
    return constraints.clamp(measured);
}

void Container::layout(const RectF& bounds)
{
    Node::layout(bounds);
    const RectF contentBounds{bounds.x + padding_.left,
                              bounds.y + padding_.top,
                              std::max(0.0f, bounds.width - padding_.horizontal()),
                              std::max(0.0f, bounds.height - padding_.vertical())};
    for (const auto& child : children()) {
        SizeF childSize = child->measure({0.0f, contentBounds.width, 0.0f, contentBounds.height});
        float childX = contentBounds.x;
        float childY = contentBounds.y;
        switch (horizontalAlignment_) {
        case Alignment::Center: childX += (contentBounds.width - childSize.width) * 0.5f; break;
        case Alignment::End: childX += contentBounds.width - childSize.width; break;
        case Alignment::Stretch: childSize.width = contentBounds.width; break;
        case Alignment::Baseline:
        case Alignment::Start: break;
        }
        switch (verticalAlignment_) {
        case Alignment::Center: childY += (contentBounds.height - childSize.height) * 0.5f; break;
        case Alignment::End: childY += contentBounds.height - childSize.height; break;
        case Alignment::Stretch: childSize.height = contentBounds.height; break;
        case Alignment::Baseline:
        case Alignment::Start: break;
        }
        child->layout({childX, childY, childSize.width, childSize.height});
    }
    clearLayoutDirtyRecursively();
}

void Container::paint(PaintContext& context)
{
    if (background_.a > 0) {
        context.fillRoundRect(bounds(), radius_, background_);
    }
    ContainerNode::paint(context);
    clearDirty(DirtyFlag::Paint);
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
    const Constraints inner = constraints.deflate(padding_);
    float width = 0.0f;
    float height = 0.0f;
    float maxBaseline = 0.0f;
    float maxBelowBaseline = 0.0f;

    const auto& childNodes = children();
    for (std::size_t index = 0; index < childNodes.size(); ++index) {
        const float usedGaps = gap_ * static_cast<float>(childNodes.size() > 0 ? childNodes.size() - 1 : 0);
        const float remainingWidth = std::max(0.0f, inner.maxWidth - width - usedGaps);
        const auto childSize = childNodes[index]->measure({0.0f, remainingWidth, 0.0f, inner.maxHeight});
        width += childSize.width;
        height = std::max(height, childSize.height);
        if (align_ == Alignment::Baseline) {
            const float baseline = childNodes[index]->baselineOffset();
            if (baseline >= 0.0f) {
                maxBaseline = std::max(maxBaseline, baseline);
                maxBelowBaseline = std::max(maxBelowBaseline, childSize.height - baseline);
            } else {
                maxBelowBaseline = std::max(maxBelowBaseline, childSize.height);
            }
        }
        if (index + 1 < childNodes.size()) {
            width += gap_;
        }
    }

    if (align_ == Alignment::Baseline) {
        height = std::max(height, maxBaseline + maxBelowBaseline);
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

    float rowBaseline = 0.0f;
    if (align_ == Alignment::Baseline) {
        for (std::size_t i = 0; i < childNodes.size(); ++i) {
            if (childNodes[i]->flex() <= 0.0f) {
                rowBaseline = std::max(rowBaseline, childNodes[i]->baselineOffset());
            }
        }
    }

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
        case Alignment::Baseline: {
            const float baseline = child->baselineOffset();
            if (baseline >= 0.0f) {
                childY += rowBaseline - baseline;
            } else {
                childY += std::max(0.0f, rowBaseline - childSize.height);
            }
            break;
        }
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
    clearLayoutDirtyRecursively();
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
    const Constraints inner = constraints.deflate(padding_);
    float width = 0.0f;
    float height = 0.0f;

    const auto& childNodes = children();
    for (std::size_t index = 0; index < childNodes.size(); ++index) {
        const float remainingHeight = std::max(0.0f, inner.maxHeight - height);
        const auto childSize = childNodes[index]->measure({0.0f, inner.maxWidth, 0.0f, remainingHeight});
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
        case Alignment::Baseline:
            break;
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
    clearLayoutDirtyRecursively();
}

} // namespace wui
