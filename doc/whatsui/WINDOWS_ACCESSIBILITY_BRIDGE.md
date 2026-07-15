# Windows accessibility bridge

## Status

WhatsUI now projects its platform-neutral semantic snapshot into a native,
Windows UI Automation (UIA) fragment tree for the GLFW host. Native
clients can discover named controls, roles, bounds, enabled/focused state,
values, and checked state. Buttons expose Invoke, checkboxes/switches expose
Toggle, editable text exposes Value, and focus requests are marshalled to the
UI thread. Name, enabled, toggle, value, focus, bounds, and structure changes
raise native UIA events. Rich text ranges, Selection patterns, and the Narrator
validation matrix are not complete, so full
screen-reader support must not yet be claimed.

The semantic projection is now a real runtime boundary rather than a
hand-authored test-only tree:

- `snapshotAccessibilityTree(const Node&, const Node*)` projects supported
  controls from the visual tree, with stable visual paths and logical bounds.
- `UiWindow::accessibilitySnapshot()` adds a stable `Application` root and
  projects only the active dialog while a modal is open.
- Button, Checkbox, Radio, Switch, Slider, ProgressBar, TextInput, Text,
  Dialog, Image, and Divider expose their current role/state. Controls carry
  current `enabled`; controls with selection carry current `checked`; the
  focused framework node carries `focused`.

`GlfwPlatformWindow` publishes this snapshot after every layout boundary. The
Windows bridge copies it behind a mutex and constructs providers from immutable
state, so UIA client threads never traverse mutable `Node` objects.

## Native bridge boundary

The existing GLFW Win32 subclass remains the single HWND message owner. It now
multiplexes IMM32 messages and `WM_GETOBJECT`; UIA requests for
`UiaRootObjectId` are routed to `UiaSnapshotBridge`, while unhandled messages
continue to the previous GLFW window procedure. The backend links
`uiautomationcore` only on Windows.

UIA needs a real provider object that implements at least:

1. `IRawElementProviderSimple` for root and elements: Name, HelpText,
   IsEnabled, ControlType, BoundingRectangle, HasKeyboardFocus, and
   ProviderOptions.
2. `IRawElementProviderFragment` for child/parent/sibling navigation,
   `GetRuntimeId`, focus, and point hit testing.
3. `IRawElementProviderFragmentRoot` for `ElementProviderFromPoint` and
   `GetFocus`.
4. `IInvokeProvider` for buttons, `IToggleProvider` for checkbox/switch/icon
   toggles, and `IValueProvider` for editable text fields. Radio selection is
   deliberately reserved for `SelectionItem`, rather than misrepresented as a
   toggle. A Value pattern is not a substitute for rich text ranges.

Fragment parents are calculated using the longest exposed visual-path prefix.
This is important because non-semantic visual containers are absent from the
snapshot. Bounds use the actual native-client/logical-size ratio independently
on each axis, then `ClientToScreen`, avoiding a second 150% DPI scale.

## Implementation plan and acceptance gates

### A. Completed: semantic source of truth

- [x] Visual controls project runtime semantics and bounds.
- [x] Focus and modal isolation are reflected in window snapshots.
- [x] Headless regression tests cover role/name/enabled/checked/value/focus.

### B. Completed: native read-only tree

- [x] Add a Windows-only `UiaSnapshotBridge` owned by
      `GlfwPlatformWindow`.
- [x] Use the single HWND subclass path to send IME and `WM_GETOBJECT`
      to registered handlers in a defined order.
- [x] Keep an immutable, mutex-protected snapshot generated after layout; do
      not let UIA client threads read mutable Nodes.
- [x] Implement UIA root/fragment navigation and map logical bounds to screen
      coordinates using the GLFW HWND and DPI scale.
- [x] Link `uiautomationcore` only on Windows.

### C. Interactive controls and events

- [x] Route Invoke/Toggle/Value/focus actions to the UI thread through explicit
      `UiWindow` accessibility actions. Do not synthesize mouse coordinates.
- [x] Keep retained provider identities stable across published snapshots,
      preferring explicit automation IDs for keyed/dynamic controls.
- [x] Raise focus, Name, IsEnabled, ToggleState, Value, BoundingRectangle, and
      structure notifications after UI state changes. Event delivery is
      posted back to the UI thread so an action COM call cannot be re-entered.
- [ ] Add TextInput selection and document-range support before claiming full
      text editing support to Narrator.

### D. Windows-native verification

- [x] Build a real GLFW window and use `IUIAutomation::ElementFromHandle`; the
      automated smoke requires a named/focused WhatsUI Button, framework id,
      control type, and client-contained screen bounds.
- [ ] Assert root name and every common Todo control's Name, ControlType,
      IsEnabled, ToggleState/Value, and bounding rectangle at 100%, 150%, and
      200% DPI.
- [x] Invoke a button, toggle a checkbox, edit text, and move focus through UIA;
      assert callbacks run on the GLFW UI thread and retained providers observe
      the newly published state.
- [x] Subscribe from an MTA UIA client and assert property, focus, bounds, and
      structure events, stable retained RuntimeIds, and zero duplicate events
      for identical snapshot republishes.
- [ ] Run a short Narrator smoke test manually only after the automated UIA
      checks pass.

Until C and the remaining D matrix are complete, the release checklist should
continue to call Windows Narrator/screen-reader support a blocker rather than
a delivered feature.
