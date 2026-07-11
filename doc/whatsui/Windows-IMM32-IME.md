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

1. Focus the task composer and switch the Windows input method to Chinese.
2. Type a multi-character pre-edit phrase: the pre-edit text must update in
   the field exactly once, and the candidate window must appear at the caret.
3. Commit a candidate: committed text must appear once, with no stale
   composition span remaining.
4. Move the window between 100% and 150%/200% scale monitors (or change
   display scale), refocus the field, and confirm that the candidate window
   remains adjacent to the visible caret.
5. Start a composition, open a confirmation dialog or switch app focus, and
   return: the pre-edit text must be cancelled rather than accidentally
   committed.

## Current boundary

IMM32 provides composition, result strings, and candidate placement for the
Windows reference host. It does not expose TSF surrounding-text services or
candidate UI styling. The planned editing-controller and shaping work remains
responsible for grapheme-aware selection, CJK/emoji fallback, composition
underlines, clipboard commands, undo/redo, and a future TSF adapter.
