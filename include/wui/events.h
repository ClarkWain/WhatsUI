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

struct PointerEvent {
    WindowId windowId{0};
    PointerType pointerType{PointerType::Mouse};
    PointerAction action{PointerAction::Move};
    MouseButton button{MouseButton::None};
    PointF position{};
    KeyModifiers modifiers{0};
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
};

class FocusManager {
public:
    void setFocused(Node* node) noexcept;
    [[nodiscard]] Node* focused() const noexcept;
    void clear() noexcept;

private:
    Node* focused_{nullptr};
};

class InputRouter {
public:
    explicit InputRouter(FocusManager* focusManager = nullptr) noexcept;

    void setRoot(Node* root) noexcept;
    [[nodiscard]] Node* root() const noexcept;
    [[nodiscard]] Node* hovered() const noexcept;

    [[nodiscard]] Node* hitTest(PointF point) const;
    bool dispatchPointer(const PointerEvent& event);
    bool dispatchKey(const KeyEvent& event);
    bool dispatchTextInput(const TextInputEvent& event);
    bool dispatchComposition(const CompositionInputEvent& event);

private:
    Node* root_{nullptr};
    Node* hovered_{nullptr};
    FocusManager* focusManager_{nullptr};
};

} // namespace wui
