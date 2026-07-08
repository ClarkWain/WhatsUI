# ADR-002: Single-Tree Runtime And State

状态：Accepted Draft

## Context

WhatsUI 需要一套足够轻量、可预测、适合 C++ 所有权模型的 UI 运行时。如果采用虚拟 DOM、重型 reconciliation，或 Flutter 式多主树体系，复杂度会迅速上升。

同时，项目仍需要支持局部状态更新、动态结构变化和后续缓存优化。

## Decision

运行时采用单树保留模型：

- 运行时只维护一棵长期存在的 `Node` 树。
- 节点统一承担测量、布局、绘制、命中与事件处理职责。
- 树所有权使用父子 `unique_ptr` 语义。

状态层保持最小化：

- `State<T>`：可观察值。
- `Binding<T>`：控件与状态之间的双向连接。
- `Computed<T>`：轻量派生值。

动态结构通过显式结构控件表达，而不是整页 diff：

- `If`
- `Switch`
- `ForEach`
- `SlotHost`

更新通过脏标记驱动：

- `NeedsStyle`
- `NeedsLayout`
- `NeedsPaint`
- `NeedsCompositing`

## Consequences

- 页面状态变化不会触发整页重建。
- 局部结构变化被限制在结构控件内部。
- C++ 生命周期与所有权模型保持清晰。
- 后续即使补 layer/cache，也不需要推翻 API 形态。
- 框架不会引入另一套隐藏很深的 UI diff 系统。
