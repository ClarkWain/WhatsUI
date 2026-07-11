#pragma once

#include <cstdint>
#include <string>

#include "wui/types.h"

namespace wui {

class Node;

enum class PointerType {
    Mouse,
    Touch,
    Pen,
};

enum class PointerAction {
    Move,
    Down,
    Up,
    Enter,
    Leave,
    // Mouse-wheel / trackpad scroll. The delta is expressed in logical pixels;
    // positive Y moves content toward its start, negative Y toward its end.
    Scroll,
};

enum class MouseButton {
    None,
    Left,
    Right,
    Middle,
};

enum class KeyAction {
    Down,
    Up,
};

using KeyModifiers = std::uint32_t;

// Modifier bits are deliberately platform-neutral. Platform backends should
// translate their native masks before constructing a KeyEvent.
constexpr KeyModifiers KeyModifierShift = 1u << 0;
constexpr KeyModifiers KeyModifierControl = 1u << 1;
constexpr KeyModifiers KeyModifierAlt = 1u << 2;
constexpr KeyModifiers KeyModifierSuper = 1u << 3;

struct PointerEvent {
    WindowId windowId{0};
    PointerType pointerType{PointerType::Mouse};
    PointerAction action{PointerAction::Move};
    MouseButton button{MouseButton::None};
    PointF position{};
    KeyModifiers modifiers{0};
    PointF scrollDelta{};
};

struct KeyEvent {
    WindowId windowId{0};
    KeyAction action{KeyAction::Down};
    int keyCode{0};
    KeyModifiers modifiers{0};
    bool isRepeat{false};
};

struct TextInputEvent {
    WindowId windowId{0};
    std::string text;
};

struct CompositionInputEvent {
    WindowId windowId{0};
    std::string text;
    // Empty updates are not overloaded as completion: platforms must state the
    // composition transition explicitly so an empty pre-edit string is valid.
    enum class Phase {
        Start,
        Update,
        End,
    } phase{Phase::Update};
};

class FocusManager {
public:
    void setFocused(Node* node) noexcept;
    [[nodiscard]] Node* focused() const noexcept;
    void clear() noexcept;

    // Advances through enabled ControlNode instances in tree (pre-order)
    // order. When focus is outside the supplied root, traversal starts at the
    // first/last focusable node. Returns false when the tree has no focusable
    // controls.
    bool focusNext(Node* root, bool reverse = false) noexcept;

private:
    Node* focused_{nullptr};
};

class InputRouter {
public:
    explicit InputRouter(FocusManager* focusManager = nullptr) noexcept;

    void setRoot(Node* root) noexcept;
    // Clears the non-owning hover pointer after a subtree has been removed.
    // No Leave event is dispatched because the target may already be destroyed.
    void clearHover() noexcept;
    [[nodiscard]] Node* root() const noexcept;
    [[nodiscard]] Node* hovered() const noexcept;

    [[nodiscard]] Node* hitTest(PointF point) const;
    bool dispatchPointer(const PointerEvent& event);
    bool dispatchPointerTo(Node* target, const PointerEvent& event);
    bool dispatchKey(const KeyEvent& event);
    bool dispatchTextInput(const TextInputEvent& event);
    bool dispatchComposition(const CompositionInputEvent& event);

private:
    Node* root_{nullptr};
    Node* hovered_{nullptr};
    FocusManager* focusManager_{nullptr};
};

} // namespace wui
