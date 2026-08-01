# ADR-006: Declarative Authoring And Runtime Node Boundary

状态：Accepted（已完成，2026-08-01）

仓库维护者已确认并完成本方向。运行时 `*Node` 命名、单一 `wui` 作者命名空间、
内部 `build()` 所有权出口、无 build 的 `ViewLike/body()` 作者路径、modifier 双引用限定、领域头拆分、身份属性拆分、
per-context UI owner、结构化诊断及 66 项映射/capability 门禁均已落地。

后续修订（2026-08-01）：正文中要求页面作者显式返回 `NodePtr` 或调用 `build()` 的示例
已由 `ViewLike` 物化协议取代。`build() &&` 仍是 Builder 的低层一次性所有权机制，
但应用作者只写控件组合、组件 `body()`、`window.content(...)` 和 Navigator/Overlay
边界。move-only `View` 只在必须存储异构页面时使用。本修订取代下文第 4、5、8 节中
与应用作者显式物化有关的旧工作流，不改变 retained tree 与 `unique_ptr` 所有权模型。

## Context

ADR-005 确立了 WhatsUI 的基本方向：使用一层轻量、move-only 的 C++ Builder 提供声明式编写体验，同时保留单棵 retained node tree 和 `unique_ptr` 单一所有权。这个运行时方向仍然成立，但当前公开 API 没有清楚区分“声明式 UI”与“运行时节点”。

重构前的 `include/wui/ui.h` 中有 66 个 Builder，其中 61 个与 `wui` 根命名空间中的具体节点同名。例如：

```cpp
wui::ui::Button // 声明式 Builder
wui::Button     // 运行时节点
```

这产生两个直接问题：

1. 仅导入 `wui::ui` 时，样式、状态和通用类型仍要写 `wui::ButtonAppearance`、`wui::State`。
2. 同时导入 `wui` 与 `wui::ui` 时，`Button`、`Text`、`Row`、`Column` 等名称产生歧义。

当前 Builder 还有几个会放大团队协作成本的问题：

- `BuilderBase` 向所有 Builder 暴露 `children()`，叶子控件也能表达非法子树。
- 多数配置方法只有 `&&` 重载，不便于具名 Builder 和条件配置。
- `operator std::unique_ptr<Node>()`、`operator->()`、`operator*()`、`get()` 和 `intoNode()` 同时存在，节点访问与所有权转移入口过多。
- `ButtonVariant`/`ButtonAppearance`、`variant()`/`appearance()` 等同义 API 在尚未发布 1.0 时制造了不必要的兼容负担。
- `accessibilityId` 容易被同时当成运行时 key、自动化 ID 和无障碍名称使用。
- 单个 `ui.h` 聚合所有 Builder 与控件头文件，声明式 API 的依赖和协作修改面过大。

## Goals

- 页面作者只需要一套主要命名空间和一套声明式词汇。
- 看到类型名称即可判断它是 Builder、运行时节点、值类型还是控制器。
- 非法的 UI 组合尽量在编译期失败，并给出短而明确的诊断。
- 节点所有权转移必须显式、单向且只能发生一次。
- 条件配置、组件拆分和自定义控件对普通 C++ 开发者自然可用。
- 本次重构不改变布局、绘制、事件、无障碍和状态更新行为；视觉输出应保持一致。

## Non-goals

- 不引入虚拟树、全树 diff 或 `shared_ptr<Node>`。
- 不把 Builder 变成运行时对象；Builder 仍只负责构造并交出 retained node。
- 不在本次命名重构中重新设计布局算法或渲染后端。
- 不保留尚未对外发布的旧类型别名。

## Decision

### 1. `wui` 是主要声明式作者 API

声明式 Builder 从 `wui::ui` 移到 `wui`。页面作者使用一个命名空间即可访问 Builder、状态、样式枚举和通用值类型：

