#include "wui/events.h"

#include "wui/node.h"

namespace wui {

void FocusManager::setFocused(Node* node) noexcept
{
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

InputRouter::InputRouter(FocusManager* focusManager) noexcept
    : focusManager_(focusManager)
{
}

void InputRouter::setRoot(Node* root) noexcept
{
    root_ = root;
}

Node* InputRouter::root() const noexcept
{
    return root_;
}

Node* InputRouter::hovered() const noexcept
{
    return hovered_;
}

Node* InputRouter::hitTest(PointF point) const
{
    return root_ != nullptr ? root_->hitTest(point) : nullptr;
}

bool InputRouter::dispatchPointer(const PointerEvent& event)
{
    Node* target = hitTest(event.position);

    if (target != hovered_) {
        if (hovered_ != nullptr) {
            auto leaveEvent = event;
            leaveEvent.action = PointerAction::Leave;
            hovered_->onPointerEvent(leaveEvent);
        }

        if (target != nullptr) {
            auto enterEvent = event;
            enterEvent.action = PointerAction::Enter;
            target->onPointerEvent(enterEvent);
        }

        hovered_ = target;
    }

    if (target == nullptr) {
        if (event.action == PointerAction::Down && focusManager_ != nullptr) {
            focusManager_->clear();
        }
        return false;
    }

    if (event.action == PointerAction::Down && focusManager_ != nullptr) {
        focusManager_->setFocused(target);
    }

    return target->onPointerEvent(event);
}

bool InputRouter::dispatchKey(const KeyEvent& event)
{
    if (focusManager_ == nullptr || focusManager_->focused() == nullptr) {
        return false;
    }
    return focusManager_->focused()->onKeyEvent(event);
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
