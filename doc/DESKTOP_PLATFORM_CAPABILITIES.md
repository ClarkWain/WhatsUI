# WhatsUI 桌面平台能力设计

## 目标

WhatsUI 的桌面能力分为三条互不混淆的边界：

```text
声明式 UI / UiApp
├─ PlatformWindow：窗口外观、窗口状态、系统拖拽与缩放
├─ DesktopServices：托盘、系统通知和桌面能力探测
└─ UiDispatcher：把原生事件安全投递回 UI 线程
        ↓
GLFW + Win32 / Cocoa / X11 / Wayland / D-Bus
        ↓
WhatsCanvas OpenGL / Software
```

GLFW 是当前参考窗口与 OpenGL 上下文后端，不是 WhatsUI 的公共桌面抽象。公共 API 只表达应用意图，不暴露 `HWND`、`NSWindow`、X11 Window、Wayland 对象或 D-Bus 类型。

## 窗口契约

`WindowOptions` 在创建前固定下列能力：

- 系统边框、自绘边框或完全无边框；
- 初始、最小和最大逻辑尺寸；
- 是否可缩放、置顶和创建时可见；
- 是否请求逐像素透明 framebuffer；
- 背景策略。

`PlatformWindow` 在运行时提供：

- 显示、隐藏、聚焦和关闭；
- 最小化、最大化、恢复和最大化切换；
- 整窗不透明度和置顶状态；
- 自绘标题栏命中区域；
- 关闭请求决策；
- 窗口状态与能力查询。

逐像素透明、整窗不透明度、系统背景材质和鼠标穿透是四种不同能力，不使用单个 `transparent` 布尔值代替。

典型窗口创建方式：

```cpp
wui::WindowOptions options;
options.title = "FocusTomato";
options.initialSize = {520.0f, 720.0f};
options.minimumSize = options.initialSize;
options.maximumSize = options.initialSize;
options.frameStyle = wui::WindowFrameStyle::Custom;
options.resizable = false;
options.visibleOnCreate = false;

auto& window = app.openWindow(options);
```

`WindowBackdrop::Transparent` 会隐含申请透明 framebuffer；显式的
`transparentFramebuffer` 保留给需要自行定义背景策略的窗口。应用必须查询
`PlatformWindow::capabilities()`，不能仅凭创建参数推断系统已经提供合成能力。

## 自绘标题栏

自绘标题栏必须把布局后的逻辑坐标区域提交给平台后端：

- `Caption`：由系统开始移动；
- `MinimizeButton`、`MaximizeButton`、`CloseButton`：系统标题按钮；
- `Client`：显式排除的交互内容。

Windows 使用 `WM_NCHITTEST` 返回 `HTCAPTION`、八方向缩放区域及标题按钮命中值，不使用逐帧 `SetWindowPos` 模拟拖拽。最大化时使用工作区，不覆盖任务栏；所有边缘宽度按当前窗口 DPI 计算。

macOS 优先使用 `fullSizeContentView` 与透明标题栏保留系统交通灯；真正的 `borderless` 只用于浮层或特殊工具窗。Linux X11 使用窗口管理器协议发起移动/缩放，Wayland 使用带输入 serial 的 `xdg_toplevel.move/resize`。后端无法保证系统移动时必须通过 capability 明确降级。

## 透明窗口

透明 framebuffer 是创建期能力。渲染后端必须：

- 创建含 Alpha 通道的 surface；
- 每帧以透明颜色清屏；
- 统一预乘 Alpha 语义；
- 保证 OpenGL 与 Software 合成结果一致；
- 独立控制鼠标穿透，不能以像素透明度推断输入行为。

不支持系统模糊或背景材质时，应用降级为普通透明或不透明背景。

## 托盘

托盘属于进程级 `DesktopServices`，不属于某一个窗口。公共模型包含 RGBA 图标、稳定 action ID、标签、启用、选中、分隔符及默认操作。原生菜单事件转换为 `DesktopEvent` 后再投递到 UI 线程。FocusTomato 复用 18 × 18 品牌图，不依赖可执行文件的默认图标。

- Windows：`Shell_NotifyIcon`，使用稳定 GUID/ID，并处理 Explorer 重启后的重新注册；
- macOS：`NSStatusItem` 与 `NSMenu`；
- Linux：D-Bus `StatusNotifierItem`，没有 Watcher 时报告不支持。

托盘不可用时，应用不得把“关闭到托盘”作为唯一出口。

## 系统通知

通知包含稳定 ID、标题、正文、紧急程度、激活载荷和可选操作。结果区分已提交、权限拒绝、不支持和失败，不能只返回 `bool`。

- Windows：优先 Windows App SDK App Notifications；参考 GLFW 后端提供基础 Win32 通知降级；
- macOS：`UNUserNotificationCenter`；
- Linux：`org.freedesktop.Notifications`。