```cpp
using namespace wui;

NodePtr buildTimerPage(std::function<void()> onStart)
{
    return Column()
        .padding(16)
        .gap(12)
        .children(
            Text("番茄钟"),
            Button("开始")
                .appearance(ButtonAppearance::Primary)
                .size(ButtonSize::Large)
                .onClick(std::move(onStart))
        )
        .build();
}
```

头文件与公共接口中仍应使用限定名称，不放置 `using namespace`：

```cpp
wui::NodePtr buildTimerPage(std::function<void()> onStart);
```

### 2. 所有具体运行时节点使用 `Node` 后缀

WhatsUI 仓库中任何继承 `Node` 并进入运行时树的具名具体类型，无论声明在公开头文件、内部头文件还是 `.cpp` 中，都必须以 `Node` 结尾：

```cpp
wui::ButtonNode
wui::TextNode
wui::ImageNode
wui::RowNode
wui::ColumnNode
wui::DialogNode
wui::SliderNode
wui::CheckboxNode
```

抽象基类 `Node`、`ContainerNode`、`ControlNode` 保持不变。`IfNode`、`ForEachNode` 已符合规则。

值类型、状态、控制器、事件和枚举不使用 `Node` 后缀：

```cpp
wui::State<int>
wui::Binding<bool>
wui::UiContext
wui::ButtonAppearance
wui::TextEditingController
```

声明式 Builder 与节点原则上形成一一对应：

| 声明式类型 | 运行时类型 | 备注 |
| --- | --- | --- |
| `Button` | `ButtonNode` | 标准映射 |
| `Text` | `TextNode` | 标准映射 |
| `Box` | `BoxNode` | 替代当前具体 `Container`；`ContainerNode` 保留为基类 |
| `TextField` | `TextFieldNode` | 替代当前单行 `TextInput` 节点命名 |
| `TextArea` | `TextAreaNode` | 多行输入节点 |
| `If` | `IfNode` | 已符合规则 |
| `ForEach`/`KeyedForEach` | `ForEachNode` | 两种构建策略共享一个运行时节点类型 |

没有直接 Builder 的具体节点也遵循同一规则，例如 `TabNode`、`TreeItemNode`、`ToolbarItemNode`、`PopupNode` 和 `VirtualListNode`。应用自己的自定义节点应遵循该规则，但不由 WhatsUI 构建系统强制扫描应用代码。

“Builder 与节点一一对应”是默认规则。仅当多个 Builder 明确表示同一个运行时策略时允许共享节点，例如 `ForEach` 与 `KeyedForEach`。新增例外必须在相关 ADR 或控件设计文档中记录原因，不能因省事让名称长期错位。

不得在 `wui` 根命名空间添加 `using Button = ButtonNode` 一类兼容别名，否则会重新制造声明式名称冲突。项目尚未发布稳定 API，本次直接迁移全部仓库内调用方。

### 3. Builder 能力按节点能力分层

基础 Builder 只包含所有节点都适用的能力：

```cpp
template<class Self, class NodeT>
class BuilderBase {
    static_assert(std::is_base_of_v<Node, NodeT>);

public:
    using node_type = NodeT;

    Self& flex(float value) &;
    Self&& flex(float value) &&;
    Self& automationId(std::string value) &;
    Self&& automationId(std::string value) &&;
    Self& debugName(std::string value) &;
    Self&& debugName(std::string value) &&;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] NodeT* node() & noexcept;
    [[nodiscard]] const NodeT* node() const & noexcept;
    NodeT* node() && = delete;
    NodePtr build() &&;
};
```

只有容器 Builder 提供 `children()`：

```cpp
template<class Self, class NodeT>
class ContainerBuilderBase : public BuilderBase<Self, NodeT> {
public:
    template<class... Children>
    Self& children(Children&&... values) &;

    template<class... Children>
    Self&& children(Children&&... values) &&;
};
```

因此 `Column().children(...)` 合法，而 `Text().children(...)` 必须在编译期失败。

可以继续按实际需要增加能力层，例如滚动容器、可选择控件或可交互表面，但不为了减少几行重复代码建立深而脆弱的 Builder 继承树。

### 4. Builder 支持链式调用与条件配置

