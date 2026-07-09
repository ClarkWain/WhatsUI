#include "wui/node.h"

#include <stdexcept>

namespace wui {

Node::~Node()
{
    for (auto& callback : teardown_) {
        if (callback) {
            callback();
        }
    }
}

void Node::appendChild(std::unique_ptr<Node> child)
{
    if (!child) {
        throw std::invalid_argument("child must not be null");
    }
    child->parent_ = this;
    children_.push_back(std::move(child));
    markDirty(DirtyFlag::Layout);
}

std::unique_ptr<Node> Node::removeChild(std::size_t index)
{
    if (index >= children_.size()) {
        throw std::out_of_range("child index out of range");
    }

    auto child = std::move(children_[index]);
    child->parent_ = nullptr;
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
            child->parent_ = nullptr;
        }
    }
    children_.clear();
    markDirty(DirtyFlag::Layout);
}

void Node::addTeardown(std::function<void()> callback)
{
    teardown_.push_back(std::move(callback));
}

void Node::layout(const RectF& bounds)
{
    bounds_ = bounds;
    clearDirty(DirtyFlag::Layout);
}

Node* Node::hitTest(PointF point)
{
    return bounds_.contains(point) ? this : nullptr;
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
    if (parent_ != nullptr) {
        parent_->markDirty(flag);
    }
}

void ContainerNode::paint(PaintContext& context)
{
    for (const auto& child : children()) {
        child->paint(context);
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
