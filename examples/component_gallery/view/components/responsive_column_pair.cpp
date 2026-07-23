#include "responsive_column_pair.h"

#include <algorithm>

namespace whatsui::gallery::view::components {

ResponsiveColumnPair& ResponsiveColumnPair::gap(float value) noexcept
{
    setGap(value);
    return *this;
}

void ResponsiveColumnPair::setGap(float value) noexcept
{
    const float next = std::max(0.0f, value);
    if (gap_ == next) return;
    gap_ = next;
    markDirty(wui::DirtyFlag::Layout);
}

ResponsiveColumnPair& ResponsiveColumnPair::align(wui::Alignment value) noexcept
{
    setAlign(value);
    return *this;
}

void ResponsiveColumnPair::setAlign(wui::Alignment value) noexcept
{
    if (alignment_ == value) return;
    alignment_ = value;
    markDirty(wui::DirtyFlag::Layout);
}

wui::SizeF ResponsiveColumnPair::measure(const wui::Constraints& constraints) const
{
    if (children().empty()) return constraints.clamp({0.0f, 0.0f});
    const bool narrow = compact(constraints.maxWidth);
    float width = 0.0f;
    float height = 0.0f;
    const float childWidth = narrow
        ? constraints.maxWidth
        : std::max(0.0f, (constraints.maxWidth - gap_) / 2.0f);
    for (std::size_t index = 0; index < children().size(); ++index) {
        const auto size = children()[index]->measureWithConstraints(
            {0.0f, childWidth, 0.0f, constraints.maxHeight});
        if (narrow) {
            width = std::max(width, size.width);
            height += size.height;
            if (index != 0) height += gap_;
        } else {
            width += size.width;
            if (index != 0) width += gap_;
            height = std::max(height, size.height);
        }
    }
    return constraints.clamp({width, height});
}

void ResponsiveColumnPair::layout(const wui::RectF& bounds)
{
    wui::Node::layout(bounds);
    const bool narrow = compact(bounds.width);
    const float childWidth = narrow
        ? bounds.width
        : std::max(0.0f, (bounds.width - gap_) / 2.0f);
    float cursor = narrow ? bounds.y : bounds.x;
    for (const auto& child : children()) {
        const auto size = child->measureWithConstraints(
            {0.0f, childWidth, 0.0f, bounds.height});
        if (narrow) {
            child->layout({bounds.x, cursor, bounds.width, size.height});
            cursor += size.height + gap_;
        } else {
            const float childHeight = alignment_ == wui::Alignment::Stretch
                ? bounds.height : std::min(bounds.height, size.height);
            child->layout({cursor, bounds.y, childWidth, childHeight});
            cursor += childWidth + gap_;
        }
    }
    clearLayoutDirtyRecursively();
}

bool ResponsiveColumnPair::compact(float width) const noexcept
{
    return width < 720.0f;
}

} // namespace whatsui::gallery::view::components
