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
7. `ADR-006-declarative-authoring-and-node-boundary.md`
8. `DECLARATIVE_API_INVENTORY.md`
9. `DECLARATIVE_API_OPTIMIZATION_ROADMAP.md`
10. `DECLARATIVE_COMPONENT_AUTHORING.md`
11. `DECLARATIVE_MODIFIER_INVENTORY.md`
12. `REACTIVITY_AND_STRUCTURE.md`
13. `LAYOUT_MODEL.md`
14. `TEXT_INPUT_AND_IME.md`
15. `WINDOWS_IME_DPI_EVIDENCE_TEMPLATE.md`
16. `TESTING_AND_VALIDATION.md`（含 sanitizer CI gate）
17. `../WINDOWS_TEXT_RENDERING_POSTMORTEM.md`（Windows 文字发虚故障复盘）
18. `FLUENT_TEXT_BASELINE_POSTMORTEM.md`（控件文字与 Todo 行对齐故障复盘）
19. `FLUENT_VISUAL_QUALITY_GATES.md`（一物理像素级视觉发布门禁）
20. `COLLECTION_VIRTUALIZATION_ARCHITECTURE.md`（集合控件 viewport、回收与懒加载架构）
21. `COLLECTION_VIRTUALIZATION_IMPLEMENTATION_PLAN.md`（集合虚拟化分步实现、测试与最终基线）
22. `COLLECTION_VIRTUALIZATION_BASELINE.md`（集合虚拟化最终验证结果与测试基线）

## 文档职责

- `../../WHATSUI_ARCHITECTURE.md`：统一总纲，适合快速了解整体方向和边界。
- `ADR-001-positioning-and-scope.md`：说明 WhatsUI 为什么存在、面向什么场景、不打算做什么。
- `ADR-002-single-tree-runtime-and-state.md`：说明为什么采用单树保留运行时，以及状态/结构更新规则。
- `ADR-003-navigation-window-and-platform-host.md`：说明多页面、多窗口、浮层和平台壳的关系与边界。
- `ADR-004-theme-and-authoring.md`：说明 UI 编写方式、样式体系和复合控件策略。
- `ADR-005-declarative-builder-api.md`：说明声明式构建器编写 API（move-only 构建器 + CRTP + 变参 children），以及为何保留 `unique_ptr` 而不改用句柄。
- `ADR-006-declarative-authoring-and-node-boundary.md`：定义声明式 Builder、运行时 `*Node`、单一 `wui` 作者命名空间、Builder 能力分层、所有权入口、组件边界和迁移验收标准。
- `DECLARATIVE_API_INVENTORY.md`：记录全部 Builder/Node 映射、组合能力、公共所有权契约和当前自动门禁范围。
- `DECLARATIVE_API_OPTIMIZATION_ROADMAP.md`：记录 ADR-006 第一阶段后的风险分级、目标能力模型和后续独立优化切片。
- `DECLARATIVE_COMPONENT_AUTHORING.md`：说明普通函数组件、Props/Callbacks、响应式数据和 `CallbackLifetime` 的安全边界。
- `DECLARATIVE_MODIFIER_INVENTORY.md`：定义 fluent modifier 的左右值配对、共享实现和组合能力清单。
- `RETIRED_EXAMPLES.md`：记录不再构建或维护的历史示例；当前 Todo App 已归档。
- `REACTIVITY_AND_STRUCTURE.md`：说明响应式绑定（`Text().bind`、节点 teardown 生命周期）、结构控件 `If`/`ForEach`、可插拔文本测量与新控件。
- `LAYOUT_MODEL.md`：细化约束布局模型、容器语义、逻辑单位与滚动边界。
- `TEXT_INPUT_AND_IME.md`：细化文本输入、组合输入、光标、选区和平台 IME 会话边界。
- `WINDOWS_IME_DPI_EVIDENCE_TEMPLATE.md`：Windows 发布候选版本的 100%/150%/200% DPI 与 IME 人工证据记录模板。
- `TESTING_AND_VALIDATION.md`：细化 golden image、布局快照、行为回归与 Software backend 验证策略。
- `../WINDOWS_TEXT_RENDERING_POSTMORTEM.md`：记录 ClearType 双重 Alpha 衰减、Windows 150% DPR 误判、字重丢失及其端到端修复和视觉证据。
- `FLUENT_TEXT_BASELINE_POSTMORTEM.md`：记录 `lineHeight / 2` 被误当 baseline 偏移、Todo 局部 4-DIP 下沉、修复公式、截图和四档 DPI 数据。
- `FLUENT_VISUAL_QUALITY_GATES.md`：定义文字、图标、圆角、描边、状态、裁切和 100/125/150/200% DPI 的一物理像素级视觉验收。
- `COLLECTION_VIRTUALIZATION_ARCHITECTURE.md`：定义集合控件共享 viewport math、节点回收、数据懒加载与分阶段迁移方案。
- `COLLECTION_VIRTUALIZATION_IMPLEMENTATION_PLAN.md`：拆解集合虚拟化实施步骤、每步验证命令、最终验证门禁与基线记录字段。
- `COLLECTION_VIRTUALIZATION_BASELINE.md`：记录集合虚拟化实现后的 Debug/Release/Visual/ASan 验证结果、基线期望与已知后续项。

## 后续扩展建议

后续如继续细化，可考虑补这些文档：

- `TEXT_INPUT_AND_IME.md`
- `TESTING_AND_VALIDATION.md`
- `PERFORMANCE_AND_CACHING.md`
