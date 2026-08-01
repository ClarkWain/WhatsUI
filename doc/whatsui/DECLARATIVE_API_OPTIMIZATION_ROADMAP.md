# Declarative API Optimization Roadmap

状态：Completed（2026-08-01）

本文记录 ADR-006 后续优化的最终落地状态。WhatsUI 保留单一 `wui`
作者命名空间、无后缀 Builder、`*Node` 运行时类型、move-only Builder、
`unique_ptr` 单一所有权和单棵 retained tree。

## 已稳定的作者契约

- 页面代码使用 `wui::Button`、`wui::Text`、`wui::Column`。
- retained tree 使用 `wui::ButtonNode`、`wui::TextNode`、`wui::ColumnNode`。
- 页面作者直接组合 Builder 或实现 `body()`，不理解 `build()`、`asNode()`、`NodePtr`。
- `ViewLike` 由框架边界统一物化；move-only `View` 只在异构路由和跨模块工厂擦除类型。
- State 使用 `get()`、`set()`、`post()`；后台线程只能通过 `post()` 或
  `UiContext::post()` 影响已挂接 UI。
- `automationId`、`accessibleLabel`、`debugName` 和 `NodeKey` 各自承担唯一语义。

## 完成结果

| 优先级 | 工作 | 最终结果 | 自动门禁 |
| --- | --- | --- | --- |
| P0 | 语义组合能力 | `AnyChildren`、`SingleContent`、`TypedChildren<T>`、`ItemFactory`、`Slots` 已分离；运行时也拒绝绕过 Builder 的非法树 | 声明式 API contract、ScrollView/Card/Accordion/Radio/Avatar/导航测试 |
| P0 | UI 线程所有权 | 每个 `UiContext` 独立绑定 owner thread；已 attach 树在 Debug/Release 都验证线程；detached Node 可后台构造 | Dispatcher、State、生命周期及 Release 测试 |
| P0 | 生命周期异常 | attach/detach/teardown/overlay 用户回调异常被隔离并写入结构化诊断，析构路径不因用户异常终止进程 | lifecycle tests |
| P1 | 领域头拆分 | `declarative.h` 成为聚合入口；builder、text、layout、input、feedback、navigation、collections、structural 可独立 include | clean domain-header consumer target |
| P1 | modifier 单实现 | 公开 modifier 保留 `&`/`&&` 对；复杂 `bind()`、`then()`、single content 共用内部实现 | API contract 与 modifier inventory |
| P1 | keyed reconciliation | 强类型 `NodeKey`、空/重复 key 验证、hash 索引、非法快照回滚、可选 Props updater | structural smoke tests |
| P1 | Component 边界 | `body()` 或返回具体 Builder 的函数配合 Props + Callbacks；`CallbackLifetime` 让已销毁 owner 的回调安全失效 | lifecycle callback test |
| P1 | 无 build 作者体验 | `body()` 组件和 `ViewLike` 可直接进入 children/content/slot、结构工厂、Root/Window、Navigator、Overlay/Dialog；公开 `asNode()` 删除 | contract、domain-header、window、Focus Tomato tests |
| P2 | 低层 build 类型 | 控件实现/节点测试仍可用具体 `unique_ptr<NodeT>`；应用作者路径不暴露物化 | compile-time contract |
| P2 | 身份拆分 | 删除旧 `accessibilityId`；重复 automation ID、非法 key、缺失 accessible name 进入统一诊断 | contract、UIA、lifecycle tests |
| P2 | 扩展与诊断 | 公开最小 Builder 基类/能力 mixin；错误 child、左值消费、空节点产生短诊断 | external custom-builder consumer |

## 组合能力

| 能力 | 代表控件 | 规则 |
| --- | --- | --- |
| `AnyChildren` | `Box`、`Row`、`Column` | 零到多个任意 NodeLike |
| `SingleContent` | `ScrollView`、`Dialog`、`Drawer`、`AccordionItem` | 至多一个具名 content，重复设置为替换 |
| `TypedChildren<T>` | `RadioGroup`、`Accordion`、`AvatarGroup` | 只接受指定 Builder/Node 类型 |
| `ItemFactory` | `TabList`、`Toolbar`、`Breadcrumb` | 只能由语义 factory 登记 item |
| `Slots` | `CardHeader` | `media()`、`action()` 独立槽位；不公开 `children()` |

## KeyedForEach 更新语义

- 三参数构造保留简单模型：key 和值都未变化时复用 Node；同 key 值变化时重建该行。
- 四参数构造可传 `ItemUpdater(Node&, const T&)`：同 key 值变化时原地更新 Props，
  Node、焦点、选区和瞬时状态保持不变。
- 初始空 key/重复 key 直接失败；运行中的非法快照保留上一棵合法树并发出
  `InvalidNodeKey` 诊断。

## UI 所有权与诊断

`UiDiagnostic` 统一承载：

- `WrongThreadMutation`
- `LifecycleCallbackException`
- `DuplicateAutomationId`
- `InvalidNodeKey`
- `MissingAccessibleName`

诊断 handler 始终在所属 UI Context 的 owner thread 执行。已挂接树的结构修改
违反线程约束时抛出 `std::logic_error`；声明为 `noexcept` 的低层属性失效入口采用
fail-fast，因此业务层应通过 `State::post()` 投递后台结果。

## 交付门禁

完成定义包括：Debug/Release 编译、声明式 contract、State/Dispatcher/lifecycle、
Focus Tomato 新建任务与 Overlay/Dialog 回归、Component Gallery、外部领域头消费者、
Software/OpenGL 相关视觉门禁和人工截图比较。新增 Builder 或 capability 时，必须同步
更新 `DECLARATIVE_API_INVENTORY.md` 与契约测试。

最终验收（2026-08-01）：Release 全量 CTest 208/208 通过。Focus Tomato 的任务列表、
会话设置、专注计时、完成提醒和短休息截图已逐页比较；计时页保持紧凑宽度，主操作使用
大尺寸图标按钮，次操作降级显示，未发现裁切、重叠或无效留白。

后续作者体验验收（2026-08-01）：Focus Tomato presentation 已移除全部 `.build()` 与
`asNode()`；静态辅助组件返回具体 `Box/Row/Column`，仅 Router 保留一个动态 `View`
边界。迁移后的五张产品截图人工复核无布局、层级或裁切变化。
