# Windows support matrix

Status: developer preview. Windows is the primary validation target, not a
claim of universal desktop or accessibility parity. This document records what
is verified in the source tree and what still requires a release gate.

## Supported build configurations

| Area | Supported preview configuration | Evidence / verification | Boundary |
| --- | --- | --- | --- |
| Language and build system | C++17 with CMake 3.20 or newer. The CI Windows job uses the Visual Studio/MSVC generator supplied by `windows-latest`. | Configure, Debug build, and CTest in CI. | Exact MSVC toolset/CRT compatibility is not yet a 1.0 promise. Rebuild consumers with the same WhatsUI source/release. The proposed post-1.0 boundary is in [Compatibility policy draft](COMPATIBILITY_POLICY_1_0_DRAFT.md) and remains unapproved. |
| Core runtime | Headless `WhatsUI::WhatsUI`, with `WHATSUI_WITH_WHATSCANVAS=OFF`. | Unit, layout, state, window, lifecycle, component, virtual-list, inspector, benchmark, and storage tests. | This path has no platform window backend or renderer. It is a runtime/developer-preview package, not an end-user GUI by itself. |
| Deterministic rendering | WhatsCanvas Software backend with `WHATSUI_WITH_WHATSCANVAS=ON`. | Text, composition, Todo, and Settings Software captures; Todo visual hash and responsive-review tests. | A matching visual result is not proof of native window, GPU, screen-reader, or IME candidate-window behavior. |
| Interactive desktop host | WhatsUI GLFW host + WhatsCanvas OpenGL. | Todo, Settings, Command Palette, and Hello Window reference targets; an external package consumer links the installed `WhatsUI::Glfw` target on Windows/MSVC. | The package smoke is link/run coverage without opening a native window. Direct Win32, WinUI, Qt, and other host integrations are not supplied. |
| Sanitizers | MSVC AddressSanitizer for the headless runtime. | `WHATSUI_ENABLE_SANITIZERS=ON` CI job. | MSVC does not supply UBSan; third-party WhatsCanvas integration is outside this sanitizer gate. |

The Linux/Clang ASan+UBSan job is a portability/memory-safety check for the
headless runtime. It does **not** upgrade Linux or macOS to an interactive
platform-support claim.

## Display, DPI, and text input

| Capability | Current Windows status | Required sign-off / limitation |
| --- | --- | --- |
| Logical layout and device scale | WhatsUI lays out in logical coordinates; the Software capture path applies the configured device scale consistently to paint/readback. | Visual review covers 2× captures, but native multi-monitor DPI behavior still needs manual Windows verification. |
| GLFW high-DPI window | The GLFW host projects the logical caret into Win32 client pixels for the IMM32 bridge. | Validate at 100%, 150%, and 200% Windows scale and after moving between monitors. See [Windows IMM32 IME](Windows-IMM32-IME.md). |
| Text shaping/fallback | The WhatsCanvas bridge requests FreeType/HarfBuzz by default when the optional renderer is enabled; the documented fallback chain includes Windows fonts. | Font availability and fallback differ by Windows installation. CJK, emoji, bidi, and the cache have automated coverage, but application-specific typography still needs visual review. |
| Editing and clipboard | `TextInput` provides selection, Home/End, word deletion, clipboard integration at the window boundary, undo/redo, and composition rendering. | Positions are currently UTF-8 byte offsets. This is not a grapheme-aware editor or rich-text engine. |
| IME | The Windows GLFW reference host has an IMM32 adapter for pre-edit/result strings and candidate/composition placement. | It is not a TSF adapter, does not style native candidate UI, and must be checked manually with an installed IME. Native candidate chrome is not in Software pixel hashes. |

## Components and interaction

The preview includes Fluent light/dark theme tokens; controls including Button,
Checkbox, TextInput/TextField, Radio, Switch, Slider, ProgressBar, Divider,
ListView, Dialog, Popup/Menu, Tooltip, IconButton, SearchField, scrolling,
and virtual lists. Keyboard routing, Tab traversal, pointer capture, and modal
focus restoration are tested runtime contracts.

This is not a declaration that every control is equivalent to its WinUI or
native Windows counterpart. In particular, full automation peers, native
screen-reader interoperability, high-contrast policy, motion preferences,
and every localized input scenario remain release validation work.

### Accessibility limitation — no Windows bridge yet

`wui/accessibility.h` provides a platform-neutral semantic tree and stable
snapshot API for tests/diagnostics. It does **not** register a Windows UI
Automation provider, expose an OS accessibility tree, synthesize input, or
promise Narrator/screen-reader support. Applications must not describe the
current preview as screen-reader accessible until a Windows accessibility
bridge and its manual validation matrix are delivered.

## Package/export status and limitation

The Windows static export path supports the core, WhatsCanvas Software/OpenGL,
the bundled advanced-text dependencies used by the selected build, GLFW, and
`WhatsUI::Glfw`. A fresh Windows/MSVC external consumer has been verified with
`find_package(WhatsUI)` and the installed GLFW target.

This remains preview engineering evidence, not a redistributable release
claim. The current package verification does not launch a native window, test
GPU drivers, sign binaries, create a release archive, or complete third-party
attribution/legal review. The proposed 1.0 static package profiles and their
MSVC/CRT boundary remain unapproved; see
[Compatibility policy draft](COMPATIBILITY_POLICY_1_0_DRAFT.md). Consumers
using another compiler, CRT, architecture, package manager, or build profile
must validate their own configuration.

## Windows commands

Run these from the repository root in a Visual Studio Developer PowerShell.

```powershell
git submodule update --init --recursive

# Headless runtime and all default tests.
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure

# Windows/MSVC AddressSanitizer runtime gate.
cmake -S . -B build-asan -DWHATSUI_ENABLE_SANITIZERS=ON
cmake --build build-asan --config Debug
ctest --test-dir build-asan -C Debug --output-on-failure

# WhatsCanvas Software captures and interactive GLFW reference apps.
cmake -S . -B build-wsc -DWHATSUI_WITH_WHATSCANVAS=ON -DWHATSUI_BUILD_TESTS=ON -DWHATSUI_BUILD_EXAMPLES=ON
cmake --build build-wsc --config Debug
ctest --test-dir build-wsc -C Debug --output-on-failure

# Review Todo composition at narrow, regular, and wide window sizes.
ctest --test-dir build-wsc -C Debug --output-on-failure -R ^whatsui_todo_visual_review$

# Manual interactive IME/DPI check.
.\build-wsc\examples\Debug\WhatsUITodoGlfw.exe
```

For a change restricted to the Todo pixels, run both
`whatsui_todo_visual_regression` and `whatsui_todo_visual_review`; inspect the
review images as described in [Todo demo delivery](TODO_DEMO_DELIVERY.md).
