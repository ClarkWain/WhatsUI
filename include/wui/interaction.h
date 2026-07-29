#pragma once

// InteractionArea — an attached component that turns any layout node into a
// clickable, hoverable, focusable, keyboard-invocable surface without forcing
// it to inherit from ControlNode. Owned lazily by the host node (see
// Container::ensureInteraction()); when absent the host stays a pure layout
// container with the exact behavior it had before this file existed.
//
// The struct is intentionally passive: it stores callbacks, optional visual
// tokens, accessibility metadata, and the current ControlVisualStates bitmask.
// The host node is responsible for wiring pointer/keyboard/a11y events into
// this data (Container does so today; Row/Column/Image can adopt the same
// pattern later without changing this contract).

#include <functional>
#include <optional>
#include <string>

#include "wui/accessibility.h"
#include "wui/events.h"
#include "wui/node.h"
#include "wui/types.h"

namespace wui {

struct InteractionArea {
    // Command semantics — fired on pointer up inside bounds after a matching
    // pointer down, or on keyboard Enter/Space (see host implementation).
    std::function<void()> onClick;

    // Raw pointer taps. Returning true consumes the event (stops propagation);
    // returning false lets the host continue its default routing.
    std::function<bool(const PointerEvent&)> onPointerDown;
    std::function<bool(const PointerEvent&)> onPointerMove;
    std::function<bool(const PointerEvent&)> onPointerUp;

    // Enter/leave transitions. Fires only on genuine state flips.
    std::function<void(bool hovered)> onHoverChange;

    // Logical focus transitions. Hosts that do not implement a focus policy
    // may leave this quiet; the callback is safe to attach regardless.
    std::function<void(bool focused)> onFocusChange;

    // Raw keyboard hook. Returning true consumes the event before the host's
    // default Enter/Space handling fires onClick.
    std::function<bool(const KeyEvent&)> onKey;

    // Optional visual tokens used by the host when painting its background.
    // Absent -> the host paints its normal background.
    std::optional<Color> hoverBackground;
    std::optional<Color> pressedBackground;

    // Accessibility. Roles other than Group let the platform bridge expose the
    // node as a Button / ListItem / MenuItem to UIA. accessibleLabel provides
    // the accessible name for narration when the host has no visible text.
    AccessibilityRole accessibleRole{AccessibilityRole::Group};
    std::string accessibleLabel;

    // Reactive state that drives the visual tokens above. Bits reuse
    // ControlVisualState so the same paint helpers work for both attached
    // interactions and native Fluent controls.
    ControlVisualStates states{0};
};

} // namespace wui
