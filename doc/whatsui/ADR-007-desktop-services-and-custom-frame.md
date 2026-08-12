# ADR-007: Desktop Services and Custom Frame in the Platform Host

状态：Accepted Draft

## Context

ADR-003 定义了 WhatsUI 的最小 host 壳：`IPlatformApp / IPlatformWindow / IRenderSurface / ITextInputSession / IClipboard / ICursorService`。这层抽象足够运行一个只依赖窗口和渲染的桌面参考应用，例如早期的 Todo 和 Focus Tomato。

但真实产品级桌面 shell 需要一批 ADR-003 未覆盖的能力：

- 自绘标题栏和窗口最小化/最大化/关闭命令的可靠命中区域；
- 关闭意图与产品级决策（`Close/Hide/Cancel`）解耦，用来支持"关闭到托盘"和"活动会话确认退出"；
- 系统托盘的图标、菜单和事件；
- 系统通知和其激活载荷；
- 事件从原生线程安全投递回 UI 线程。

如果这些能力散落进每个应用，就会引入平台条件分支、直接依赖 `HWND / NSStatusItem / D-Bus StatusNotifierItem`，并绕过 `UiDispatcher`。这与 ADR-001 的"平台边界最小化"和 ADR-003 的"平台层只暴露必需的最小抽象"相互冲突。

## Decision

在 ADR-003 的 host 壳基础上追加两类抽象：

### 1. `PlatformWindow` 扩展

在 `PlatformWindow` 上加入 pre-1.0 preview 阶段的以下方法与关联类型：

- `hide / focus / minimize / maximize / restore / toggleMaximized`
- `state() -> WindowState`
- `capabilities() -> WindowCapabilities`
- `setAlwaysOnTop / setOpacity`
- `setFrameRegions(std::vector<WindowFrameRegion>)`
- `setCloseRequestHandler(WindowCloseRequestHandler)`
- 静态辅助 `hitTestWindowFrameRegions`
- 关联类型 `WindowFrameStyle / WindowBackdrop / WindowState / WindowFrameRegion / WindowCapabilities / WindowCloseDecision / WindowOptions`

新的 `PlatformHost::createWindow(const WindowOptions&)` 是首选构造入口。旧的 `createWindow(std::string, SizeF)` 保留但成为兼容降级路径。

### 2. `DesktopServices` 作为进程级 host 壳成员

`PlatformHost` 拥有 `DesktopServices& desktopServices()`，返回**每个 host 实例私有**的服务对象（默认为空实现）。`DesktopServices` 提供：

- `capabilities() -> DesktopCapabilities`
- `setTrayIcon / removeTrayIcon`
- `showNotification`
- `setEventHandler / setEventDispatcher`

事件通过 `publishEvent -> DesktopEventDispatcher -> DesktopEventHandler` 三段流水线传递：原生回调只调用 `publishEvent`，`UiApp` 在拿到 host 后把 dispatcher 绑到自己的 `UiDispatcher`，handler 因此永远在 UI 线程执行，不会与页面树竞态。

关联类型：`TrayIconOptions / TrayMenuItem / TrayMenuItemKind / DesktopIcon / DesktopNotification / NotificationUrgency / DesktopEvent / DesktopEventKind / DesktopCapabilities / DesktopOperationResult`。

### 3. 更新后的最小 host 壳清单

ADR-003 的最小 host 壳更新为：

- `IPlatformApp` (`PlatformHost`)
- `IPlatformWindow` (`PlatformWindow`)
- `IRenderSurface` (`RenderSurface`)
- `ITextInputSession` (`TextInputSession`)
- `IClipboard` (`Clipboard`)
- `ICursorService` (`CursorService`)
- **`IDesktopServices` (`DesktopServices`)** — 进程级，非窗口级

### 4. 降级策略

- 未实现 tray/通知的后端返回 `DesktopOperationResult::Unsupported`；
- 未实现自绘边框、透明 framebuffer、系统材质、置顶或不透明度的后端在 `WindowCapabilities` 中显式声明 `false`；
- 应用不允许在产品逻辑里凭空假设能力存在，必须先查 `capabilities()`；
- `createWindow(WindowOptions)` 的默认降级会在窗口创建后运行时应用 `alwaysOnTop / visibleOnCreate`，但会静默丢弃 `frameStyle / backdrop / transparentFramebuffer / resizable / min/max size`。后端要完整支持这些字段必须直接重写 `createWindow(const WindowOptions&)`。

## Consequences

- 满足 ADR-001 面向"桌面工具与嵌入式场景"的定位——tray、通知、自绘边框都是这类应用的常见需求，而不是"办公套件级"或"浏览器级"扩展。
- 保持 ADR-003 的"页面作者不直接感知平台窗口对象"的边界：应用永远不接触 `HWND / NSWindow / xdg_toplevel`；能力探测与降级都在公共类型上完成。
- `DesktopServices` 是**进程级**成员，不是 `PlatformWindow` 成员：托盘和通知代表用户面向进程的会话，不属于具体窗口的生命周期。
- 默认实现是每个 host 实例私有的，避免多 host 场景（多进程壳、测试并发 host）串扰事件。
- 与 ADR-005 / ADR-006 的声明式 Builder 契约不冲突：应用只通过 `UiApp::desktopServices()` 使用这些能力，不需要新的 declarative API。
- macOS Cocoa / Linux Wayland / Linux X11 的原生实现是后续 M6+ 的独立工作项，本 ADR 只固定契约并允许它们在实现前返回 `Unsupported`。

## Relation to other documents

- 本 ADR 扩展 ADR-003；ADR-003 的"最小 host 壳"清单以本 ADR 为准。
- 详细的能力设计、命中区域策略、跨平台后端矩阵、生命周期决策见 [`../DESKTOP_PLATFORM_CAPABILITIES.md`](../DESKTOP_PLATFORM_CAPABILITIES.md)。
- 公共 API 契约见 [`../../include/wui/platform.h`](../../include/wui/platform.h) 与 [`../../include/wui/app.h`](../../include/wui/app.h)。
- Focus Tomato 参考实现见 [`focus-tomato/WHATSUI_IMPLEMENTATION.md`](../focus-tomato/WHATSUI_IMPLEMENTATION.md) 的"桌面系统集成"章节。
