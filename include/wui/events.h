#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "wui/types.h"

namespace wui {

class Node;

// Pointer events make one deterministic trip through the hit path. Capture
// observes root-to-leaf, Target invokes the hit node once, and Bubble returns
// leaf-to-root. This is intentionally independent of pointer capture storage.
enum class EventPhase {
    Capture,
    Target,
    Bubble,
};

// New handlers use this instead of an unqualified bool. More than one action
// may be requested through EventContext; the result describes the handler's
// primary disposition.
enum class EventResult {
    Ignored,
    Handled,
    StopPropagation,
    RequestFocus,
    CapturePointer,
    ReleasePointer,
};

enum class PointerCaptureRequest {
    None,
    Capture,
    Release,
};

class EventContext {
public:
    [[nodiscard]] EventPhase phase() const noexcept { return phase_; }
    [[nodiscard]] Node* target() const noexcept { return target_; }
    [[nodiscard]] Node* currentTarget() const noexcept { return currentTarget_; }

    void stopPropagation() noexcept { propagationStopped_ = true; }
    [[nodiscard]] bool isPropagationStopped() const noexcept { return propagationStopped_; }

    // No argument means the node currently handling this event.
    void requestFocus(Node* node = nullptr) noexcept
    {
        focusRequested_ = true;
        focusTarget_ = node != nullptr ? node : currentTarget_;
    }
    [[nodiscard]] bool isFocusRequested() const noexcept { return focusRequested_; }
    [[nodiscard]] Node* requestedFocus() const noexcept { return focusTarget_; }

    void capturePointer() noexcept { pointerCaptureRequest_ = PointerCaptureRequest::Capture; }
    void releasePointer() noexcept { pointerCaptureRequest_ = PointerCaptureRequest::Release; }
    [[nodiscard]] PointerCaptureRequest pointerCaptureRequest() const noexcept { return pointerCaptureRequest_; }

private:
    friend class InputRouter;
    EventContext(EventPhase phase, Node* target, Node* currentTarget) noexcept
        : phase_(phase), target_(target), currentTarget_(currentTarget) {}

    EventPhase phase_;
    Node* target_{nullptr};
    Node* currentTarget_{nullptr};
    Node* focusTarget_{nullptr};
    PointerCaptureRequest pointerCaptureRequest_{PointerCaptureRequest::None};
    bool propagationStopped_{false};
    bool focusRequested_{false};
};

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
    // Terminates an active pointer gesture without activating it. Hosts send
    // this when native input is interrupted; the router also synthesizes it
    // when capture loses its node, window activation, or input layer.
    Cancel,
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
    // Captures subsequent pointer events for `target`, independent of hit
    // testing.  A second capture first cancels the old gesture.  The router
    // automatically captures a handled left-button Down and releases it on
    // the matching Up, which gives ordinary controls correct drag-outside
    // behavior; custom controls may use this API for other gestures.
    [[nodiscard]] bool capturePointer(Node* target) noexcept;
    void releasePointer(Node* target = nullptr) noexcept;
    // Sends exactly one PointerAction::Cancel to the current target and then
    // clears capture. Safe if the target mutates the tree during cancellation.
    void cancelPointerCapture() noexcept;
    [[nodiscard]] Node* root() const noexcept;
    [[nodiscard]] Node* hovered() const noexcept;
    [[nodiscard]] Node* capturedPointer() const noexcept;

    [[nodiscard]] Node* hitTest(PointF point) const;
    bool dispatchPointer(const PointerEvent& event);
    bool dispatchPointerTo(Node* target, const PointerEvent& event);
    bool dispatchKey(const KeyEvent& event);
    bool dispatchTextInput(const TextInputEvent& event);
    bool dispatchComposition(const CompositionInputEvent& event);

private:
    struct CaptureState {
        Node* target{nullptr};
    };

    static void cancelCaptureState(const std::shared_ptr<CaptureState>& state) noexcept;

    Node* root_{nullptr};
    Node* hovered_{nullptr};
    FocusManager* focusManager_{nullptr};
    std::shared_ptr<CaptureState> captureState_{std::make_shared<CaptureState>()};
};

} // namespace wui
