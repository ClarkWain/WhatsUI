# Windows GLFW IMM32 IME bridge

The GLFW reference host uses an IMM32 adapter on Windows for `TextInput`.
The adapter is deliberately contained in `WhatsUIGlfw`; `TextInput`, the
editing model, and headless builds do not depend on Win32 headers or libraries.

## Contract

- Focusing a `TextInput` activates the native session. `UiWindow` synchronizes
  its logical caret rectangle and surrounding text after every input event.
- The adapter subclasses the GLFW `HWND` without touching GLFW's
  `GWLP_USERDATA`. It handles `WM_IME_STARTCOMPOSITION`,
  `WM_IME_COMPOSITION`, and `WM_IME_ENDCOMPOSITION`.
- `GCS_COMPSTR` is converted from UTF-16 to UTF-8 and routed as a
  `CompositionInputEvent::Update`; `GCS_RESULTSTR` is routed as a committed
  `TextInputEvent`, followed by an explicit composition end. This prevents
  GLFW's committed-character callback from duplicating an IME commit.
- The current logical caret is converted to the Win32 client-pixel coordinate
  space using `GetClientRect / glfwGetWindowSize`. `ImmSetCompositionWindow`
  and `ImmSetCandidateWindow` then keep pre-edit and candidate UI adjacent to
  the rendered caret on high-DPI displays.
- Deactivation cancels an in-progress composition instead of committing it.
  Native window procedure subclassing is removed before GLFW destroys the
  `HWND`.

## Manual Windows verification

Build the interactive Todo or Settings sample with WhatsCanvas enabled, then
verify the following with Microsoft Pinyin or another installed IMM32-backed
IME:

```powershell
cmake -S . -B build-wsc -DWHATSUI_WITH_WHATSCANVAS=ON -DWHATSUI_BUILD_EXAMPLES=ON
cmake --build build-wsc --config Debug --target WhatsUITodoGlfw
.\build-wsc\examples\Debug\WhatsUITodoGlfw.exe
```

Use this matrix for a Windows sign-off. Repeat it for Todo's composer and the
Settings reference input; a pass in one sample is not evidence that the
window/session boundary works in the other.

| Check | Steps | Required result |
| --- | --- | --- |
| Pre-edit and underline | Focus the field, switch to Microsoft Pinyin, and type a multi-character phrase without selecting a candidate. | The pre-edit string appears exactly once and has the composition visual treatment (the M2 rendering target is an underline, not an indistinguishable committed string). The candidate UI is adjacent to its caret. |
| Commit / cancel | Select a candidate, then repeat and press Escape. | A commit appears exactly once and clears the composition treatment. Escape or loss of focus leaves no pre-edit text behind and does not commit it. |
| Fractional DPI | At **100%, 150%, and 200%** Windows display scale, place the caret at the start, middle, and end of a longer line. Move the window to each monitor and refocus before typing. | Candidate and composition windows track the visible caret; they are not offset by the desktop position, by a factor of 1.5/2, or by one line height. |
| Selection / clipboard | Select text with Shift+Arrow and mouse drag; run Ctrl+C, Ctrl+X, Ctrl+V, Ctrl+Z, Ctrl+Y. Repeat immediately after committing a candidate. | Selection stays in logical text order. Clipboard commands operate on the selected committed range, never on a cancelled pre-edit range; undo/redo does not resurrect a stale composition decoration. |
| Modal / activation | Start a pre-edit phrase, open Todo's confirmation dialog, then close it with Escape. Repeat by Alt+Tabbing away and back. | The native session deactivates/cancels composition, the dialog receives keyboard input, and restoring focus repositions candidate UI at the current caret. |

Capture one screenshot or short recording per DPI value while the candidate UI
is visible. Native IME chrome is owned by Windows and is therefore not part of
the deterministic Software pixel hash; these artifacts are the required human
evidence for the HWND/IMM32 boundary.

## Automated coverage

`WhatsUIWindowTests` deliberately exercises the portable half of this contract
without creating an HWND:

- `testHighDpiImeCaretCompositionAndClipboardContract` uses a 400×260 logical
  window with a 600×390 client/framebuffer projection (150%). It proves that
  `UiWindow` passes an **unscaled logical** caret rectangle to the platform
  session, and checks the documented rounding projection used by the IMM32
  candidate/composition placement boundary.
- The same test drives Start/End composition, verifies the retained
  composition range and synchronized surrounding selection, then checks
  Ctrl+C/Ctrl+X/Ctrl+V after the lifecycle.

The native `GetClientRect` and `ImmSet*Window` calls remain private to the GLFW
backend, so they are not faked as Win32 behavior in a headless unit test. The
fractional-DPI rows above are mandatory Windows manual verification, not a
claim that a Software-renderer test can validate native candidate placement.

## Current boundary

IMM32 provides composition, result strings, and candidate placement for the
Windows reference host. It does not expose TSF surrounding-text services or
candidate UI styling. Grapheme-aware selection, CJK/emoji fallback, the
composition-underline renderer, clipboard commands, undo/redo, and a future
TSF adapter remain separately versioned contracts; do not represent the native
candidate placement check as proof of those higher-level text features.
