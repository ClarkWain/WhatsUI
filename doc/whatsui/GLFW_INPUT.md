# GLFW input contract

`WhatsUI::Glfw` translates GLFW callbacks into the platform-neutral events in
`wui/events.h`. Pointer positions are GLFW logical-window coordinates, matching
`PlatformWindow::metrics().logicalSize`; scroll steps are normalized to 40
logical pixels per GLFW unit.

All GLFW modifier bits are explicitly translated to `KeyModifierShift`,
`KeyModifierControl`, `KeyModifierAlt`, and `KeyModifierSuper`. Pointer move,
enter/leave, and wheel events query the currently held modifier keys; button
events use GLFW's callback mask. This avoids relying on coincidental native bit
layouts and makes chords available to pointer-driven controls.

The backend emits pointer enter/leave callbacks and requests a redraw for every
delivered native input event. It also reports native window focus changes to
`UiWindow`: focus loss deactivates an active text-input session and clears
transient hover routing, while retaining logical focus; focus gain restores the
session and synchronizes its caret and surrounding text.

GLFW only standardizes arrow, I-beam, hand, horizontal-resize, and
vertical-resize cursors. The two diagonal WhatsUI cursor icons therefore use
the portable arrow fallback. Cursor objects are reused when the requested
standard shape is already active.
