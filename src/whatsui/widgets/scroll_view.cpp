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

ScrollView& ScrollView::setAxis(ScrollAxis axis) noexcept
{
    if (axis_ != axis) {
        axis_ = axis;
        clampOffset();
        markDirty(DirtyFlag::Layout);
    }
    return *this;
}

ScrollAxis ScrollView::axis() const noexcept { return axis_; }

void ScrollView::setScrollOffset(float offset) noexcept
{
    setScrollOffset({scrollOffset_.x, offset});
}

void ScrollView::setScrollOffset(PointF offset) noexcept
{
    scrollOffset_ = offset;
    clampOffset();
    markDirty(DirtyFlag::Paint);
}

float ScrollView::scrollOffset() const noexcept { return scrollOffset_.y; }
float ScrollView::scrollOffsetX() const noexcept { return scrollOffset_.x; }
float ScrollView::scrollOffsetY() const noexcept { return scrollOffset_.y; }
float ScrollView::maxScrollOffset() const noexcept { return maxScrollOffsetY(); }
float ScrollView::maxScrollOffsetX() const noexcept { return std::max(0.0f, contentSize_.width - bounds().width); }
float ScrollView::maxScrollOffsetY() const noexcept { return std::max(0.0f, contentSize_.height - bounds().height); }
SizeF ScrollView::contentSize() const noexcept { return contentSize_; }

SizeF ScrollView::measure(const Constraints& constraints) const
{
    if (children().empty()) return constraints.clamp({});
    const bool horizontal = axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both;
    const bool vertical = axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both;
    const SizeF content = children().front()->measureWithConstraints({0.0f,
                                                        horizontal ? std::numeric_limits<float>::infinity() : constraints.maxWidth,
                                                        0.0f,
                                                        vertical ? std::numeric_limits<float>::infinity() : constraints.maxHeight});
    return constraints.clamp(content);
}

void ScrollView::layout(const RectF& bounds)
{
    Node::layout(bounds);
    contentSize_ = {};
    if (!children().empty()) {
        Node* content = children().front().get();
        const bool horizontal = axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both;
        const bool vertical = axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both;
        contentSize_ = content->measureWithConstraints({0.0f,
                                          horizontal ? std::numeric_limits<float>::infinity() : bounds.width,
                                          0.0f,
                                          vertical ? std::numeric_limits<float>::infinity() : bounds.height});
        // Keep content in its document coordinate system. Paint and hit test
        // apply the viewport translation, avoiding accumulated rounding.
        content->layout({bounds.x, bounds.y,
                         horizontal ? contentSize_.width : bounds.width,
                         vertical ? contentSize_.height : bounds.height});
    }
    clampOffset();
    clearLayoutDirtyRecursively();
}

void ScrollView::paint(PaintContext& context)
{
    (void)context.save();
    context.clipRect(bounds());
    context.translate(-scrollOffset_.x, -scrollOffset_.y);
    for (const auto& child : children()) child->paint(context);
    context.restore();
    clearDirty(DirtyFlag::Paint);
}

Node* ScrollView::hitTest(PointF point)
{
    if (!bounds().contains(point)) return nullptr;
    const PointF documentPoint{point.x + scrollOffset_.x, point.y + scrollOffset_.y};
    for (auto it = children().rbegin(); it != children().rend(); ++it) {
        if (Node* hit = (*it)->hitTest(documentPoint)) return hit;
    }
    return this;
}

EventResult ScrollView::onPointerEvent(const PointerEvent& event, EventContext& context)
{
    // Capture is observational. Consuming here would let an outer ScrollView
    // steal its child's wheel before the child reaches target/bubble.
    if (context.phase() == EventPhase::Capture || event.action != PointerAction::Scroll) return EventResult::Ignored;

    const PointF previous = scrollOffset_;
    PointF requested = previous;
    PointF remaining = event.scrollDelta;
    if (axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both) {
        requested.x -= event.scrollDelta.x;
    }
    if (axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both) {
        requested.y -= event.scrollDelta.y;
    }
    setScrollOffset(requested);

    // Positive event delta moves toward the document start; derive the
    // residual from the actual clamped offset movement for each axis.
    if (axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both) {
        remaining.x += scrollOffset_.x - previous.x;
    }
    if (axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both) {
        remaining.y += scrollOffset_.y - previous.y;
    }
    context.setRemainingScrollDelta(remaining);
    return scrollOffset_.x != previous.x || scrollOffset_.y != previous.y ? EventResult::Handled : EventResult::Ignored;
}

bool ScrollView::onPointerEvent(const PointerEvent& event)
{
    if (event.action != PointerAction::Scroll) return false;
    const PointF previous = scrollOffset_;
    PointF requested = previous;
    if (axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both) requested.x -= event.scrollDelta.x;
    if (axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both) requested.y -= event.scrollDelta.y;
    setScrollOffset(requested);
    return scrollOffset_.x != previous.x || scrollOffset_.y != previous.y;
}

void ScrollView::clampOffset() noexcept
{
    const bool horizontal = axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both;
    const bool vertical = axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both;
    scrollOffset_.x = horizontal ? std::clamp(scrollOffset_.x, 0.0f, maxScrollOffsetX()) : 0.0f;
    scrollOffset_.y = vertical ? std::clamp(scrollOffset_.y, 0.0f, maxScrollOffsetY()) : 0.0f;
}

} // namespace wui