所有配置方法必须同时支持临时值和具名变量：

```cpp
auto button = Button("删除");
if (isDangerous) {
    button.appearance(ButtonAppearance::Danger);
}
if (!canDelete) {
    button.enabled(false);
}
return std::move(button).build();
```

约定为：

- `&` 重载返回 `Self&`。
- `&&` 重载返回 `Self&&`。
- `build()` 仅提供 `&&` 重载，明确消费 Builder。
- Builder 保持 move-only。
- 移动后的 Builder 处于 empty 状态，只允许析构、重新赋值、调用 `empty()`，或调用返回 `nullptr` 的左值 `.node()`。
- 对 empty Builder 调用配置方法或 `build()` 必须抛出 `std::logic_error`，Debug 与 Release 行为一致。
- `.node()` 只允许左值调用；禁止 `Button("临时").node()`，避免 Builder 在完整表达式末尾销毁后留下悬空指针。
- `empty()` 的签名为 `[[nodiscard]] bool empty() const noexcept`。
- 成功调用 `std::move(builder).build()` 后，源 Builder 进入 empty 状态；第二次消费抛出 `std::logic_error`。
- move construction/assignment 将节点转移给目标并使源变空；从 empty Builder 移动得到 empty Builder。
- self-move assignment 不改变对象；move assignment 会先销毁目标原来持有但尚未交出的节点。
- 配置方法抛出异常时 Builder 仍持有节点；除具体 Node setter 另有更强契约外，节点值保持该 setter 声明的异常保证。

### 5. 节点访问和所有权转移只有显式入口

删除以下公开入口：

```cpp
operator std::unique_ptr<Node>();
operator->();
operator*();
get();
```

保留：

```cpp
NodeT* node() & noexcept; // 高级逃生口，不转移所有权
NodePtr build() &&;  // 转移到运行时树
```

`children(...)`、结构节点工厂和窗口 `setContent(...)` 可以通过受约束模板消费 Builder，但不得依赖公开隐式转换。消费规则为：

- 接受右值 Builder：`children(Button("保存"))`。
- 接受显式移动的具名 Builder：`children(std::move(button))`。
- 接受右值 `NodePtr` 或 `std::unique_ptr<DerivedNode>`。
- 拒绝 Builder 左值和 `unique_ptr` 左值，错误信息要求调用方显式 `std::move`。
- 拒绝空节点并抛出 `std::invalid_argument`。
- 父 Builder 必须在消费任何 child 参数前检查非 empty；empty 父 Builder 立即抛出 `std::logic_error`。
- 在修改父节点前，先将全部参数转换并验证到局部 `std::vector<NodePtr>`；转换或验证失败时父节点保持不变，但此前传入的右值 Builder/NodePtr 允许已经被消费为空。
- 参数全部转换后通过新的批量挂接入口追加到现有 children；重复调用 `children()` 的语义是追加，不是替换。
- 批量挂接对正常异常（无效 parent、空 child、生命周期预检失败）提供父节点不变保证。内存分配失败或违反“不抛异常”的 attach callback 合同时只保证树仍可析构和继续使用，不承诺回滚全部已挂接节点。
- `setContent(...)`、结构工厂和其它 Builder 消费入口遵循同一套右值、empty、异常与二次消费规则。

新增公共别名：

```cpp
using NodePtr = std::unique_ptr<Node>;
```

### 6. 属性命名保持单一含义

统一约定：

| 场景 | 形式 |
| --- | --- |
| Builder 配置 | `.appearance(value)` |
| Node 写入 | `.setAppearance(value)` |
| Node 读取 | `.appearance()` |
| State 同步读写 | `.get()` / `.set(value)` |
| State 跨线程提交 | `.post(value)` |

在 1.0 前删除同义兼容入口，例如 `ButtonVariant`、`variant()` 和 `setVariant()`，只保留 `ButtonAppearance` 体系。其它控件也要检查 `foo/setFoo`、`value/setValue`、旧枚举与新枚举是否表达重复概念。

