#include "wui/events.h"

#include "wui/node.h"
#include "wui/text_input.h"

#include <algorithm>
#include <iterator>
#include <vector>

namespace wui {

void FocusManager::setFocused(Node* node) noexcept
{
    if (auto* control = dynamic_cast<ControlNode*>(node); control != nullptr && !control->isEnabled()) {
        node = nullptr;
    }
    if (focused_ == node) {
        return;
    }

    if (auto* control = dynamic_cast<ControlNode*>(focused_)) {
        control->setVisualState(ControlVisualState::Focused, false);
    }

    focused_ = node;

    if (auto* control = dynamic_cast<ControlNode*>(focused_)) {
        control->setVisualState(ControlVisualState::Focused, true);
    }
}

Node* FocusManager::focused() const noexcept
{
    return focused_;
}

void FocusManager::clear() noexcept
{
    setFocused(nullptr);
}

bool FocusManager::focusNext(Node* root, bool reverse) noexcept
{
    if (root == nullptr) {
        clear();
        return false;
    }

    std::vector<Node*> candidates;
    const auto collect = [&candidates](const auto& self, Node* node) -> void {
        if (auto* control = dynamic_cast<ControlNode*>(node); control != nullptr && control->isEnabled()) {
            candidates.push_back(node);
        }
        for (const auto& child : node->children()) {
            self(self, child.get());
        }
    };
    collect(collect, root);
    if (candidates.empty()) {
        clear();
        return false;
    }

    auto current = std::find(candidates.begin(), candidates.end(), focused_);
    if (current == candidates.end()) {
        setFocused(reverse ? candidates.back() : candidates.front());
        return true;
    }

    if (reverse) {
        setFocused(current == candidates.begin() ? candidates.back() : *std::prev(current));
    } else {
        setFocused(std::next(current) == candidates.end() ? candidates.front() : *std::next(current));
    }
    return true;
}

InputRouter::InputRouter(FocusManager* focusManager) noexcept
    : focusManager_(focusManager)
{
}

void InputRouter::setRoot(Node* root) noexcept
{
    // Changing the routed tree invalidates any in-flight gesture even when
    // the old subtree is retained elsewhere. Never deliver its eventual Up
    // into a different page.
    cancelPointerCapture();
    root_ = root;
    clearHover();
}

void InputRouter::clearHover() noexcept
{
    hovered_ = nullptr;
}

bool InputRouter::capturePointer(Node* target) noexcept
{
    if (target == nullptr) {
        return false;
    }
    if (captureState_->target == target) {
        return true;
    }

    cancelPointerCapture();
    captureState_->target = target;

    // Node detach is synchronous. The weak state keeps this callback safe
    // after an InputRouter has been destroyed while a retained node outlives
    // it (for example during UiWindow teardown).
    const std::weak_ptr<CaptureState> state = captureState_;
    target->addDetachCallback([state] {
        if (const auto locked = state.lock()) {
            InputRouter::cancelCaptureState(locked);
        }
    });
    return true;
}

void InputRouter::releasePointer(Node* target) noexcept
{
    if (target == nullptr || captureState_->target == target) {
        captureState_->target = nullptr;
    }
}

void InputRouter::cancelPointerCapture() noexcept
{
    cancelCaptureState(captureState_);
}

void InputRouter::cancelCaptureState(const std::shared_ptr<CaptureState>& state) noexcept
{
    Node* const target = state->target;
    if (target == nullptr) {
        return;
    }

    // Clear first: a Cancel handler can dismiss an overlay, detach itself, or
    // initiate another gesture without observing stale capture state.
    state->target = nullptr;
    PointerEvent cancel;
    cancel.action = PointerAction::Cancel;
    target->onPointerEvent(cancel);
}

Node* InputRouter::root() const noexcept
{
    return root_;
}

Node* InputRouter::hovered() const noexcept
{
    return hovered_;
}

Node* InputRouter::capturedPointer() const noexcept
{
    return captureState_->target;
}

Node* InputRouter::hitTest(PointF point) const
{
    return root_ != nullptr ? root_->hitTest(point) : nullptr;
}

bool InputRouter::dispatchPointer(const PointerEvent& event)
{
    return dispatchPointerTo(hitTest(event.position), event);
}

bool InputRouter::dispatchPointerTo(Node* target, const PointerEvent& event)
{
    if (event.action == PointerAction::Cancel) {
        cancelPointerCapture();
        return true;
    }

    // A native Down begins a fresh sequence. Do not permit a lost Up from a
    // previous sequence to keep routing into a stale target.
    if (event.action == PointerAction::Down && captureState_->target != nullptr) {
        cancelPointerCapture();
    }

    // Hover follows physical hit testing even while delivery is captured;
    // controls can therefore clear hover when the pointer leaves their bounds
    // while still receiving Move/Up to finish the gesture.
    Node* const hitTarget = target;

    // A disabled composite disables its entire subtree. Keep this guard at
    // routing level so every current and future ControlNode gets the same
    // pointer safety without requiring each widget to duplicate it.
    Node* deliveryTarget = captureState_->target != nullptr ? captureState_->target : hitTarget;
    for (Node* current = deliveryTarget; current != nullptr; current = current->parent()) {
        if (auto* control = dynamic_cast<ControlNode*>(current); control != nullptr && !control->isEnabled()) {
            if (event.action == PointerAction::Down && focusManager_ != nullptr) {
                focusManager_->clear();
            }
            cancelPointerCapture();
            return false;
        }
    }

    if (hitTarget != hovered_) {
        if (hovered_ != nullptr) {
            auto leaveEvent = event;
            leaveEvent.action = PointerAction::Leave;
            EventContext context(EventPhase::Target, hovered_, hovered_);
            hovered_->onPointerEvent(leaveEvent, context);
        }

        if (hitTarget != nullptr) {
            auto enterEvent = event;
            enterEvent.action = PointerAction::Enter;
            EventContext context(EventPhase::Target, hitTarget, hitTarget);
            hitTarget->onPointerEvent(enterEvent, context);
        }

        hovered_ = hitTarget;
    }

    if (deliveryTarget == nullptr) {
        if (event.action == PointerAction::Down && focusManager_ != nullptr) {
            focusManager_->clear();
        }
        return false;
    }

    if (event.action == PointerAction::Down && focusManager_ != nullptr) {
        focusManager_->setFocused(deliveryTarget);
    }

    // Snapshot the path once. Deferred structural mutations make this safe
    // for the duration of dispatch and make the three phases deterministic.
    std::vector<Node*> path;
    for (Node* current = deliveryTarget; current != nullptr; current = current->parent()) {
        path.push_back(current);
    }
    std::reverse(path.begin(), path.end());

    bool handled = false;
    bool explicitCaptureRequest = false;
    const auto dispatchPhase = [&](Node* current, EventPhase phase) {
        EventContext context(phase, deliveryTarget, current);
        const EventResult result = current->onPointerEvent(event, context);
        switch (result) {
        case EventResult::Handled:
            handled = true;
            break;
        case EventResult::StopPropagation:
            handled = true;
            context.stopPropagation();
            break;
        case EventResult::RequestFocus:
            handled = true;
            context.requestFocus();
            break;
        case EventResult::CapturePointer:
            handled = true;
            context.capturePointer();
            break;
        case EventResult::ReleasePointer:
            handled = true;
            context.releasePointer();
            break;
        case EventResult::Ignored:
            break;
        }

        if (context.isFocusRequested()) {
            handled = true;
            if (focusManager_ != nullptr) {
                focusManager_->setFocused(context.requestedFocus());
            }
        }
        if (context.pointerCaptureRequest() == PointerCaptureRequest::Capture) {
            handled = true;
            explicitCaptureRequest = true;
            (void)capturePointer(current);
        } else if (context.pointerCaptureRequest() == PointerCaptureRequest::Release) {
            handled = true;
            explicitCaptureRequest = true;
            releasePointer(current);
        }
        if (context.isPropagationStopped()) {
            handled = true;
            return true;
        }
        return false;
    };
    // A propagation stop is about ancestor delivery, not gesture lifetime.
    // In particular, an Up which is consumed in Capture or Target must still
    // release the active left-button capture.
    const auto finishDispatch = [&] {
        if (event.action == PointerAction::Up && event.button == MouseButton::Left) {
            releasePointer();
        }
        return handled;
    };

    // Capture: root to the parent of the delivery target.
    for (std::size_t index = 0; index + 1 < path.size(); ++index) {
        if (dispatchPhase(path[index], EventPhase::Capture)) {
            return finishDispatch();
        }
    }

    if (dispatchPhase(deliveryTarget, EventPhase::Target)) {
        return finishDispatch();
    }

    // Bubble: target parent to root. Legacy bool handlers are invoked here,
    // preserving the old target-then-ancestor behavior exactly.
    for (std::size_t index = path.size() - 1; index > 0; --index) {
        if (dispatchPhase(path[index - 1], EventPhase::Bubble)) {
            handled = true;
            return finishDispatch();
        }
    }

    // Default capture is intentionally tied to handled primary Down, rather
    // than every hit. This preserves passive hit-test nodes and makes buttons,
    // checkboxes, and future drag controls robust when the pointer leaves.
    if (event.action == PointerAction::Down && event.button == MouseButton::Left && handled && !explicitCaptureRequest) {
        (void)capturePointer(deliveryTarget);
    }
    return finishDispatch();
}

bool InputRouter::dispatchKey(const KeyEvent& event)
{
    if (focusManager_ == nullptr) {
        return false;
    }

    // GLFW reports these as 258/257, while several native hosts use the
    // conventional virtual-key values 9/13. Accept both until KeyEvent gains
    // a backend-independent key enum.
    const bool isTab = event.keyCode == 9 || event.keyCode == 258;
    if (event.action == KeyAction::Down && isTab) {
        return focusManager_->focusNext(root_, (event.modifiers & KeyModifierShift) != 0);
    }

    Node* focused = focusManager_->focused();
    if (focused == nullptr) {
        return false;
    }
    if (auto* control = dynamic_cast<ControlNode*>(focused); control != nullptr && !control->isEnabled()) {
        focusManager_->clear();
        return false;
    }

    if (event.action == KeyAction::Down && (event.keyCode == 32 || event.keyCode == 13 || event.keyCode == 257)) {
        // Controls already express their activation behavior through pointer
        // events. Synthesizing a complete click preserves that single path
        // for Button and forthcoming controls without widget-specific casts.
        // TextInput is focusable but Enter/Space remain text-editing keys;
        // it must not receive a synthetic pointer click.
        if (dynamic_cast<ControlNode*>(focused) != nullptr && dynamic_cast<TextInput*>(focused) == nullptr) {
            PointerEvent down;
            down.action = PointerAction::Down;
            down.button = MouseButton::Left;
            down.position = {focused->bounds().x, focused->bounds().y};
            PointerEvent up = down;
            up.action = PointerAction::Up;
            return focused->onPointerEvent(down) | focused->onPointerEvent(up);
        }
    }

    return focused->onKeyEvent(event);
}

bool InputRouter::dispatchTextInput(const TextInputEvent& event)
{
    if (focusManager_ == nullptr || focusManager_->focused() == nullptr) {
        return false;
    }
    return focusManager_->focused()->onTextInput(event);
}

bool InputRouter::dispatchComposition(const CompositionInputEvent& event)
{
    if (focusManager_ == nullptr || focusManager_->focused() == nullptr) {
        return false;
    }
    return focusManager_->focused()->onCompositionInput(event);
}

} // namespace wui
