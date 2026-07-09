# WhatsUI Reactivity, Structure And Text Measurement

状态：Draft v0.1

本文记录构建器层之上的三块能力：响应式绑定、结构控件、可插拔文本测量，以及配套的新控件。实现见 `include/wui/ui.h`、`include/wui/structural.h`、`include/wui/text_metrics.h`。

## 1. 响应式绑定

- `State<T>` 变化 → 通知订阅者。控件通过 `Node::addTeardown(...)` 注册"销毁时回调"，在节点析构时**自动 unsubscribe**，因此**即使 State 比节点活得久也安全**。
- `Text().bind(state, format)`：初次渲染 `format(state.get())`，之后每次 `state` 变化都 `setValue(format(...))` 并标脏。另有 `Text().bind(stateString)` 便捷重载（`State<std::string>`）。
- 生命周期约定：绑定持有 `State&`；请保证 **State 的生命周期覆盖被绑定的节点**（典型：State 属于页面/应用对象，节点属于其子树）。节点先亡是安全的（teardown 会退订）；State 先亡属于未定义用法。

```cpp
wui::State<int> count{0};
auto label = Text().bind(count, [](const int& c) { return "Count: " + std::to_string(c); });
```

## 2. 结构控件（If / ForEach）

动态结构由**显式结构控件**托管，把变化限制在局部子树，不做全树 diff（见 ADR-002）。

- `If(state).then(factory)`：`state` 为真时挂载 `factory()` 生成的子树，为假时卸载。
- `ForEach<T>(items, itemBuilder)`：按 `State<std::vector<T>>` 生成一列子节点，列表变化时重建（当前为整体重建，keyed 复用为后续优化）。

```cpp
wui::State<bool> showAdvanced{false};
auto panel = Column().children(
    If(showAdvanced).then([]{ return Text("Advanced settings"); })
);

wui::State<std::vector<std::string>> items{{"a", "b"}};
auto list = ForEach<std::string>(items, [](const std::string& s){ return Text(s); }).gap(4);
```

对应节点：`wui::IfNode`（挂载/卸载单个子树）、`wui::ForEachNode`（复用 `Column` 的纵向布局）。

### 2.1 延迟失效（帧调度）

结构变化（If 挂载/卸载、ForEach 重建）**不在** `State` 通知里同步执行，而是通过调度器排队、在**帧边界统一 flush**（架构 §8 帧管线 / §11.7 更新规则）：

- `wui::scheduleStructuralUpdate(key, action)`：按 `key`（通常是节点指针）**合并**排队。
- `wui::flushStructuralUpdates()`：每帧 layout 前调用一次，执行并清空队列。

这样做的关键收益：**事件处理器可以安全地修改 `State`**。若结构重建同步发生在按钮自己的 `onClick` 里，会立刻销毁正在执行的那个按钮（自毁）；延迟到 flush 后，处理器先安全返回，再统一重建。宿主的帧循环应为：`flushStructuralUpdates → layout → paint → present`。

（`Text().bind` 的文本更新是纯属性写入，不改树，仍同步生效。）


## 3. 可插拔文本测量

文本布局不再依赖硬编码字符启发式，而是走 `TextMeasurer` 接口：

- `wui::setTextMeasurer(TextMeasurer*)` / `wui::textMeasurer()`：进程级、非拥有。未安装时 `Text::measure` 回退到启发式。
- `WhatsCanvasTextMeasurer`（`include/wui/whatscanvas_text.h`，仅在 `WHATSUI_WITH_WHATSCANVAS=ON` 生效）：用 `wsc::Canvas::measureTextMetrics` + 带字号的 `wsc::Paint` 拿**真实整形宽度与行高**。

```cpp
#ifdef WHATSUI_HAS_WHATSCANVAS
wui::WhatsCanvasTextMeasurer measurer(canvas);
wui::setTextMeasurer(&measurer);   // 之后所有 Text 用真实文本度量
#endif
```

宿主在拿到测量用 Canvas 后安装一次即可；核心库与响应式/结构层都不感知渲染后端。

## 4. 新增控件

- `Box`（包装 `wui::Container`）：单子节点容器。
- `Spacer(width, height)`（`wui::Spacer`）：定尺寸空节点，用于显式留白。
- `TextField(placeholder)`（包装 `wui::TextInput`）：文本输入，支持 IME（见 `TEXT_INPUT_AND_IME.md`）。

## 验证

`tests/whatsui_smoke_tests.cpp` 覆盖：响应式 Text 更新、If 挂载/卸载、ForEach 重建、`Text::measure` 走已装测量器与回退。默认（无 WhatsCanvas）与 `WHATSUI_WITH_WHATSCANVAS=ON` 两种构建均通过。
