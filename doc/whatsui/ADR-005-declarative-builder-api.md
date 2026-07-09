# ADR-005: Declarative Builder Authoring API

状态：Accepted Draft

## Context

页面作者需要一种"声明式"的 UI 编写手感：结构一眼可读、配置贴着控件类型、子节点列在最后。ADR-002 已确定运行时是**单树保留 + `unique_ptr` 父子所有权**，UI 编写面走**强类型 C++ 组合 API**（不引入 XML/QML/CSS）。

在 C++17 下把"声明式手感"落到实处时，有两个硬约束：

1. **没有指派初始化**（`.padding = 16` 是 C++20），只能用链式 setter。
2. **`std::initializer_list` 的元素是 const，无法搬移 move-only 值**。因此 `Column({ std::make_unique<Text>(...) })` 这类"花括号子节点"无法把 `unique_ptr` 搬进树里。

同时，早期的节点直接暴露 `.child(std::move(...))` 逐个链式：子节点一多，`.child(...)` 会把 `.gap()` / `.padding()` 这类配置推得离 `Column()` 很远，可读性差。

## 决策

新增一层**极薄、header-only、move-only 的构建器层**，命名空间 `wui::ui`，包装现有 `wui::` 节点，不改变运行时所有权模型。

要点：

- **保留 `unique_ptr` 单一所有权**。构建器内部持 `std::unique_ptr<NodeT>`，被父节点或 `setContent` 消费时一次性交出所有权（`operator std::unique_ptr<wui::Node>()`）。
- **CRTP** 复用公共链式方法并保持返回具体类型，链上仍可见子类特有方法（如 `Column::gap`）。
- **变参 `children(a, b, c)`（配合 C++17 折叠表达式）** 取代花括号：完美转发、天然吃 move-only，规避 `initializer_list` 搬移坑；配置在前、子节点在后。
- 页面作者**永远不写 `make_unique`、不碰 `unique_ptr`**，但底层所有权依然严格。

页面作者写法：

```cpp
using namespace wui::ui;

auto page =
    Column()
        .padding(16)
        .gap(8)
        .children(
            Text("Settings"),
            Row().gap(8).children(
                Text("Name:"),
                Button("Save").onClick([&] { save(); })
            ),
            Button("Close").onClick([&] { nav.pop(); })
        );

root.setContent(page); // 隐式交出所有权给树
```

### 被否决的方案：值句柄（`shared_ptr<Node>`）以支持花括号 `{...}`

花括号 `.children({ ... })` 在 C++17 里要求元素**可拷贝**，这意味着控件必须句柄化为 `shared_ptr<Node>`。否决原因：

- 保留树里一个节点**有且只有一个父**，天然是**单一所有权**语义；`shared_ptr` 表达的"共享所有权"与领域不符，还会放任"同一节点被塞进两个父"这类运行期错误，而 `unique_ptr` 在编译期就堵死它。
- 引用计数（每节点一个原子控制块）与"轻量"目标相悖。
- 需要把核心从 `unique_ptr` 换成 `shared_ptr`，是伤筋动骨的重构。

页面作者要的"配置在前、子节点在后"由链尾 `children(a, b, c)` 已经完全满足，**唯一让出的只是花括号**这一观感，不值得为它更换所有权模型。

## Consequences

- 声明式手感（配置贴着类型、子节点在链尾）达成，且保持 `unique_ptr` 单一所有权与轻量。
- 构建器层是纯增量、header-only，不改动节点运行时；旧的 `node->appendChild(...)` 命令式路径仍可用。
- `children(...)` 同时接受其它构建器与裸 `std::unique_ptr<wui::Node>`，便于与工厂/自定义节点混用。
- 代价：没有花括号字面量；`Text(...)`/`Button(...)` 等是 `wui::ui` 工厂类型而非节点本身。
- 后续 `State`→控件属性的响应式绑定、结构控件 `If/ForEach` 均可在此构建器形态上叠加，不需推翻 API。
