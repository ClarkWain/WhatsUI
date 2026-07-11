# Pointer event routing

`InputRouter` dispatches a pointer event through the stable path from the
routed root to the delivery target. The path is captured before handlers run;
structural changes requested by a handler remain deferred until the frame
boundary.

The order is:

1. `EventPhase::Capture` — root through the target's parent.
2. `EventPhase::Target` — the target exactly once.
3. `EventPhase::Bubble` — the target's parent back through the root.

`Enter` and `Leave` are synthesized hover-state notifications rather than
physical host events. They are delivered directly to the affected hover target
with `EventPhase::Target`; they do not form a second capture/bubble traversal
around the physical `Move` event that caused the hover change.

New controls override:

```cpp
EventResult onPointerEvent(const PointerEvent&, EventContext&) override;
```

`EventContext` exposes the current phase, target and current target, plus
`stopPropagation()`, `requestFocus()`, `capturePointer()` and
`releasePointer()`. `EventResult` provides corresponding single-result forms
for simple handlers. A propagation stop ends the current dispatch immediately.

The older `bool onPointerEvent(const PointerEvent&)` remains source-compatible.
It runs only at Target for the delivery node and at Bubble for ancestors, which
preserves the former target-then-parent behavior and prevents legacy widgets
from processing an event twice during the new Capture phase.

Pointer capture ownership and cancellation are provided by `InputRouter`'s
capture API. Context capture/release requests delegate to that API; the event
context does not own or retain capture state.