### 7. 运行时身份、自动化和无障碍语义分离

不同 API 表达不同身份。普通 Builder 上的身份属性为：

```cpp
.automationId("task-start-button")
.accessibleLabel("开始任务：完成产品设计稿")
.debugName("TaskCard.StartButton")
```

- `key`：仅由 `KeyedForEach` 的 key provider 消费，用于该集合当前数据快照中的复用和重排；它不是 child Builder 属性，也不是全树 diff 身份。未来其它 keyed 容器需要 key 时必须另行定义公开表达方式，不能先在通用 Builder 上增加无消费者的 `.key()`。
- `automationId`：Windows UI Automation 与自动化测试身份；同一窗口内唯一且稳定。
- `accessibleLabel`：辅助技术向用户朗读的名称，可本地化。
- `debugName`：Inspector、日志和诊断信息，不参与产品语义。

`automationId` 与 `debugName` 属于所有节点的基础能力。`accessibleLabel` 由 `AccessibleBuilderMixin<Self, NodeT>` 提供，只有具备可发布无障碍语义和 `setAccessibleLabel` 契约的节点才能混入；控件 inventory 必须记录该 capability。`key` 属于 `KeyedForEach` 构造参数：

```cpp
using NodeKey = std::string;

KeyedForEach(
    items,
    [](const Task& task) -> NodeKey { return task.id; },
    [](const Task& task) { return TaskCard(task); }
);
```

key 必须非空，并且在一次 `State<std::vector<T>>` 数据快照中唯一，按 `NodeKey` 精确相等比较。首次构建发现无效 key 时抛出 `std::invalid_argument`。后续 State 更新发现无效 key 时保留上一次有效子树，不再自动追加索引修复 key，并通过结构更新诊断通道报告错误；Debug 和测试必须可观察该错误，Release 记录诊断但不得崩溃。

这些值不得互相回退或隐式复用。当前每个 `accessibilityId` 调用点必须按真实用途迁移到一个或多个新属性：自动化查询迁到 `automationId`，朗读文本迁到 `accessibleLabel`，集合复用迁到 key provider。迁移前后需要对 UIA snapshot、可访问名称和 RuntimeId 行为做等价比较。

`automationId` 的窗口级唯一性不能在独立 Builder 的 `build()` 阶段判断。它应在首次挂载、动态插入或重挂载、已挂载节点修改 automationId，以及发布 UIA snapshot 前由语义树验证器重新检查。验证不拒绝挂载或抛出异常：测试和 Debug 诊断返回明确重复路径，Release 不得崩溃，但重复 ID 仍属于无效应用输入并写入诊断日志。

### 8. 普通函数是默认组件抽象

业务组件默认是返回 `NodePtr` 的普通函数，不新增强制性的 `Component` 基类：

```cpp
struct TaskCardProps {
    std::string title;
    int tomatoCount{0};
    bool completed{false};
};

struct TaskCardCallbacks {
    std::function<void()> onStart;
    std::function<void()> onToggle;
    std::function<void()> onDelete;
};

NodePtr TaskCard(TaskCardProps props, TaskCardCallbacks callbacks);
```

Props 描述输入数据，Callbacks 描述用户意图。组件不应为了方便而依赖整个页面 ViewModel，除非组件确实拥有该业务边界。

自定义绘制或事件路由需要新运行时类型时，再实现 `CustomControlNode : ControlNode`，并为它提供同名无后缀 Builder。

### 9. 响应式绑定遵循属性方向

命名重构后继续统一响应式 API：

- 只读属性接受 `State<T>` 或 `Computed<T>`。
- 可编辑属性接受 `Binding<T>`。
- `onChange` 表达用户事件，不应要求每个页面重复实现基础双向同步。

目标写法：

```cpp
Text().text(titleState);
Checkbox("完成").checked(completedBinding);
Slider().value(volumeBinding);
```

节点层仍只操作普通值：

```cpp
CheckboxNode node;
node.setChecked(true);
```

