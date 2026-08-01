# Declarative API Optimization Roadmap

状态：Active

本文记录 ADR-006 第一阶段实现后的架构评审结论和后续优化顺序。目标不是推翻现有方向，而是在保留单一 `wui` 命名空间、无后缀 Builder、`*Node` 运行时类型、move-only Builder、`unique_ptr` 单一所有权和 `build() &&` 的基础上，补齐组合安全、线程安全和长期维护能力。

## 已确认保留的设计

- 页面作者使用 `wui::Button`、`wui::Text`、`wui::Column`。
- retained tree 使用 `wui::ButtonNode`、`wui::TextNode`、`wui::ColumnNode`。
- Builder 是 move-only，所有权只能通过 `build() &&` 转移。
- 运行时保持单棵 retained Node tree，不引入第二套 Widget/Element 树。
- State 使用 `get()`、`set()`、`post()`，后台更新通过 `UiContext` 投递。
- API inventory、行为测试和视觉基线作为重构门禁。

## P0：优先修复的正确性问题

### 1. 声明式组合能力不能直接复制运行时继承关系

`ContainerNode` 只表示运行时能够持有子节点，不代表页面作者可以传入任意类型、任意数量的 children。声明式层需要按语义拆分：

| 能力 | 适用对象 | 规则 |
| --- | --- | --- |
| `AnyChildren` | `Box`、`Row`、`Column`、普通内容容器 | 接受零到多个任意 Node |
| `SingleContent` | `ScrollView`、`Dialog`、`Drawer`、`Field`、`AccordionItem` | 恰好一个或至多一个专用 content |
| `TypedChildren<T>` | `RadioGroup`、`Accordion`、`AvatarGroup` | 只接受指定 Builder/Node 类型 |
| `ItemFactory` | `TabList`、`Toolbar`、`Breadcrumb` | 通过 `tab/item/option` API 创建并登记语义项 |
| `Slots` | `CardHeader` 等复合控件 | 每个槽位具有独立名称和数量规则 |

第一切片先处理 `ScrollView`：删除通用 `children()`，增加单一 `content()`；对应运行时节点也必须拒绝多个直接 child，避免“只布局第一个但绘制全部”的无效树。

### 2. UI 线程约束必须在 Release 中成立

当前 `WUI_ASSERT_UI_THREAD` 在 Release 中为空操作，而 Node、UiRoot、Navigator、Overlay 和焦点对象仍然是 UI-thread confined。后续需要：

- 已 attach 的运行时树绑定明确的 `UiContext`/owner token。
- 所有公开树修改入口在 Debug 和 Release 都校验 owner thread。
- 明确定义 detached Node 是否允许后台构建；允许时不得绑定或读取 UI State。
- 后台更新只通过 `UiContext::post()` 或 `State::post()` 进入 UI 队列。
- 消除全局单一 UI thread ID 与 per-app `UiContext` 的双重模型。

### 3. 生命周期回调与 `noexcept` 必须一致

`UiRoot::setContent()`、递归 detach 和 Node 析构路径会执行 `std::function` 回调。不能只靠注释假设回调永不抛出。需要在以下方案中统一选择：

- 捕获回调异常并写入结构化诊断通道；或
- 移除不真实的 `noexcept`，为 attach/detach 定义失败和回滚语义。

任何方案都必须保证析构路径不会因为用户回调直接 `std::terminate`。

## P1：可维护性和扩展性

### 4. 拆分声明式聚合头

当前 `wui/declarative.h` 同时包含基础控件、结构控件、响应式绑定和 keyed reconciliation。计划拆分为：

```text
wui/declarative/core.h
wui/declarative/layout.h
wui/declarative/input.h
wui/declarative/feedback.h
wui/declarative/navigation.h
wui/declarative/collections.h
wui/declarative/structural.h
wui/declarative.h
```

聚合头继续保留，但领域头必须能够独立由 external consumer 编译。

