# Windows accessibility bridge

## Status

WhatsUI has a tested, platform-neutral semantic snapshot, but **does not yet
register a Windows UI Automation (UIA) provider**. Consequently, Narrator and
other UIA clients must not be described as supported in this release.

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

This gives a Windows adapter a coherent, frame-safe input. It does not expose
an OS accessibility tree by itself.

## Why there is no truthful one-line GLFW bridge

The current GLFW host has no `WM_GETOBJECT` handler and does not link
`uiautomationcore`. Its only native window subclass is owned by
`GlfwTextInputSession`, which processes IMM32 messages then delegates to the
previous GLFW window procedure. Returning a placeholder from `WM_GETOBJECT`,
or returning `UiaHostProviderFromHwnd`, would expose at most the native HWND;
it would not expose the custom-rendered WhatsUI controls. That would make
automation clients believe accessibility exists while hiding the Todo controls.

UIA needs a real provider object that implements at least:

1. `IRawElementProviderSimple` for root and elements: Name, HelpText,
   IsEnabled, ControlType, BoundingRectangle, HasKeyboardFocus, and
   ProviderOptions.
2. `IRawElementProviderFragment` for child/parent/sibling navigation,
   `GetRuntimeId`, focus, and point hit testing.
3. `IRawElementProviderFragmentRoot` for `ElementProviderFromPoint` and
   `GetFocus`.
4. `IInvokeProvider` for buttons, `IToggleProvider` for checkbox/radio/switch,
   and `IValueProvider` for editable text fields. A read-only value is not a
   substitute for text editing.

The GLFW host must answer `WM_GETOBJECT` for `OBJID_CLIENT` with
`UiaReturnRawElementProvider(hwnd, wParam, lParam, rootProvider)`. The IME and
UIA handlers must be multiplexed through one owner of the HWND subclass; two
independent `SetWindowLongPtrW(GWLP_WNDPROC, ...)` users are not safe.

## Implementation plan and acceptance gates

### A. Completed: semantic source of truth

- [x] Visual controls project runtime semantics and bounds.
- [x] Focus and modal isolation are reflected in window snapshots.
- [x] Headless regression tests cover role/name/enabled/checked/value/focus.

### B. Native read-only tree

- [ ] Add a Windows-only `Win32AccessibilityBridge` owned by
      `GlfwPlatformWindow`.
- [ ] Add a single HWND subclass dispatcher that sends IME and `WM_GETOBJECT`
      to registered handlers in a defined order.
- [ ] Keep an immutable, mutex-protected snapshot generated after layout; do
      not let UIA client threads read mutable Nodes.
- [ ] Implement UIA root/fragment navigation and map logical bounds to screen
      coordinates using the GLFW HWND and DPI scale.
- [ ] Link `uiautomationcore` only on Windows.

### C. Interactive controls and events

- [ ] Route Invoke/Toggle/Value actions to the UI thread through explicit
      `UiWindow` accessibility actions. Do not synthesize mouse coordinates.
- [ ] Raise focus/property/structure notifications after UI state changes.
- [ ] Add TextInput selection and document-range support before claiming full
      text editing support to Narrator.

### D. Windows-native verification

- [ ] Build a real GLFW Todo window and use `IUIAutomation::ElementFromHandle`.
- [ ] Assert root name and every common Todo control's Name, ControlType,
      IsEnabled, ToggleState/Value, and bounding rectangle at 100%, 150%, and
      200% DPI.
- [ ] Invoke a button and toggle a task through UIA, then assert the rendered
      state and UIA property event.
- [ ] Run a short Narrator smoke test manually only after the automated UIA
      checks pass.

Until B is complete, the release checklist should continue to call Windows
UIA/Narrator support a blocker rather than a delivered feature.
