# Fluent Todo UI 验收清单（Windows 优先）

状态：P0 visual and core task-flow automated verification complete. Windows
IME/DPI manual sign-off and final release-owner gates remain open.

本清单定义 WhatsUI Todo demo 的视觉与交互完成条件。**hash 一致、能编译或单元测试通过不等于视觉通过**；每次视觉改动都必须在 Software backend 下生成并人工审查窄屏、常规、宽屏截图。

## 目标

做出一套接近 Windows Fluent 2 语言的、可交互、信息清晰的 Windows Todo List，而不是控件拼接演示。

## P0：视觉与版式

- [x] 常规宽度（640×560）有单一、稳定的内容列；任务卡、分组标题、输入区和 Hero 卡使用同一列宽，不出现无意义的大块右侧空白。
- [x] 宽屏（1180×760）内容列居中并有最大宽度；不得拉伸成全屏散乱的长条。
- [x] 窄屏（360×720）输入区、任务标题、状态和操作区不重叠、不溢出；操作区降级为紧凑表现。
- [x] 顶部标题、日期/说明、进度、分组标题、任务标题和辅助信息有清晰、可读的 Fluent 层级。
- [x] Surface、边框、圆角、阴影、accent、hover、pressed、focus 与默认 Fluent token 一致；不残留硬编码“蓝块 + 白卡片”样板感。
- [x] 文本在 Software backend 中完整显示；不得出现裁切、像素块、下溢或不支持字形。截图文案优先使用已验证字体路径可显示的字符。
- [x] 删除操作是低权重的次级动作；完成任务是行内最主要操作；危险操作不能主导视觉层级。

## P0：可用交互

- [x] 通过真实 TextInput 输入任意任务标题后可添加；空/纯空白标题不可添加。
- [x] Checkbox、键盘 Space/Enter 和鼠标均可完成/恢复任务；进度与分组立即更新。
- [x] 删除与清除已完成项使用 Dialog 确认，Esc 取消，焦点正确恢复。
- [x] Tab/Shift+Tab 顺序符合阅读顺序，所有操作有可见 focus 状态。
- [x] 鼠标 hover/pressed、禁用及键盘焦点在截图/行为测试中可验证。

## P0：可验证交付

- [x] `WhatsUITodoApp <out> --size 360x720`、`640x560`、`1180x760` 都可生成四个完整状态截图。
- [x] 视觉 review CTest 覆盖以上三种尺寸并保存 CI artifact。
- [x] 主审人工打开并审阅 `todo_0`（空）、`todo_2`（混合）、`todo_3`（删除后）三种关键状态的三种尺寸。
- [x] `WhatsUITodoGlfw` Debug 构建成功，并以同一棵 Todo UI tree 运行。
- [x] WhatsCanvas 单元测试与 WhatsCanvas Software visual CTest 均通过。

## 暂不纳入本轮

- Windows 原生 IME 候选窗/TSF。
- 其他平台适配。
- 虚拟列表、拖拽排序、同步存储、提醒与多列表。

## Recorded P0 evidence (2026-07-13)

- `whatsui_todo_ui_interaction_smoke_tests` drives the actual Todo tree: text
  input, mouse/Space/Enter completion, Tab/Shift+Tab focus, cancel and confirm
  deletion/clear-completed, plus hover/pressed/disabled cancellation paths for
  the real Add button and task checkbox. It passed 20 consecutive Debug runs
  without a CRT dialog.
- `whatsui_todo_visual_regression` and `whatsui_todo_visual_review` validate
  the current deterministic four-state captures. The regular review also
  hashes `todo_scroll_end.ppm`, proving the last task is reachable at 640×560.
- `whatsui_glfw_resize_tests` verifies the native OpenGL viewport after
  640×560 → wide → narrow resizes; `whatsui_todo_native_perf_smoke` runs the
  interactive GLFW Todo under a bounded watchdog. It exercises composer
  text-entry/Enter submission, a below-fold task-checkbox toggle, and a radio
  selection, with independent conservative latency budgets (1200/900/900ms).
  The 2026-07-13 Debug evidence measured 32.499/24.290/25.326ms respectively.
- `whatsui_todo_ui_interaction_smoke_tests` also snapshots the actual Todo
  accessibility tree: composer value, task Checkbox name/state, named Delete
  command, modal background isolation, and Esc restoration.
- Full Windows/MSVC Debug gate: `ctest --test-dir build-fluent-visual -C Debug
  --output-on-failure` completed on 2026-07-13 with **63/63 passed** in
  189.51 seconds. This includes the WhatsCanvas unit, Software visual,
  package-consumer, Todo visual-review, interaction, native-performance, and
  GLFW-resize tests.
