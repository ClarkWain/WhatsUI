# Upgrade and contributing guide

Status: developer preview. Public source APIs are intentionally evolving and
there is no binary ABI guarantee before 1.0. Read
[Source stability and compatibility](STABILITY_AND_COMPATIBILITY.md) together
with this guide before updating a consumer or sending a change.

## Upgrade a source consumer

1. Pin a known WhatsUI release/tag and update the Git submodule revision with
   the project, rather than copying headers or static libraries between builds.
2. Read the release notes/changelog and public-header diffs for renamed,
   removed, or newly experimental APIs. Rebuild all application objects against
   the upgraded WhatsUI source; do not mix preview libraries from different
   revisions.
3. Start with a clean build directory and run the headless validation path:

   ```powershell
   cmake -S . -B build-upgrade
   cmake --build build-upgrade --config Debug
   ctest --test-dir build-upgrade -C Debug --output-on-failure
   ```

4. If the application uses rendering or a GLFW window, initialize all
   submodules and validate the same configuration used in production:

   ```powershell
   git submodule update --init --recursive
   cmake -S . -B build-upgrade-wsc -DWHATSUI_WITH_WHATSCANVAS=ON -DWHATSUI_BUILD_TESTS=ON -DWHATSUI_BUILD_EXAMPLES=ON
   cmake --build build-upgrade-wsc --config Debug
   ctest --test-dir build-upgrade-wsc -C Debug --output-on-failure
   ```

5. Re-run the application’s keyboard, DPI, and IME acceptance paths. For the
   GLFW reference host, use the 100%/150%/200% manual matrix in
   [Windows IMM32 IME](Windows-IMM32-IME.md). Review any changed Software
   capture rather than accepting a new image hash without viewing it.

6. Keep the application’s state/data migrations separate from a WhatsUI
   runtime upgrade. For example, Todo’s versioned local store is application
   data; it must retain its own migration and recovery policy.

### Package/export boundary

The Windows static developer-preview package can export the headless core and,
when configured with `WHATSUI_WITH_WHATSCANVAS=ON`, WhatsCanvas
Software/OpenGL, the selected bundled text dependencies, GLFW, and
`WhatsUI::Glfw`. A fresh Windows/MSVC external consumer is covered for both
the core and GLFW target.

This is not cross-toolchain or release-distribution certification. The package
smoke does not open a GLFW window, test a GPU driver, or satisfy the manual
IME/DPI, release-archive, signed-binary, or third-party legal/notice gates.
Keep a source checkout with initialized submodules available for development
and revalidate the package with the compiler/CRT and architecture you ship.

## Contributor setup

```powershell
git clone --recursive <repository-url>
Set-Location WhatsUI
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Use a separate directory for each materially different CMake option set (for
example `build`, `build-asan`, and `build-wsc`). Do not reuse a cached
headless build after enabling WhatsCanvas or sanitizers.

## Required verification by change type

| Change | Minimum evidence before review | Additional Windows evidence when applicable |
| --- | --- | --- |
| Runtime, node lifetime, state, input, focus, scrolling | Debug build, relevant focused tests, then full default CTest. | MSVC ASan for ownership/event/lifecycle work. |
| TextInput, text measurement, composition, clipboard | Focused text/window tests plus full default CTest. | GLFW IME check with a real installed IME; 100%/150%/200% caret/candidate placement when the host boundary changes. |
| Widget, layout, Fluent theme, Todo/Settings/Command Palette UI | Focused tests and full default CTest. | WhatsCanvas Software build; run visual hash and responsive-review tests. A reviewer must inspect the generated images. |
| WhatsCanvas/GLFW/backend wiring | Clean `WHATSUI_WITH_WHATSCANVAS=ON` configure/build. | Run the relevant interactive reference app, and state the GPU/driver/Windows version used for manual findings. |
| Public header/API | Consumer-facing compile coverage and documentation update. | State whether the change is additive, source-breaking, or experimental; no ABI promise may be implied. |
| Persistence/data format | Unit tests for round trip, malformed-input recovery, and version migration. | Verify the target LocalAppData or chosen injected path does not overwrite user data on failed saves. |

Run the complete commands in [Windows support matrix](WINDOWS_SUPPORT_MATRIX.md)
before marking a Windows milestone/release task complete.

## Review expectations

- Keep a change narrow: do not mix unrelated generated artifacts, local build
  directories, screenshot output, or submodule updates with a feature unless
  they are deliberately part of that feature.
- Add or update a focused regression test for each fixed bug. Tests must assert
  behavior, not merely that a process exits successfully.
- For a visual change, include the before/after scenario, regenerated hashes
  only after human image review, and the narrow/regular/wide captures when the
  Todo layout changes.
- Preserve ownership, attach/detach, deferred structural update, focus, and
  pointer-capture contracts. A UI sample that looks correct is not sufficient
  evidence when it changes these runtime boundaries.
- Document an intentional limitation at the public boundary. Do not advertise
  a Windows UI Automation bridge, TSF support, ABI stability, cross-platform
  parity, or release-grade GLFW behavior beyond the documented Windows/MSVC
  static package smoke.
- Use semantic Fluent tokens and documented component states rather than
  hard-coded per-screen rendering behavior when changing product UI.

## Pre-review command checklist

```powershell
# Required baseline.
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure

# Required for memory/lifetime-sensitive core changes.
cmake -S . -B build-asan -DWHATSUI_ENABLE_SANITIZERS=ON
cmake --build build-asan --config Debug
ctest --test-dir build-asan -C Debug --output-on-failure

# Required for renderer/example/visual changes.
cmake -S . -B build-wsc -DWHATSUI_WITH_WHATSCANVAS=ON -DWHATSUI_BUILD_TESTS=ON -DWHATSUI_BUILD_EXAMPLES=ON
cmake --build build-wsc --config Debug
ctest --test-dir build-wsc -C Debug --output-on-failure
ctest --test-dir build-wsc -C Debug --output-on-failure -R ^whatsui_todo_visual_review$
```

Sanitizers intentionally cover the headless runtime. Do not claim that a clean
sanitizer run validates WhatsCanvas; its third-party renderer path has a
separate validation contract.
