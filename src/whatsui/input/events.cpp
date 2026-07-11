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
    root_ = root;
    clearHover();
}

void InputRouter::clearHover() noexcept
{
    hovered_ = nullptr;
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
    return dispatchPointerTo(hitTest(event.position), event);
}

bool InputRouter::dispatchPointerTo(Node* target, const PointerEvent& event)
{

    // A disabled composite disables its entire subtree. Keep this guard at
    // routing level so every current and future ControlNode gets the same
    // pointer safety without requiring each widget to duplicate it.
    for (Node* current = target; current != nullptr; current = current->parent()) {
        if (auto* control = dynamic_cast<ControlNode*>(current); control != nullptr && !control->isEnabled()) {
            if (event.action == PointerAction::Down && focusManager_ != nullptr) {
                focusManager_->clear();
            }
            return false;
        }
    }

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

    // Route unhandled input through ancestors. This lets composite widgets
    // such as ScrollView own wheel gestures while their interactive content
    // remains the precise hit-test target.
    for (Node* current = target; current != nullptr; current = current->parent()) {
        if (current->onPointerEvent(event)) {
            return true;
        }
    }
    return false;
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
