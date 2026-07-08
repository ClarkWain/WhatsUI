# ADR-003: Navigation, Window, And Platform Host

状态：Accepted Draft

## Context

WhatsUI 需要支持多页面、多窗口和平台接入，但这些能力的边界如果不清晰，很容易相互污染：

- 页面切换可能和浮层混在一起。
- 多窗口可能退化成全局状态泥团。
- 平台层可能泄露到底层 UI API。

## Decision

概念分层固定如下：

- Page：同一窗口里的内容切换。
- Overlay：同一窗口里的临时浮层。
- Window：独立 root、surface、focus、IME 域。

窗口内使用 `Navigator` 管理页面栈，`OverlayHost` 管理 dialog、menu、toast、tooltip 等浮层。

应用级使用 `UiApp` 管理多个 `UiWindow`。每个 `UiWindow` 至少包含：

- `UiRoot`
- `Navigator`
- `OverlayHost`
- `FocusManager`
- `RenderSurface`

平台层只提供最小 host 壳：

- `IPlatformApp`
- `IPlatformWindow`
- `IRenderSurface`
- `ITextInputSession`
- `IClipboard`
- `ICursorService`

## Consequences

- 页面、浮层、多窗口职责清晰，不再混杂。
- 一个 `Node` 只能属于一个窗口，一个页面只能属于一个 `Navigator`。
- 跨窗口通信必须通过 app state 或 command bus，而不是直接共享节点实例。
- 平台层保持最小化，不把原生窗口和 GPU 对象直接暴露给页面作者。