响应式属性的完整类型设计不阻塞本次命名重构，但新 Builder API 不得继续增加互不一致的临时绑定形式。

### 10. 声明式头文件按领域拆分

将当前单个 `ui.h` 拆分为：

```text
include/wui/declarative/
    builder_base.h
    layout.h
    text.h
    input.h
    controls.h
    feedback.h
    navigation.h
    overlays.h
    collections.h
    structural.h
```

新增 `wui/declarative.h` 作为声明式聚合入口，`wui/wui.h` 继续作为完整框架聚合入口并包含它。旧 `wui/ui.h` 在本次无兼容迁移中删除，避免文件名继续暗示已经移除的 `wui::ui` 命名空间。

每个 Builder 只属于一个领域头；领域头仅依赖 `builder_base.h`、对应 Node 头和直接使用的值类型。安装清单、CMake file set、umbrella header 和 external-consumer 测试必须同时更新，禁止依赖未声明的传递 include。

## Additional Team-scale Rules

### Declarative API is an authoring syntax, not a virtual tree

Builder 创建并配置 retained node，然后交出所有权。Builder 不参与后续渲染，也不存在隐式全树 diff。文档和示例必须使用“声明式编写层”或“Builder”，避免让有 React、Flutter、SwiftUI 背景的开发者误以为页面函数会被持续重新执行。

### Composition diagnostics must be intentional

`children(...)` 应使用受约束的 `NodeLike` trait 接受右值 Builder、右值 `NodePtr` 或右值 `unique_ptr<DerivedNode>`。不支持的类型应触发一条面向使用者的 `static_assert`，而不是暴露多页模板实例化错误。

概念上的约束：

```cpp
static_assert(
    is_node_like_v<T>,
    "children() accepts an rvalue WhatsUI Builder or NodePtr; use std::move for a named value"
);
```

负向契约使用 CMake `try_compile` 或独立 compile-fail fixture 验证，不能把预期无法编译的代码放入普通单元测试目标。

### Accessibility validation belongs near construction

必须通过节点挂载验证器和测试辅助工具验证明显错误，例如：

- icon-only Button 缺少 `accessibleLabel`。
- 同一窗口出现重复 `automationId`。
- 可交互自定义 Node 没有角色或可访问名称。
- 输入控件没有可推导标签。

Debug 构建应给出包含 `debugName` 和节点类型的诊断；Release 构建不得因此崩溃。

### Callback lifetime must be explicit

页面和组件不得默认假设捕获引用永远覆盖节点寿命。文档示例优先接收值语义 Callback、捕获共享 State 句柄、稳定控制器或有明确页面所有权的对象。若捕获引用，函数契约必须写明所有者寿命覆盖返回节点。后台结果必须通过 `State::post` 或 `UiContext::post` 回到所属 UI Context。

### Diagnostics must be observable without changing tree behavior

增加 UI Context 级诊断通道，用于重复 automationId、无效 key、缺少无障碍名称等不能安全地从异步结构更新中抛出的错误：

```cpp
enum class UiDiagnosticCode {
    DuplicateAutomationId,
    InvalidNodeKey,
    MissingAccessibleName,
};

struct UiDiagnostic {
    UiDiagnosticCode code;
    std::string message;
    std::string debugName;
};

app.onDiagnostic([](const UiDiagnostic& diagnostic) {
    // 测试可捕获；默认实现写入平台诊断日志。
});
```

诊断 Handler 在所属 UI 线程执行且不得抛出异常。诊断不用于普通业务错误，也不替代会修改数据的验证返回值。

## Developer Workflows

### Page author

只使用无后缀 Builder、State、Binding 和样式类型，不接触 `*Node`。

### Reusable component author

通过 Props、Callbacks 和 `NodePtr` 组合 Builder。只有需要命令式焦点、平台句柄或自定义绘制时才调用 `.node()`。

### Control author

实现 `*Node` 的布局、绘制、事件和无障碍语义，再提供一个无后缀 Builder。节点 API 使用 `setFoo/foo`，Builder 使用 `.foo(value)`。

### Test author

