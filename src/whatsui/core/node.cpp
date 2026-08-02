#include "wui/node.h"
#include "wui/thread_check.h"

#include <stdexcept>
#include <unordered_map>

namespace wui {

Node::~Node()
{
    // UiRoot and OverlayHost detach owned trees before destruction, while the
    // concrete dynamic types are still alive. Calling virtual detach hooks
    // from this base destructor would dispatch after derived teardown and can
    // make a child callback re-enter an owner whose vptr is already `Node`.
    for (auto& callback : teardown_) {
        if (callback) {
            try {
                callback();
            } catch (const std::exception& error) {
                reportLifecycleException("teardown", error.what());
            } catch (...) {
                reportLifecycleException("teardown");
            }
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

void Node::appendChild(NodePtr child)
{
    requireTreeMutationThread();
    insertChild(children_.size(), std::move(child));
}

void Node::appendChildren(std::vector<NodePtr> children)
{
    requireTreeMutationThread();
    const std::size_t resultingCount = children_.size() + children.size();
    for (std::size_t index = 0; index < children.size(); ++index) {
        const auto& child = children[index];
        if (!child) {
            throw std::invalid_argument("child must not be null");
        }
        validateChildInsertion(
            *child, children_.size() + index, resultingCount);
    }

    // Reserve before mutating parent/child relationships. Invalid input and
    // normal allocation failures therefore leave this node unchanged.
    children_.reserve(children_.size() + children.size());
    for (auto& child : children) {
        child->parent_ = this;
        child->setInvalidationHandler(invalidationHandler_);
        Node* const rawChild = child.get();
        children_.push_back(std::move(child));
        if (attached_) {
            rawChild->attachRecursively(ownerContext_);
        }
    }
    if (!children.empty()) {
        if (attached_) {
            validateIdentitySubtree();
        }
        markDirty(DirtyFlag::Layout);
    }
}

void Node::insertChild(std::size_t index, NodePtr child)
{
    requireTreeMutationThread();
    if (!child) {
        throw std::invalid_argument("child must not be null");
    }
    if (index > children_.size()) {
        throw std::out_of_range("child insertion index out of range");
    }
    validateChildInsertion(*child, index, children_.size() + 1);
    child->parent_ = this;
    child->setInvalidationHandler(invalidationHandler_);
    Node* const rawChild = child.get();
    children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(index), std::move(child));
    if (attached_) {
        rawChild->attachRecursively(ownerContext_);
        validateIdentitySubtree();
    }
    markDirty(DirtyFlag::Layout);
}

void Node::moveChild(std::size_t from, std::size_t to)
{
    requireTreeMutationThread();
    if (from >= children_.size() || to >= children_.size()) {
        throw std::out_of_range("child move index out of range");
    }
    if (from == to) {
        return;
    }
    auto child = std::move(children_[from]);
    children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(from));
    children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(to), std::move(child));
    markDirty(DirtyFlag::Layout);
}

NodePtr Node::removeChild(std::size_t index)
{
    requireTreeMutationThread();
    if (index >= children_.size()) {
        throw std::out_of_range("child index out of range");
    }

    auto child = std::move(children_[index]);
    children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(index));
    child->detachRecursively();
    child->parent_ = nullptr;
    child->setInvalidationHandler({});
    markDirty(DirtyFlag::Layout);
    return child;
}

void Node::clearChildren()
{
    requireTreeMutationThread();
    if (children_.empty()) {
        return;
    }
    auto children = std::move(children_);
    children_.clear();
    for (auto& child : children) {
        if (child) {
            child->detachRecursively();
            child->parent_ = nullptr;
            child->setInvalidationHandler({});
        }
    }
    markDirty(DirtyFlag::Layout);
}

void Node::addTeardown(std::function<void()> callback)
{
    requireTreeMutationThread();
    teardown_.push_back(std::move(callback));
}

void Node::validateChildInsertion(
    const Node& child,
    std::size_t index,
    std::size_t resultingCount) const
{
    (void)child;
    (void)index;
    (void)resultingCount;
}

void Node::setAutomationId(std::string id)
{
    requireTreeMutationThread();
    automationId_ = std::move(id);
    if (attached_) {
        validateIdentitySubtree();
    }
}

void Node::addAttachCallback(std::function<void()> callback)
{
    requireTreeMutationThread();
    if (!callback) {
        return;
    }
    attachCallbacks_.push_back(std::move(callback));
    if (attached_) {
        try {
            attachCallbacks_.back()();
        } catch (const std::exception& error) {
            reportLifecycleException("attach", error.what());
        } catch (...) {
            reportLifecycleException("attach");
        }
    }
}

