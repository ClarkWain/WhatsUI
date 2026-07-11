#include "wui/widgets.h"

#include <algorithm>
#include <limits>

namespace wui {

ScrollView& ScrollView::child(std::unique_ptr<Node> child)
{
    clearChildren();
    appendChild(std::move(child));
    return *this;
}

void ScrollView::setScrollOffset(float offset) noexcept
{
    scrollOffset_ = std::max(0.0f, offset);
    clampOffset();
    markDirty(DirtyFlag::Paint);
}

float ScrollView::scrollOffset() const noexcept { return scrollOffset_; }
float ScrollView::maxScrollOffset() const noexcept { return std::max(0.0f, contentSize_.height - bounds().height); }
SizeF ScrollView::contentSize() const noexcept { return contentSize_; }

SizeF ScrollView::measure(const Constraints& constraints) const
{
    if (children().empty()) return constraints.clamp({});
    // Content is measured with an unbounded vertical main axis; the incoming
    // constraints determine the viewport size, not the document height.
    const SizeF content = children().front()->measure(
        {0.0f, constraints.maxWidth, 0.0f, std::numeric_limits<float>::infinity()});
    return constraints.clamp(content);
}

void ScrollView::layout(const RectF& bounds)
{
    Node::layout(bounds);
    contentSize_ = {};
    if (!children().empty()) {
        Node* content = children().front().get();
        contentSize_ = content->measure({0.0f, bounds.width, 0.0f, std::numeric_limits<float>::infinity()});
        // Keep content in its document coordinate system. Paint and hit test
        // apply the viewport translation, avoiding accumulated rounding.
        content->layout({bounds.x, bounds.y, bounds.width, contentSize_.height});
    }
    clampOffset();
    clearLayoutDirtyRecursively();
}

void ScrollView::paint(PaintContext& context)
{
    (void)context.save();
    context.clipRect(bounds());
    context.translate(0.0f, -scrollOffset_);
    for (const auto& child : children()) child->paint(context);
    context.restore();
    clearDirty(DirtyFlag::Paint);
}

Node* ScrollView::hitTest(PointF point)
{
    if (!bounds().contains(point)) return nullptr;
    const PointF documentPoint{point.x, point.y + scrollOffset_};
    for (auto it = children().rbegin(); it != children().rend(); ++it) {
        if (Node* hit = (*it)->hitTest(documentPoint)) return hit;
    }
    return this;
}

bool ScrollView::onPointerEvent(const PointerEvent& event)
{
    if (event.action != PointerAction::Scroll || event.scrollDelta.y == 0.0f) return false;
    const float previous = scrollOffset_;
    // Positive wheel delta conventionally means scrolling up.
    setScrollOffset(scrollOffset_ - event.scrollDelta.y);
    return scrollOffset_ != previous;
}

void ScrollView::clampOffset() noexcept
{
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maxScrollOffset());
}

} // namespace wui