行为测试通过公开 Builder 构建 UI；节点单元测试显式使用 `ButtonNode` 等运行时类型；视觉测试验证本次 API 重构没有改变像素输出。

## Rejected Alternatives

### 保留 `wui::ui`，只给节点增加后缀

可以消除双 `using namespace` 的同名歧义，但页面仍需在 `wui::ui` Builder 与 `wui` 样式/State 之间切换。拒绝作为最终 API。

### 将节点移动到 `wui::nodes`

同样可以解决冲突，但给自定义控件、节点测试和 Builder 实现增加额外命名空间层级。`Node` 后缀已经足以表达类型角色。

### 保留旧节点名称作为根命名空间别名

会重新引入 `Button` 等歧义。项目尚无稳定 API 使用者，没有保留价值。

### 使用小写工厂函数 `button()` 规避类型名冲突

能够避开 C++ 名称冲突，但会放弃已经形成的 `Button()/Text()/Column()` 声明式风格，并没有解决运行时节点名称不自解释的问题。

### 完全隐藏运行时节点

会阻断自定义绘制、节点级测试、平台集成和高级控件开发。节点应公开，但必须通过 `Node` 后缀明确标识。

## Implementation Record

以下阶段全部完成；细节与验证矩阵见
`DECLARATIVE_API_OPTIMIZATION_ROADMAP.md`。

### Phase 1: API contract tests

- 增加只包含公开头文件的 external-consumer 编译测试。
- 建立 `DECLARATIVE_API_INVENTORY.md`，逐项记录 Builder、Node、leaf/container 分类、capability、公开头文件和迁移来源；编译测试对 inventory 中的映射执行 `static_assert`。
- 锁定 `wui::Button` 是 Builder、`wui::ButtonNode` 继承 `Node`。
- 锁定叶子控件没有 `children()`。
- 锁定 Builder 可条件配置，`build()` 只能消费右值。
- 锁定 `children()` 对错误参数给出明确诊断。
- 锁定临时 Builder 不能调用 `.node()`，empty Builder 的失败行为在 Debug/Release 一致。
- 锁定 `setContent()`、结构工厂与 `children()` 使用相同消费契约。

### Phase 2: Runtime node rename

- 按控件领域将所有具体 Node 类型增加 `Node` 后缀。
- 同步修改构造函数、析构函数、方法限定名、继承关系、forward declaration、`dynamic_cast`、测试和平台桥接。
- 将当前具体 `wui::Container` 重命名为 `wui::BoxNode`；当前 `wui::Box` 在 Phase 3 迁为 `wui::Box`，不存在要保留的声明式 `Container`。
- 将当前单行 `wui::TextInput` 原地重命名为 `wui::TextFieldNode`，不拆分或改变编辑行为；当前 `wui::TextField` 在 Phase 3 迁为 `wui::TextField`。
- 将当前 `wui::TextArea` 原地重命名为 `wui::TextAreaNode` 并继续继承 `TextFieldNode`；`SearchFieldNode` 与 `ComboboxNode` 同步改为继承 `TextFieldNode`。这些都是类型重命名，不新增第二套输入实现。
- 不添加旧名称别名。
- 每完成一个领域即编译其单元测试，避免一次性机械替换掩盖错误。

### Phase 3: Declarative namespace and Builder semantics

- 将 Builder 从 `wui::ui` 移入 `wui`。
- 拆分 `BuilderBase` 与 `ContainerBuilderBase`。
- 为所有公开 fluent modifier（不含构造函数、`.node()`、`empty()` 和 `build()`）增加左值/右值重载，并建立 API 清单检查遗漏。
- 删除隐式节点转换和指针操作符，增加 `.node()` 与 `NodePtr`。
- 删除重复兼容属性与枚举。

### Phase 4: Identity, composition and documentation

