# WhatsUI 文档索引

状态：Draft v0.1

本目录用于承载 WhatsUI 的结构化架构文档。仓库根部的 `WHATSUI_ARCHITECTURE.md` 保留为统一总纲，这里则拆成便于长期维护的主题文档与 ADR。

## 阅读顺序

1. `../../WHATSUI_ARCHITECTURE.md`
2. `ADR-001-positioning-and-scope.md`
3. `ADR-002-single-tree-runtime-and-state.md`
4. `ADR-003-navigation-window-and-platform-host.md`
5. `ADR-004-theme-and-authoring.md`
6. `ADR-005-declarative-builder-api.md`
7. `REACTIVITY_AND_STRUCTURE.md`
8. `LAYOUT_MODEL.md`
9. `TEXT_INPUT_AND_IME.md`
10. `TESTING_AND_VALIDATION.md`

## 文档职责

- `../../WHATSUI_ARCHITECTURE.md`：统一总纲，适合快速了解整体方向和边界。
- `ADR-001-positioning-and-scope.md`：说明 WhatsUI 为什么存在、面向什么场景、不打算做什么。
- `ADR-002-single-tree-runtime-and-state.md`：说明为什么采用单树保留运行时，以及状态/结构更新规则。
- `ADR-003-navigation-window-and-platform-host.md`：说明多页面、多窗口、浮层和平台壳的关系与边界。
- `ADR-004-theme-and-authoring.md`：说明 UI 编写方式、样式体系和复合控件策略。
- `ADR-005-declarative-builder-api.md`：说明声明式构建器编写 API（move-only 构建器 + CRTP + 变参 children），以及为何保留 `unique_ptr` 而不改用句柄。
- `REACTIVITY_AND_STRUCTURE.md`：说明响应式绑定（`Text().bind`、节点 teardown 生命周期）、结构控件 `If`/`ForEach`、可插拔文本测量与新控件。
- `LAYOUT_MODEL.md`：细化约束布局模型、容器语义、逻辑单位与滚动边界。
- `TEXT_INPUT_AND_IME.md`：细化文本输入、组合输入、光标、选区和平台 IME 会话边界。
- `TESTING_AND_VALIDATION.md`：细化 golden image、布局快照、行为回归与 Software backend 验证策略。

## 后续扩展建议

后续如继续细化，可考虑补这些文档：

- `TEXT_INPUT_AND_IME.md`
- `TESTING_AND_VALIDATION.md`
- `PERFORMANCE_AND_CACHING.md`



