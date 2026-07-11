# Settings Panel reference

The M1 Settings reference is a Windows-first Fluent light application used to
exercise the input, focus, scroll, dialog, and overlay contracts together.
The application has exactly one declarative tree shared by both targets:

- `WhatsUISettingsApp`: deterministic WhatsCanvas Software capture.
- `WhatsUISettingsGlfw`: interactive GLFW/OpenGL window.

## Controls and interactions

The General page deliberately includes Button, Checkbox, ScrollView, keyboard
Tab / Shift+Tab traversal, a modal reset confirmation, and a non-modal overlay
popup menu. The overflow button opens the popup; its actions dismiss the popup
before changing state or opening the reset dialog. Escape cancels the dialog
and restores its invoking control through `UiWindow::showDialog`.

M1 does not yet expose a pointer-drag Slider. **Text scale** is therefore a
usable discrete stepper: `-` and `+` move between 80% and 130% in 10% steps,
are reachable by Tab, and activate with Space/Enter. It is intentionally
labelled as a temporary substitute; M3 must replace it with the Slider control
listed in [ROADMAP.md](../../ROADMAP.md).

## Build and capture

```powershell
cmake -S . -B build-wsc -DWHATSUI_WITH_WHATSCANVAS=ON -DWHATSUI_BUILD_EXAMPLES=ON
cmake --build build-wsc --config Debug --target WhatsUISettingsApp WhatsUISettingsGlfw
.\build-wsc\examples\Debug\WhatsUISettingsApp.exe build-wsc\settings-visual --size 900x680
.\build-wsc\examples\Debug\WhatsUISettingsGlfw.exe
```

The first command writes `settings_general.ppm` at 2x device-pixel ratio.
Review it alongside the interactive window whenever layout/theme work changes:
the navigation rail must retain its width, the content cards must keep a
consistent vertical rhythm, and the scroll viewport must clip content rather
than paint beyond the application surface.