### 5. 去除复杂 modifier 的双份实现

`&`/`&&` 两个公开重载继续保留，但 `bind()`、`then()`、`content()` 等复杂行为只能有一份内部实现：

```cpp
void applyValue(Value value);

Self& value(Value value) &
{
    applyValue(std::move(value));
    return *this;
}

Self&& value(Value value) &&
{
    applyValue(std::move(value));
    return std::move(*this);
}
```

逐 modifier inventory 应验证左右值重载成对出现，并防止两套逻辑漂移。

### 6. 重做 keyed reconciliation 契约

- 使用强类型 `NodeKey`。
- 空 key、重复 key 不再静默改写，必须进入结构化诊断；测试模式下失败。
- 用 key index/hash map 避免 O(n²) 搜索。
- 明确区分 insert、remove、move、update。
- 同 key 数据变化应优先更新 Props，不能默认销毁 Node 并丢失焦点、选区和瞬时状态。
- 在增量 Props 模型完成前，文档必须明确当前 key 只能保留“值未变化”的节点。

### 7. 建立 Component/Props/Callbacks 边界

复杂页面组件需要独立 Props 和 Callback 对象，避免把 ViewModel、Window 或临时局部变量直接引用捕获到长生命周期 Node。框架需要提供 owner/lifetime token 或 weak callback 辅助设施，使页面销毁后的后台完成事件安全失效。

## P2：API 一致性与诊断体验

### 8. 评估让 `build()` 保留具体 Node 类型

当前通用 `build()` 返回 `NodePtr`，会过早丢失 `NodeT`。候选方案是统一返回 `std::unique_ptr<NodeT>`，由 `asNode()` 在组合边界上转为 `NodePtr`。这样可以删除 `Dialog::build()` 特例，并让 `auto node = Button(...).build()` 保留 `ButtonNode` API。

无论是否调整返回类型，文档都必须强调：当前 Builder 在构造时已经创建 Node，`build()` 是一次性 finalize/ownership transfer，不是 Flutter 式可重复生命周期回调。

### 9. 拆分身份属性

删除含义模糊的 `accessibilityId`，分别提供：

- `automationId`：UI 自动化和稳定测试定位。
- `accessibleLabel`：辅助技术朗读名称。
- `debugName`：Inspector、日志和诊断。
- `NodeKey`：结构 reconciliation 身份。

这些属性不能互相回退，重复 automationId 和非法 key 必须进入统一诊断通道。

### 10. 改善编译诊断和自定义 Builder 扩展

- 为 lvalue Builder、空 NodePtr、错误 child 类型提供短而明确的静态诊断。
- 为自定义 Builder 提供公开、最小的 capability 组合接口，避免依赖 `detail`。
- 增加 clean external-consumer 测试，覆盖领域头、聚合头和自定义 Node/Builder。

## 实施与提交策略

每个切片遵循以下顺序：

1. 先增加编译期或运行期失败测试。
2. 修改最小 API 和实现使测试通过。
3. 迁移仓库内调用点，不保留未发布旧 API 别名。
4. 运行 Debug/Release、行为、生命周期和相关视觉基线。
5. 每个独立能力单独提交，避免线程、组合和头文件拆分混入同一提交。

## 当前进度

已完成第一切片：`ScrollView` 从 `AnyChildren` 迁移到 `SingleContent`，并建立了可复用的 `SingleContentBuilderBase`、Node child invariant hook、空内容/多内容事务验证以及 Debug/Release 契约测试。

已完成第二切片：`RadioGroup`、`Accordion`、`AvatarGroup` 使用 `TypedChildren<T>`，错误 Builder 子类型在编译期不可用；`TabList`、`Toolbar`、`Breadcrumb` 只保留语义 item factory。六个运行时容器同时验证直接 child 类型，避免绕过 Builder 形成无效 retained tree。

下一切片：统一 `UiContext` owner token、Release 树修改线程验证和结构化诊断通道。