- 拆分 `key`、`automationId`、`accessibleLabel`、`debugName`。
- 增加 UI Context 级诊断通道，并让无效 key、重复 automationId 和无障碍构建检查使用同一结构化诊断类型。
- 将每个旧 `accessibilityId` 调用点按真实语义迁移，并保存 UIA/无障碍等价基线。
- 补充 Props/Callbacks 组件示例和自定义 Node 指南。
- 更新 ADR-005、控件文档和全部示例。
- 拆分声明式头文件，新增 `wui/declarative.h`，删除 `wui/ui.h`，同时保留 `wui/wui.h` 聚合入口。

### Phase 5: Verification

- Debug 与 Release 全量构建。
- 运行全部单元、交互、生命周期、UI Dispatcher 和外部消费者测试。
- 运行 Software/OpenGL 视觉测试与 100/125/150/200% DPI 基线。
- 对番茄钟的新建任务、Dialog、Overlay、键盘焦点、关闭窗口和后台 State 投递执行回归。
- 对 API 重构前后的参考截图做像素比较；命名重构不得产生视觉差异。
- 对事件路由、焦点顺序、UIA snapshot、RuntimeId、可访问名称、automationId 和键盘操作做重构前后等价比较。
- 验证回调在页面销毁、节点移除和后台任务完成后的生命周期安全。
- 验证安装后的领域头、`wui/declarative.h` 和 `wui/wui.h` 均可由干净的外部消费者独立编译。

## Acceptance Criteria

- `using namespace wui;` 后可以无歧义地使用 `Button`、`ButtonAppearance`、`State` 和 `Column`。
- `wui::Button` 是 move-only Builder，`wui::ButtonNode` 是运行时节点。
- WhatsUI 仓库内所有具名具体 Node 类型符合 `*Node` 命名规则。
- 叶子 Builder 不公开 `children()`。
- 条件配置不需要绕过类型系统或直接操作 Node。
- Builder 没有隐式 `unique_ptr` 转换和指针操作符。
- `.node()` 不能在临时 Builder 上调用；empty Builder 的错误行为有 Debug/Release 测试。
- 成功 `build()` 后源 Builder 为空，二次消费失败；move construction、move assignment、self-move、从 empty 移动和 `const node()` 均有契约测试。
- `children()` 明确拒绝左值和空节点，并对父节点提供已定义的异常安全保证。
- 重复调用 `children()` 追加子节点；右值 `unique_ptr<DerivedNode>`、批量挂接失败、`setContent()` 和结构工厂遵循同一消费规则。
- 页面级代码默认不出现 `*Node`；节点和平台测试可以显式使用它。
- 不存在为未发布旧 API 添加的根命名空间兼容别名。
- `KeyedForEach` 只通过 key provider 接受 `NodeKey`；空 key、重复 key 和后续无效更新的行为有测试。
- `automationId`、`accessibleLabel`、`debugName` 不互相回退，旧 `accessibilityId` 已逐点迁移到一个或多个明确属性。
- UIA snapshot、RuntimeId、可访问名称、焦点、事件与键盘行为通过重构前后等价测试。
- 组件示例说明 Callback/引用捕获的生命周期，页面销毁后的回调与后台投递测试通过。
- UI 诊断通道可在测试中捕获无效 key、重复 automationId 和缺失可访问名称，并保证 Handler 在所属 UI 线程执行。
- `wui/declarative.h`、领域头和安装后的 external-consumer 编译测试通过，不依赖偶然的传递 include。
- `DECLARATIVE_API_INVENTORY.md` 覆盖全部 Builder/Node 映射、capability 和 fluent modifier；新增控件缺少 inventory 条目时契约测试失败。
- 完整测试和视觉门禁通过，运行时与像素输出没有因重命名发生变化。

## Consequences

- 页面作者获得单一、无歧义、可发现的 API。
- 节点作者和测试作者能从名称直接识别 retained tree 类型。
- 大量类型重命名会造成一次性的仓库级编译修改，但当前尚未承诺稳定 API，成本最低。
- Builder API 的约束更强，部分当前能够编译但语义错误的写法将被有意拒绝。
- 运行时树、所有权和渲染模型保持不变，因此可以用现有行为与视觉基线验证重构。
