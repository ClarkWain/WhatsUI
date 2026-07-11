# WhatsUI TextInput And IME

状态：Implemented baseline v0.2

## 1. 目标

这份文档用于明确 WhatsUI 中文本输入相关的边界，尤其是：

- 键盘事件与文本输入事件的分流
- 输入法会话模型
- 光标、选区、组合输入的职责划分
- 平台层与控件层的协作方式

TextInput 往往是轻量 UI 框架里最容易失控的一部分，所以这部分必须单独建模，而不能事后补丁式拼上去。

## 2. 设计原则

- 键盘事件与文本输入事件分离。
- IME 会话是平台能力，不是普通按键处理的副产物。
- 文本输入状态属于控件或编辑模型，不属于全局输入系统。
- 焦点、光标矩形、选区更新应由窗口级输入上下文统一协调。
- 早期先把契约定清楚，再逐步扩实现。

## 3. 关键概念

建议区分以下几个概念：

- `KeyEvent`：物理键盘动作，例如 Enter、Backspace、ArrowLeft。
- `TextInputEvent`：文本提交或组合输入结果。
- `Composition`：尚未最终提交的组合输入文本。
- `Caret`：插入点位置。
- `Selection`：文本选择范围。
- `SurroundingText`：供平台 IME 参考的上下文文本。

如果这些概念混在一起，后面会很难同时支持：

- 中文/日文/韩文输入法
- dead key
- emoji 选择输入
- 组合输入中的候选词与未提交文本

## 4. 输入事件分流

建议 WhatsUI 明确维护两条输入流：

- 键盘控制流：方向键、删除、确认、撤销、快捷键
- 文本输入流：commit text、composition update、composition end

这意味着：

- `Backspace`、`ArrowLeft`、`Ctrl+C` 等优先走 `KeyEvent`
- 实际插入到文本中的字符走 `TextInputEvent`
- 组合中的临时文本也应通过 `TextInputEvent` 或 composition 相关事件表达

不要尝试仅靠键盘事件拼出所有文本输入结果。

## 5. 文本输入会话

建议平台层独立出 `ITextInputSession`：

- `activate()`
- `deactivate()`
- `setCaretRect()`
- `setSurroundingText()`

语义建议：

- `TextInput` 获取焦点时激活会话。
- `TextInput` 失焦或窗口失活时停用会话。
- 控件每次光标或选区变化后更新 caret rect 和 surrounding text。
- 平台通过统一事件把 composition 和 commit 结果送回 UI runtime。

## 6. TextInput 控件职责

`TextInput` 控件自身或其内部编辑模型应负责：

- 当前文本内容
- 当前光标位置
- 当前选区范围
- 当前组合输入范围
- 插入、删除、替换文本的规则
- 滚动到光标可见区域

不建议把这些状态散落到平台层或全局输入系统中。

## 7. 组合输入模型

组合输入至少要支持以下阶段：

1. `CompositionStart`
2. `CompositionUpdate`
3. `CompositionCommit`
4. `CompositionEnd`

建议语义：

- `CompositionStart` 表示进入组合输入态。
- `CompositionUpdate` 更新候选中的临时文本和范围。
- `CompositionCommit` 表示确认一段文本，应写入最终内容。
- `CompositionEnd` 表示当前组合流程结束。

框架不应把组合输入文本当成已提交文本直接覆盖正文。

## 8. 光标与选区

光标和选区建议由文本编辑模型统一维护。

至少要有：

- 当前 caret 索引
- 当前 selection start/end
- 将文本索引映射到像素坐标的能力
- 将点击位置映射回文本索引的能力

平台层只需要知道：

- 当前 caret rect 在窗口中的位置
- 当前 surrounding text 与选区范围

也就是说，平台层不直接理解文档模型，只接收必要输出。

## 9. 焦点与窗口语义

IME 会话是窗口级的，不是应用全局级的。

建议规则：

- 一个时刻只有当前激活窗口里的一个文本输入目标拥有活跃会话。
- 窗口失焦时，当前会话应暂停或停用。
- 焦点切换到非文本控件时，文本会话应被正确收起。

这与此前的多窗口架构保持一致：

- 焦点域是窗口级
- 输入法会话域也是窗口级

## 10. 平台接口与运行时边界

平台层负责：

- 打开和关闭输入法会话
- 接收原生 composition/commit 事件
- 根据 caret rect 安放候选窗

运行时负责：

- 把当前焦点控件映射到活跃文本输入目标
- 更新 selection / caret / composition state
- 请求局部重绘
- 保证光标可见

页面作者不应直接与 IME 平台接口交互。

## 11. 建议的最小事件集合

建议至少定义以下文本输入相关事件：

- `TextCommitEvent`
- `CompositionStartEvent`
- `CompositionUpdateEvent`
- `CompositionEndEvent`
- `SelectionChangedEvent`

以及以下键盘相关事件：

- `KeyDownEvent`
- `KeyUpEvent`
- `ShortcutEvent`

这样文本输入与控制命令的语义保持干净分离。

## 12. 早期实现边界

建议初期不要同时追求所有高级编辑能力。较稳妥的实现顺序是：

1. 单行文本输入
2. 基本光标移动与删除
3. 基本选区
4. 基本 IME commit/composition 支持
5. 多行输入与滚动
6. 更丰富的快捷键和编辑命令

这不是因为这些能力不重要，而是因为 TextInput 的复杂度非常容易淹没整个框架实现节奏。

## 13. 非目标

当前这套文档明确不承诺：

- 完整富文本编辑器语义
- 浏览器级内容编辑能力
- 所有平台高级输入法特性的第一版全覆盖
- 文本输入和渲染完全解耦成独立编辑框架

目标是先把一个可用、可跨平台、契约稳定的 TextInput 架构定下来。

## 14. 当前基线实现

- `UiWindow` 将当前焦点中的 `TextInput` 映射为唯一活跃的窗口级
  `TextInputSession`：获得焦点即 `activate`，切页、浮层变化、换根或失焦即
  `deactivate`。
- 每次指针、键盘、文本提交或组合输入后，窗口同步 caret 矩形和 surrounding
  text/selection 至平台会话；坐标使用窗口逻辑像素。
- `CompositionInputEvent::Phase` 明确 Start/Update/End。空的 pre-edit 更新
  合法；只有 End 清除组合范围。`TextInputEvent` 始终表示已提交文本。
- 当前 caret 和 selection 索引以 UTF-8 字节偏移表示。平台接入时必须采用同一
  单位；Unicode grapheme 移动、鼠标按字形命中、多行滚动、剪贴板快捷键和原生
  IME composition 回调仍是后续工作。