void Node::addDetachCallback(std::function<void()> callback)
{
    requireTreeMutationThread();
    if (callback) {
        detachCallbacks_.push_back(std::move(callback));
    }
}

void Node::attachRecursively(UiContext ownerContext)
{
    if (attached_) {
        return;
    }
    ownerContext_ = std::move(ownerContext);
    diagnosticContext_ = ownerContext_;
    ownerThread_ = std::this_thread::get_id();
    attached_ = true;
    onAttach();
    for (auto& callback : attachCallbacks_) {
        try {
            callback();
        } catch (const std::exception& error) {
            reportLifecycleException("attach", error.what());
        } catch (...) {
            reportLifecycleException("attach");
        }
    }
    for (const auto& child : children_) {
        child->attachRecursively(ownerContext_);
    }
    if (parent_ == nullptr) {
        validateIdentitySubtree();
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
        try {
            callback();
        } catch (const std::exception& error) {
            reportLifecycleException("detach", error.what());
        } catch (...) {
            reportLifecycleException("detach");
        }
    }
    onDetach();
    attached_ = false;
    ownerContext_ = {};
    ownerThread_ = {};
}

void Node::requireTreeMutationThread() const
{
    if (!attached_) {
        return;
    }
    if (ownerContext_.isValid()) {
        ownerContext_.requireCurrentThread();
        return;
    }
    if (ownerThread_ != std::this_thread::get_id()) {
        throw std::logic_error(
            "Attached Node mutation must run on its owning UI thread");
    }
}

void Node::setDebugName(std::string name)
{
    requireTreeMutationThread();
    debugName_ = std::move(name);
}

void Node::reportLifecycleException(
    std::string_view phase,
    std::string_view message) const noexcept
{
    try {
        std::string text = "Node ";
        text.append(phase);
        text.append(" callback threw");
        if (!message.empty()) {
            text.append(": ");
            text.append(message);
        }
        diagnosticContext_.reportDiagnostic({
            UiDiagnosticCode::LifecycleCallbackException,
            std::move(text),
            debugName_,
        });
    } catch (...) {
    }
}

void Node::validateIdentitySubtree() const noexcept
{
    try {
        const Node* root = this;
        while (root->parent_ != nullptr) {
            root = root->parent_;
        }

        std::unordered_map<std::string, const Node*> ids;
        std::function<void(const Node&)> visit = [&](const Node& node) {
            if (!node.automationId_.empty()) {
                const bool inserted =
                    ids.emplace(node.automationId_, &node).second;
                if (!inserted) {
                    root->diagnosticContext_.reportDiagnostic({
                        UiDiagnosticCode::DuplicateAutomationId,
                        "Duplicate automationId: " + node.automationId_,
                        node.debugName_,
                    });
                }
            }
            for (const auto& child : node.children_) {
                visit(*child);
            }
        };
        visit(*root);
    } catch (...) {
    }
}

void Node::setInvalidationHandler(std::function<void()> handler)
{
    requireTreeMutationThread();
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

PointF Node::mapPointToContent(PointF point) const noexcept
{
    return point;
}

EventResult Node::onPointerEvent(const PointerEvent& event, EventContext& context)
{
    // The legacy callback historically ran on the hit target and then during
    // bubbling. Do not invoke it during the newly introduced capture phase:
    // existing ScrollViewNode/DialogNode/ButtonNode implementations must not observe the
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

AccessibilityActionCapabilities Node::accessibilityActions() const noexcept
{
    return {};
}

AccessibilityActionStatus Node::performAccessibilityAction(
    AccessibilityActionKind kind, std::string_view value)
{
    (void)kind;
    (void)value;
    return AccessibilityActionStatus::NotSupported;
}

void Node::markDirty(DirtyFlag flag) noexcept
{
    try {
        requireTreeMutationThread();
    } catch (...) {
        std::terminate();
    }
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
        try {
            invalidationHandler_();
        } catch (const std::exception& error) {
            reportLifecycleException("invalidation", error.what());
        } catch (...) {
            reportLifecycleException("invalidation");
        }
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
    const auto next = value ? visualStates_ | mask : visualStates_ & ~mask;
    if (next == visualStates_) return;
    visualStates_ = next;
    markDirty(DirtyFlag::Paint);
}

} // namespace wui
