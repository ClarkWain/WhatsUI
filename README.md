# WhatsUI

[![CI](https://github.com/ClarkWain/WhatsUI/actions/workflows/ci.yml/badge.svg)](https://github.com/ClarkWain/WhatsUI/actions/workflows/ci.yml)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)](CMakeLists.txt)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C.svg)](CMakeLists.txt)

**WhatsUI is a retained-mode C++17 desktop UI framework with a Fluent 2 default
design system, deterministic visual tests, and a Windows-first GLFW/OpenGL host.**

WhatsUI 面向编辑器、调试器、设置页、数据工具和嵌入式桌面 UI。它使用保留节点树、
声明式 Builder、响应式状态和可预测的输入/布局/绘制管线，而不是把浏览器运行时搬进
C++。

> 当前版本为 **0.1 Developer Preview**。Windows 是主交付平台；1.0 仍需要干净候选
> 版本、IME/DPI/Narrator 实机证据、包/法律材料与发布负责人签核。详见
> [release checklist](doc/whatsui/RELEASE_CHECKLIST.md)。

## 能力概览

### Fluent 2 默认设计系统

- Fluent 风格的颜色、暗色主题、排版、间距、圆角、描边、阴影/elevation、焦点环和
  交互状态 token；主题可在运行时切换，并支持设计系统扩展。
- 像素对齐几何、100/125/150/200% DPI 视觉回归，以及 Windows 小字号文本的原生
  栅格化路径。
- 内置 [Fluent System Icons](doc/whatsui/FLUENT_ICONS.md) Regular/Filled 字体资源。

### 组件

| 类别 | 已提供的能力 |
| --- | --- |
| 基础输入 | Button、IconButton、ToggleButton、Split/Menu/Compound Button、Label、Text、Link、TextInput、TextArea、SearchField、Field 与校验消息。 |
| 选择与数值 | Checkbox（三态）、Radio/RadioGroup、Switch、Slider、ProgressBar、Rating/RatingDisplay、ListBox、Combobox、Dropdown。 |
| 导航与集合 | Toolbar、TabList/TabPanel、Breadcrumb、Accordion、Tree、ListView、VirtualList、Table、DataGrid、Calendar、DatePicker、TimePicker。 |
| 表面与浮层 | Card、Dialog、Drawer、Popup、Popover、TeachingPopover、Tooltip、菜单与命令面板。 |
| 反馈与身份 | MessageBar、Toast、Spinner、Badge/CounterBadge/PresenceBadge、Avatar/AvatarGroup、Persona、Image。 |
| 运行时 | Column/Row/Stack、滚动、响应式 State、焦点/Tab traversal、指针捕获、窗口/Overlay 生命周期与 UI Inspector。 |

完整 API、完成度和设计取舍见 [controls](doc/whatsui/CONTROLS.md)、
[Fluent component checklist](doc/whatsui/FLUENT_COMPONENT_COMPLETION_TODOS.md) 与
[roadmap](ROADMAP.md)。

### Windows、文本与无障碍

- GLFW + OpenGL 原生窗口 host；Software renderer 用于确定性截图与无 GPU 测试。
- Windows 默认采用高质量原生文本栅格化并结合 HarfBuzz shaping；TextInput 支持
  UTF-8 编辑、选择、撤销/重做、剪贴板、IME composition 与 IMM32 候选窗口定位。
- Windows UI Automation fragment tree 支持 Invoke、Toggle、Value、RangeValue、
  Selection/SelectionItem、焦点、边界和原生事件，并通过 UI 线程调度。

富文本 TextRange、Narrator 和多显示器 DPI 的最终实机签核仍属于 1.0 gate。参见
[Windows support matrix](doc/whatsui/WINDOWS_SUPPORT_MATRIX.md)、
[Windows IME](doc/whatsui/Windows-IMM32-IME.md) 与
[Windows UIA bridge](doc/whatsui/WINDOWS_ACCESSIBILITY_BRIDGE.md)。

## 示例应用

| 示例 | Software capture | GLFW/OpenGL 交互窗口 |
| --- | --- | --- |
| Todo | `WhatsUITodoApp` | `WhatsUITodoGlfw` |
| Windows Settings | `WhatsUISettingsApp` | `WhatsUISettingsGlfw` |
| Command Palette | `WhatsUICommandPaletteApp` | `WhatsUICommandPaletteGlfw` |
| Debug Inspector | `WhatsUIDebugInspectorApp` | `WhatsUIDebugInspectorGlfw` |
| Hello Window | — | `WhatsUIHelloWindow` |

Todo 是端到端参考：本地持久化、添加/编辑/删除、完成状态、筛选/搜索、撤销、响应式
布局和原生交互均有自动化覆盖。见 [Todo demo delivery](doc/whatsui/TODO_DEMO_DELIVERY.md)。

## 快速开始（Windows Todo）

```powershell
git clone --recursive https://github.com/ClarkWain/WhatsUI.git
cd WhatsUI

# 默认构建并启动 Release Todo；也可传 Debug。
.\build.bat
```

手动配置完整渲染/示例构建：

```powershell
git submodule update --init --recursive
cmake -S . -B build-windows `
  -DWHATSUI_WITH_WHATSCANVAS=ON `
  -DWHATSUI_BUILD_TESTS=ON `
  -DWHATSUI_BUILD_EXAMPLES=ON
cmake --build build-windows --config Release --parallel 4
& .\build-windows\examples\Release\WhatsUITodoGlfw.exe
```

## 构建与测试

### Headless runtime

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

### 完整 Windows renderer / Fluent / Todo gate

```powershell
cmake -S . -B build-release `
  -DWHATSUI_WITH_WHATSCANVAS=ON `
  -DWHATSUI_BUILD_TESTS=ON `
  -DWHATSUI_BUILD_EXAMPLES=ON
cmake --build build-release --config Release --parallel 1

# 部分 WhatsCanvas 测试会在内部运行 CMake/MSBuild；Windows 完整 gate 必须串行。
ctest --test-dir build-release -C Release --output-on-failure --parallel 1
```

隔离 Release gate 的自动化证据（Fluent DPI、Todo、包消费、DirectWrite 与真实 Windows
UIA）见 [release checklist](doc/whatsui/RELEASE_CHECKLIST.md)。

### Sanitizer

Sanitizer 覆盖 headless runtime；Windows/MSVC 使用 ASan，Linux/Clang 使用 ASan +
UBSan，不与 WhatsCanvas renderer 支持矩阵混用。

```powershell
cmake -S . -B build-asan -DWHATSUI_ENABLE_SANITIZERS=ON
cmake --build build-asan --config Debug
ctest --test-dir build-asan -C Debug --output-on-failure
```

## CMake 消费

核心包导出 `WhatsUI::WhatsUI`。开启 WhatsCanvas 后，Windows 包还导出 Software/OpenGL、
advanced-text 依赖及 `WhatsUI::Glfw`：

```cmake
find_package(WhatsUI 0.1 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE WhatsUI::WhatsUI)

# Requires a package built with WHATSUI_WITH_WHATSCANVAS=ON.
target_link_libraries(my_window PRIVATE WhatsUI::Glfw)
```

预览阶段不承诺 ABI 兼容；升级时应完整重编译 WhatsUI、WhatsCanvas 和消费方。详情见
[upgrade and contributing](doc/whatsui/UPGRADE_AND_CONTRIBUTING.md) 和
[stability policy](doc/whatsui/STABILITY_AND_COMPATIBILITY.md)。

## 仓库结构

```text
include/wui/          Public C++ API
src/whatsui/          Runtime, widgets, platform backends
examples/             Todo, Settings, Command Palette, Inspector, Hello Window
tests/                Unit, interaction, native UIA, DPI and visual regressions
assets/fonts/         Reviewed Fluent System Icons font assets
doc/whatsui/          ADRs, API contracts, platform and release documentation
third_party/WhatsCanvas/
                       Renderer, shaping, software/OpenGL backends (submodule)
```

## 项目状态与贡献

- 先阅读 [architecture](WHATSUI_ARCHITECTURE.md)、[roadmap](ROADMAP.md) 与
  [development TODO](DEVELOPMENT_TODO.md)。
- UI 改动必须同时考虑 100/125/150/200% DPI、键盘/焦点、可访问性语义和视觉回归。
- 新功能不能因为“有一个 demo”就声称“已发布”；IME、兼容性、供应链和人工验收均有
  单独发布 gate。

WhatsUI 使用 [MIT License](LICENSE)；第三方依赖、NOTICE 和 SBOM 信息见
[NOTICE](NOTICE) 与 [SBOM](doc/whatsui/SBOM.md)。
