#include "wui/node.h"

#include <stdexcept>

namespace wui {

Node::~Node()
{
    // UiRoot/OverlayHost normally detach their content before releasing it.
    // Keep this fallback for direct owners so subscriptions registered through
    // addDetachCallback cannot outlive an attached tree by accident.
    detachRecursively();
    for (auto& callback : teardown_) {
        if (callback) {
            callback();
        }
    }
}

void Node::prepare(PaintContext& context)
{
    for (const auto& child : children_) {
        child->prepare(context);
    }
}

float Node::baselineOffset() const noexcept
{
    return -1.0f;
}

SizeF Node::measureWithConstraints(const Constraints& constraints) const
{
    lastMeasuredConstraints_ = constraints;
    return measure(constraints);
}

void Node::appendChild(std::unique_ptr<Node> child)
{
    if (!child) {
        throw std::invalid_argument("child must not be null");
    }
    child->parent_ = this;
    child->setInvalidationHandler(invalidationHandler_);
    Node* const rawChild = child.get();
    children_.push_back(std::move(child));
    if (attached_) {
        rawChild->attachRecursively();
    }
    markDirty(DirtyFlag::Layout);
}

std::unique_ptr<Node> Node::removeChild(std::size_t index)
{
    if (index >= children_.size()) {
        throw std::out_of_range("child index out of range");
    }

    auto child = std::move(children_[index]);
    child->detachRecursively();
    child->parent_ = nullptr;
    child->setInvalidationHandler({});
    children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(index));
    markDirty(DirtyFlag::Layout);
    return child;
}

void Node::clearChildren()
{
    if (children_.empty()) {
        return;
    }
    for (auto& child : children_) {
        if (child) {
            child->detachRecursively();
            child->parent_ = nullptr;
            child->setInvalidationHandler({});
        }
    }
    children_.clear();
    markDirty(DirtyFlag::Layout);
}

void Node::addTeardown(std::function<void()> callback)
{
    teardown_.push_back(std::move(callback));
}

void Node::addAttachCallback(std::function<void()> callback)
{
    if (!callback) {
        return;
    }
    attachCallbacks_.push_back(std::move(callback));
    if (attached_) {
        attachCallbacks_.back()();
    }
}

void Node::addDetachCallback(std::function<void()> callback)
{
    if (callback) {
        detachCallbacks_.push_back(std::move(callback));
    }
}

void Node::attachRecursively()
{
    if (attached_) {
        return;
    }
    attached_ = true;
    onAttach();
    for (auto& callback : attachCallbacks_) {
        callback();
    }
    for (const auto& child : children_) {
        child->attachRecursively();
    }
}

void Node::detachRecursively() noexcept
{
    if (!attached_) {
        return;
    }

    // Descendants detach first: their callback can still inspect their
    // parent during cleanup, but no descendant can observe the parent after
    // its own detach callback has run.
    for (const auto& child : children_) {
        child->detachRecursively();
    }
    for (auto& callback : detachCallbacks_) {
        callback();
    }
    onDetach();
    attached_ = false;
}

void Node::setInvalidationHandler(std::function<void()> handler)
{
    invalidationHandler_ = std::move(handler);
    for (const auto& child : children_) {
        child->setInvalidationHandler(invalidationHandler_);
    }
}

void Node::layout(const RectF& bounds)
{
    bounds_ = bounds;
    clearDirty(DirtyFlag::Layout);
}

void Node::clearLayoutDirtyRecursively() noexcept
{
    clearDirty(DirtyFlag::Layout);
    for (const auto& child : children_) {
        child->clearLayoutDirtyRecursively();
    }
}

Node* Node::hitTest(PointF point)
{
    return bounds_.contains(point) ? this : nullptr;
}

EventResult Node::onPointerEvent(const PointerEvent& event, EventContext& context)
{
    // The legacy callback historically ran on the hit target and then during
    // bubbling. Do not invoke it during the newly introduced capture phase:
    // existing ScrollView/Dialog/Button implementations must not observe the
    // same gesture twice simply because the router gained capture semantics.
    if (context.phase() == EventPhase::Capture) {
        return EventResult::Ignored;
    }
    return onPointerEvent(event) ? EventResult::Handled : EventResult::Ignored;
}

bool Node::onPointerEvent(const PointerEvent& event)
{
    (void)event;
    return false;
}

bool Node::onKeyEvent(const KeyEvent& event)
{
    (void)event;
    return false;
}

bool Node::onTextInput(const TextInputEvent& event)
{
    (void)event;
    return false;
}

bool Node::onCompositionInput(const CompositionInputEvent& event)
{
    (void)event;
    return false;
}

void Node::markDirty(DirtyFlag flag) noexcept
{
    dirtyFlags_ |= toMask(flag);
    // Geometry is part of the rendered output.  Keeping layout and paint
    // invalidation coupled here prevents a relaid-out subtree from appearing
    // clean to a paint-only frame scheduler.
    if (flag == DirtyFlag::Layout) {
        dirtyFlags_ |= toMask(DirtyFlag::Paint);
    }
    if (parent_ != nullptr) {
        parent_->markDirty(flag);
    } else if (invalidationHandler_) {
        invalidationHandler_();
    }
}

void ContainerNode::paint(PaintContext& context)
{
    for (const auto& child : children()) {
        // Paint state is deliberately isolated at every tree edge.  A child
        // may establish a viewport clip or transform while painting; the next
        // sibling must never inherit it.  Apart from making custom widgets
        // safer, this is essential for structural nodes: a branch can be
        // unmounted between frames, so a stale clip left by that branch would
        // otherwise constrain newly mounted siblings in the following frame.
        //
        // Well-behaved built-in widgets already balance their own save/restore
        // pairs.  This outer guard is therefore pixel-neutral for them and is
        // a containment boundary for third-party nodes.
        const int paintState = context.save();
        child->paint(context);
        context.restoreTo(paintState);
    }
    clearDirty(DirtyFlag::Paint);
}

Node* ContainerNode::hitTest(PointF point)
{
    if (!bounds().contains(point)) {
        return nullptr;
    }

    for (auto it = children().rbegin(); it != children().rend(); ++it) {
        if (Node* hit = (*it)->hitTest(point)) {
            return hit;
        }
    }

    return this;
}

void ControlNode::setEnabled(bool enabled) noexcept
{
    setVisualState(ControlVisualState::Disabled, !enabled);
}

void ControlNode::setVisualState(ControlVisualState state, bool value) noexcept
{
    const auto mask = toMask(state);
    if (value) {
        visualStates_ |= mask;
    } else {
        visualStates_ &= ~mask;
    }
    markDirty(DirtyFlag::Paint);
}

} // namespace wui
