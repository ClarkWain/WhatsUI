# Command Palette reference

状态：Retired（2026-08-02）

独立 Command Palette App、CMake target 和捕获入口已删除，不再构建、发布或维护。
下文仅保留为历史设计记录；当前命令面板控件场景由 Component Gallery 覆盖。

`WhatsUICommandPaletteApp` and `WhatsUICommandPaletteGlfw` are one M2 source
tree with two hosts: deterministic WhatsCanvas Software captures and the
interactive Windows GLFW window.

The reference proves the intended composition contract:

- `Dialog` owns the modal scrim, focus isolation, Escape dismissal, and focus
  restoration.
- `TextInput::onChange` updates the result `State` immediately; `ForEach`
  rebuilds the filtered list at the structural frame boundary.
- `TextInput::onSubmit` runs the first filtered command on Enter. Its
  `onCancel` is useful when embedded outside a dialog; an active `Dialog`
  deliberately consumes Escape first so it can restore the prior focus.
- Each command entry exposes an equivalent pointer action. Tab and
  Shift+Tab traverse the input and result buttons within the active dialog.

Build the capture target with `WHATSUI_WITH_WHATSCANVAS=ON` and
`WHATSUI_BUILD_EXAMPLES=ON`, then run:

```powershell
./WhatsUICommandPaletteApp <output-directory>
```

It writes `command_palette_default.ppm` and
`command_palette_filtered.ppm`. The GLFW target opens the landing page; use
**Open command palette**, type a query, press **Enter** to run the first
result, or **Escape** to dismiss it.

The palette is intentionally a reference rather than a global shortcut
manager. A production host may bind Ctrl+K (or an app-specific accelerator)
to the same `showDialog` action without coupling shortcut policy to the core
widget runtime.