通知点击或托盘事件可能来自原生回调，必须经 `UiDispatcher::post` 后才能访问 ViewModel、Navigator 或 UI 节点。

`UiApp` 在接收 `PlatformHost` 后，会把 `DesktopServices` 的事件通道绑定到自己的
`UiDispatcher`。平台后端只调用 `publishEvent`；处理函数总是在后续 UI 调度边界执行。
移除处理函数后，已经排队但尚未执行的事件也会失效，避免应用退出期间访问旧 Router。

## 生命周期

产品层决定关闭策略：

```text
无活动业务：关闭窗口 → 退出
有活动业务且托盘可用：关闭窗口 → 隐藏到托盘
有活动业务但托盘不可用：显示确认 → 退出或取消
```

“退出应用”“隐藏窗口”“结束会话”是三个独立命令。平台层只执行产品给出的 `Close`、`Hide` 或 `Cancel` 决策。

FocusTomato 当前策略为：

1. 没有活动会话时，关闭主窗直接退出；
2. 有活动会话且托盘安装成功时，关闭主窗只隐藏，计时继续；
3. 有活动会话但托盘不可用时，取消原生关闭并显示退出确认；
4. 托盘双击或“打开”恢复、聚焦窗口；
5. 托盘可暂停/继续当前会话，菜单状态与 ViewModel 同步；
6. 托盘“退出”在存在活动会话时先确认，并说明快照恢复行为；
7. 专注或休息自然结束后发送系统通知；点击通知恢复到完成页或任务页。

## 验证方法

无头契约测试覆盖：

- 创建选项完整传递；
- capability 降级；
- 窗口状态命令；
- 自绘命中区域优先级；
- 托盘与通知事件只通过稳定 ID；
- 原生事件不会在错误线程直接访问 UI。

Windows 原生测试覆盖：

- 系统边框、自绘边框和透明 framebuffer 创建；
- 标题区域拖拽、八方向缩放、最小化、最大化和恢复；
- 100%、125%、150%、200% DPI；
- 最大化不覆盖任务栏；
- 托盘创建、更新、移除和 Explorer 重启恢复；
- 通知发送与激活；
- 隐藏到托盘后恢复窗口；
- Debug/Release OpenGL 生命周期。

当前可重复执行的验证命令：

```powershell
cmake --build build --config Debug --target `
  WhatsUIWindowTests WhatsUIFocusTomatoApp WhatsUIFocusTomatoCapture -j 4
./build/tests/Debug/WhatsUIWindowTests.exe
./build/examples/Debug/WhatsUIFocusTomatoApp.exe --new-task-smoke
./build/examples/Debug/WhatsUIFocusTomatoCapture.exe `
  ./artifacts/focus_tomato_desktop_capabilities
```

契约测试验证创建参数和桌面事件线程切换；原生 smoke 验证真实 GLFW/OpenGL
窗口及 Dialog 生命周期；截图使用与产品相同的 520 × 720 壳层，包含自绘标题栏。

## 实现索引

- `include/wui/platform.h`：窗口、托盘、通知和 capability 公共契约；
- `include/wui/app.h`、`src/whatsui/core/app.cpp`：结构化创建和 UI 线程事件绑定；
- `src/whatsui/platform/glfw_platform.cpp`：GLFW 通用窗口能力、Win32 自绘边框、托盘和基础通知；
- `examples/focus_tomato/presentation/components/common_components.*`：不依赖平台对象的标题栏视图；
- `examples/focus_tomato/presentation/focus_router.*`：页面壳层、标题栏命中区域和可逆导航；
- `examples/focus_tomato/main.cpp`：关闭策略、托盘命令、通知激活和业务状态串联；
- `tests/window_tests.cpp`：创建参数和桌面事件调度契约。

## 当前后端支持矩阵

| 能力 | Windows GLFW | macOS GLFW | Linux GLFW |
|---|---|---|---|
| 系统边框 | 支持 | 支持 | 支持 |
| 无边框 | 支持 | 支持 | 支持 |
| 透明 framebuffer | 支持，运行时探测 | 支持，运行时探测 | 取决于 compositor |
| 系统拖拽/缩放 | Win32 原生命中测试 | 尚未实现 Cocoa bridge | 尚未实现 Wayland/X11 bridge |
| 托盘 | `Shell_NotifyIcon` | 尚未实现 `NSStatusItem` bridge | 尚未实现 StatusNotifierItem bridge |
| 系统通知 | Win32 基础通知降级 | 尚未实现 UserNotifications bridge | 尚未实现 Notifications D-Bus bridge |

公共 API 对未实现能力返回 `Unsupported`，不静默假装成功。后续后端必须遵循同一契约补齐，不能在应用层加入平台条件分支。

这里的“跨平台”指公共应用代码与能力契约跨平台；上表标为“尚未实现”的原生适配器
不属于已交付能力。Windows 是当前完成并经过本机编译、契约测试、原生 smoke 和视觉
对照的参考实现。
