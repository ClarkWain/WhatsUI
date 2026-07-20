#include "wui/events.h"

#include "wui/basic_controls.h"
#include "wui/node.h"
#include "wui/text_input.h"

#include <algorithm>
#include <iterator>
#include <vector>

namespace wui {

void FocusManager::clearFocusIfCurrent(const std::weak_ptr<FocusState>& weakState, Node* node,
                                       bool clearVisualState) noexcept
{
    const auto state = weakState.lock();
    if (!state || state->focused != node) {
        return;
    }

    // Clear the non-owning pointer before touching node state. A detach hook
    // can invalidate its window while paint invalidation propagates, so no
    // re-entrant caller may observe the departing node as focused.
    state->focused = nullptr;
    state->focusVisible = false;
    if (clearVisualState) {
        if (auto* control = dynamic_cast<ControlNode*>(node)) {
            control->setVisualState(ControlVisualState::Focused, false);
            control->setVisualState(ControlVisualState::FocusVisible, false);
        }
    }
}

void FocusManager::setFocused(Node* node, bool focusVisible) noexcept
{
    if (auto* control = dynamic_cast<ControlNode*>(node); control != nullptr && !control->isEnabled()) {
        node = nullptr;
    }
    if (state_->focused == node) {
        setFocusVisible(node != nullptr && focusVisible);
        return;
    }

    if (auto* control = dynamic_cast<ControlNode*>(state_->focused)) {
        control->setVisualState(ControlVisualState::Focused, false);
        control->setVisualState(ControlVisualState::FocusVisible, false);
    }

    state_->focused = node;
    state_->focusVisible = node != nullptr && focusVisible;

    if (node != nullptr) {
        const std::weak_ptr<FocusState> weakState = state_;
        // Detach is the normal keyed-reconciliation path. It runs while the
        // concrete ControlNode is still alive, so its visual state can be
        // cleared as well as the raw focus pointer.
        node->addDetachCallback([weakState, node] {
            clearFocusIfCurrent(weakState, node, true);
        });
        // A detached node can also be directly owned and destroyed without
        // ever receiving detachRecursively(). Clear the raw pointer in that
        // final fallback too. The derived control is already gone here, so do
        // not inspect or mutate its visual state.
        node->addTeardown([weakState, node] {
            clearFocusIfCurrent(weakState, node, false);
        });
    }

    if (auto* control = dynamic_cast<ControlNode*>(state_->focused)) {
        control->setVisualState(ControlVisualState::Focused, true);
        control->setVisualState(ControlVisualState::FocusVisible, state_->focusVisible);
    }
}

void FocusManager::setFocusVisible(bool visible) noexcept
{
    visible = state_->focused != nullptr && visible;
    if (state_->focusVisible == visible) return;
    state_->focusVisible = visible;
    if (auto* control = dynamic_cast<ControlNode*>(state_->focused)) {
        control->setVisualState(ControlVisualState::FocusVisible, visible);
    }
}

Node* FocusManager::focused() const noexcept
{
    return state_->focused;
}

bool FocusManager::isFocusVisible() const noexcept
{
    return state_->focusVisible;
}

void FocusManager::clear() noexcept
{
    setFocused(nullptr, false);
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

    auto current = std::find(candidates.begin(), candidates.end(), state_->focused);
    if (current == candidates.end()) {
        setFocused(reverse ? candidates.back() : candidates.front(), true);
        return true;
    }

    if (reverse) {
        setFocused(current == candidates.begin() ? candidates.back() : *std::prev(current), true);
    } else {
        setFocused(std::next(current) == candidates.end() ? candidates.front() : *std::next(current), true);
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
    // Do not synthesize Leave here. This method is also called by teardown
    // paths, where the old node may already have been destroyed.
    hoverState_->target = nullptr;
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

void InputRouter::clearHoverIfCurrent(const std::weak_ptr<HoverState>& weakState, Node* node) noexcept
{
    const auto state = weakState.lock();
    if (state && state->target == node) {
        // Clear before any later input dispatch can synthesize Leave. The
        // detach callback runs while the node is still alive, but the next
        // native event may arrive after its owning unique_ptr is released.
        state->target = nullptr;
    }
}

void InputRouter::setHovered(Node* target) noexcept
{
    hoverState_->target = target;
    if (target == nullptr) {
        return;
    }

    // Mirror pointer capture's weak-state pattern. A retained node may
    // outlive its InputRouter during window teardown, so this callback must
    // never capture `this`.
    const std::weak_ptr<HoverState> state = hoverState_;
    target->addDetachCallback([state, target] {
        InputRouter::clearHoverIfCurrent(state, target);
    });
}

Node* InputRouter::root() const noexcept
{
    return root_;
}

Node* InputRouter::hovered() const noexcept
{
    return hoverState_->target;
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

    Node* const previousHover = hoverState_->target;
    if (hitTarget != previousHover) {
        // A Leave handler can remove or destroy itself. Clear the state
        // before invoking it so re-entrant dispatch never observes a stale
        // node, then register the new target before Enter for the same reason.
        clearHover();
        if (previousHover != nullptr) {
            auto leaveEvent = event;
            leaveEvent.action = PointerAction::Leave;
            EventContext context(EventPhase::Target, previousHover, previousHover);
            previousHover->onPointerEvent(leaveEvent, context);
        }

        if (hitTarget != nullptr) {
            setHovered(hitTarget);
            auto enterEvent = event;
            enterEvent.action = PointerAction::Enter;
            EventContext context(EventPhase::Target, hitTarget, hitTarget);
            hitTarget->onPointerEvent(enterEvent, context);
        }
    }

    if (deliveryTarget == nullptr) {
        if (event.action == PointerAction::Down && focusManager_ != nullptr) {
            focusManager_->clear();
        }
        return false;
    }

    if (event.action == PointerAction::Down && focusManager_ != nullptr) {
        focusManager_->setFocused(deliveryTarget, false);
    }

    // Each phase sees the same mutable routed event. Scroll handlers can
    // reduce scrollDelta through EventContext, allowing the unconsumed part
    // to continue through the bubble path.
    PointerEvent routedEvent = event;

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
        EventContext context(phase, deliveryTarget, current, &routedEvent);
        const EventResult result = current->onPointerEvent(routedEvent, context);
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
                focusManager_->setFocused(context.requestedFocus(), false);
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
    if (event.action == KeyAction::Down) {
        // A control reached by pointer keeps logical focus without an outline.
        // Its first keyboard interaction switches modality and reveals the
        // same focus owner, as :focus-visible does on the Web.
        focusManager_->setFocusVisible(true);
    }

    // Give each widget first refusal: selection and range controls own their
    // precise keyboard contract, and text controls must keep editing keys.
    if (focused->onKeyEvent(event)) {
        // Radio groups use arrow keys to change the active option. Fluent
        // requires the roving tab stop to follow that selection, rather than
        // leaving the FocusManager on the old Radio while a different option
        // is visually checked. Keep this at the router boundary because only
        // it owns the real window focus state.
        const bool isArrow = event.action == KeyAction::Down &&
            (event.keyCode == 37 || event.keyCode == 38 || event.keyCode == 39 || event.keyCode == 40);
        if (isArrow) {
            if (auto* radio = dynamic_cast<Radio*>(focused)) {
                if (auto* group = dynamic_cast<RadioGroup*>(radio->parent())) {
                    if (Radio* selected = group->selectedRadio(); selected != nullptr) {
                        focusManager_->setFocused(selected, true);
                    }
                }
            }
        }
        return true;
    }

    // Plain Buttons deliberately share their programmatic Invoke path instead
    // of receiving a fake pointer gesture at the control's top-left corner.
    // This avoids accidental Slider jumps and Enter toggles on controls whose
    // keyboard contract only accepts Space.
    const bool isActivation = event.action == KeyAction::Down &&
        (event.keyCode == 32 || event.keyCode == 13 || event.keyCode == 257);
    if (isActivation && focused->accessibilityActions().invoke) {
        return focused->performAccessibilityAction(AccessibilityActionKind::Invoke, {}) ==
               AccessibilityActionStatus::Succeeded;
    }
    return false;
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
