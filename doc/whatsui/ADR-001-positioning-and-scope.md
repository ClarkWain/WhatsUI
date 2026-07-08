# ADR-001: Positioning And Scope

状态：Accepted Draft

## Context

WhatsCanvas 已经提供了文本、裁剪、圆角、阴影、多后端和 Software backend 等能力，足以支撑一个轻量 2D UI 系统。问题不在于是否能画 UI，而在于应该做成什么形态。

如果 WhatsUI 试图复制 Qt、Flutter、Electron 或浏览器应用平台的目标，它会立刻进入高复杂度、高维护成本的领域；这和项目当前体量、诉求以及依赖的底层都不匹配。

## Decision

WhatsUI 的定位定为：

- 基于 WhatsCanvas 的轻量原生 UI 框架。
- 面向桌面工具、设置界面、调试面板、启动器、嵌入式 UI。
- 重点提供稳定的布局、输入、文本、导航、浮层、多窗口和可测试能力。

WhatsUI 明确不以以下目标为方向：

- 不做 CSS 引擎。
- 不做浏览器式 URL 路由平台。
- 不做大型跨端超级框架。
- 不做通用脚本运行时。
- 不追求和 Qt、Flutter、Electron 正面竞争。

## Consequences

- 架构可以明显偏向“宿主驱动 + 轻量运行时”，而不是平台全包。
- UI 编写方式可以只依赖强类型 C++ API，不需要引入独立描述语言。
- 多页面和多窗口可以做，但必须服从轻量边界。
- 文本、Software backend 和验证能力会成为 WhatsUI 的差异化优势。
