# WhatsUI Layout Model

状态：Draft v0.1

## 1. 目标

WhatsUI 的布局模型需要满足以下目标：

- 对页面作者足够简单，可预测。
- 对框架实现足够稳定，便于局部更新。
- 与 C++ 强类型 API 自然契合。
- 能覆盖工具型桌面 UI 的大多数常见布局。
- 不把系统推向 CSS 级规则复杂度。

## 2. 核心原则

- 使用约束布局：`Constraints -> Size -> Position`。
- 布局使用逻辑单位，不直接暴露物理像素。
- 子节点不主动决定最终坐标，坐标由父容器统一分配。
- 滚动是容器能力，不是全局布局模式。
- 动态结构变化应局限在局部容器，不影响无关子树。

## 3. 基本数据类型

建议布局系统至少围绕这些基础类型构建：

- `Size`
- `Point`
- `Rect`
- `Insets`
- `Constraints`
- `Alignment`

其中 `Constraints` 建议至少包含：

- `minWidth`
- `maxWidth`
- `minHeight`
- `maxHeight`

语义要求：

- `min <= max`
- 无穷上界只允许出现在容器内部计算阶段，不建议暴露给页面作者
- 约束不承载视觉语义，只承载可用空间边界

## 4. 布局契约

每个参与布局的节点建议遵守两步契约：

1. `measure(const Constraints&) -> Size`
2. `layout(const Rect&)`

语义如下：

- `measure()` 只决定“我想占多大”。
- `layout()` 决定“我最后被放在哪儿”。
- 父节点先测量子节点，再统一分配位置。
- 节点不应在 `measure()` 阶段依赖最终坐标。

这样做的好处是：

- 布局算法更容易推理。
- 局部重新布局更容易实现。
- 后续要做滚动、虚拟化、对齐策略时，不需要推翻基本契约。

## 5. 逻辑单位与 DPI

WhatsUI 应明确区分：

- `logicalSize`
- `framebufferSize`
- `scaleFactor`

布局系统只消费逻辑单位：

- 所有间距、宽高、字体相关布局度量都按逻辑单位处理。
- 平台层负责把窗口与 framebuffer 的差异告诉运行时。
- `PaintContext` 在进入绘制前处理好逻辑到物理像素的映射。

这样可以避免：

- 高 DPI 下布局与渲染纠缠
- 页面作者到处处理像素倍率
- 同一组件在不同平台下出现尺寸语义漂移

## 6. 基础容器语义

### 6.1 Row

`Row` 负责沿主轴横向排列子节点。

建议支持：

- `gap`
- 主轴对齐
- 交叉轴对齐
- 可选 `flex` 分配

建议规则：

- 非 flex 子节点先测量。
- 剩余空间再按 `flex` 权重分配给 flex 子节点。
- 交叉轴尺寸默认取子节点最大值，或由父约束进一步裁定。

### 6.2 Column

`Column` 与 `Row` 语义对称，只是主轴变为纵向。

建议同样支持：

- `gap`
- 主轴对齐
- 交叉轴对齐
- `flex`

`Column` 会是表单、设置页、面板型 UI 的最常用容器，应优先保证其行为简单稳定。

### 6.3 Stack

`Stack` 负责在同一布局区域内叠放多个子节点。

建议支持：

- 默认铺满父区域
- 单子节点对齐策略
- 局部绝对定位接口

`Stack` 适合：

- overlay within page
- badge / icon overlay
- loading mask
- corner action

### 6.4 Padding

`Padding` 是纯布局修饰节点，只负责在 child 外加 `Insets`。

建议规则：

- `measure()` 时先从父约束扣除 padding，再测量 child
- 返回尺寸时再把 padding 加回去
- `layout()` 时 child 的矩形在自身边界内收缩后分配

### 6.5 Align

`Align` 用于在已有分配区域内调整 child 的停靠方式。

建议支持：

- `Start`
- `Center`
- `End`
- `Stretch`

`Align` 的职责是位置分配，不负责重写 child 自身测量逻辑。

### 6.6 ScrollView

`ScrollView` 是容器能力，不是另一套布局世界。

建议语义：

- viewport 自己服从父约束
- content 在滚动方向上允许超过 viewport
- 非滚动方向上尽量沿用普通容器规则
- 滚动偏移只影响可视窗口与命中区域，不改写子树的固有测量语义

## 7. Flex 语义建议

如果 WhatsUI 早期需要 `flex` 能力，建议只支持最小可用子集：

- `flexGrow`
- `flexShrink`
- `flexBasis`

但不建议早期复制完整 Web Flexbox 语义。比较稳妥的做法是：

- 先支持剩余空间分配
- 先不支持所有复杂换行和基线规则
- 先保证 `Row`/`Column` 在工具型 UI 里足够好用

## 8. Intrinsic Measurement 边界

Intrinsic measurement 很容易把布局系统复杂度迅速拉高。

建议策略：

- 只给确实需要的节点提供 intrinsic size 能力，例如文本、图片、基础按钮。
- 不把任意节点的多向 intrinsic contract 作为早期必做能力。
- 页面作者不依赖它解决大多数布局问题。

这能防止布局系统过早变成一个重型求解器。

## 9. 可见性与折叠

建议尽早区分两类行为：

- `visible = false`：仍占布局空间，但不绘制也不响应事件。
- `collapsed`：不占布局空间，通常应通过 `If` 或 `Switch` 这类结构控件表达。

不要把“隐藏但占位”和“彻底移出结构”混成一个属性，不然后面事件和布局语义会混乱。

## 10. 布局失效规则

建议这些变化触发 `NeedsLayout`：

- width / height 变化
- min / max 约束变化
- padding / gap / align 变化
- font size 或文本换行边界变化
- child 数量变化
- `If` / `Switch` / `ForEach` 带来的结构变化

建议这些变化只触发 `NeedsPaint`：

- 背景色变化
- 文本颜色变化
- 阴影颜色变化
- 边框颜色变化

这条分界线越清楚，后续布局性能越稳定。

## 11. 页面作者应如何使用布局系统

页面作者的理想写法应接近：

```cpp
auto page = ui::Column()
    .padding(16)
    .gap(12)
    .child(header())
    .child(
        ui::Row()
            .gap(8)
            .child(sidebar())
            .child(
                ui::Container()
                    .flexGrow(1)
                    .child(content())
            )
    );
```

页面作者思考的是：

- 结构是横排还是竖排
- 哪一块跟随剩余空间伸展
- 哪一块需要滚动
- 哪一块只是局部对齐或补 padding

而不是去直接解一套复杂布局方程。

## 12. 非目标

当前布局模型明确不追求：

- 完整 CSS Flexbox / Grid 兼容
- 浏览器级 baseline 细节
- 通用自动布局求解器
- 任意规则混排后的隐式回退行为

这份文档的目标，是给 WhatsUI 一个足够稳、足够清晰、足够好实现的布局内核。
# Text wrapping and truncation

`Text` measures and paints the same resolved line list. By default it preserves
each explicit newline and otherwise remains on one line. `TextWrap::Word` uses
the available finite maximum width to wrap at ASCII spaces/tabs; an unbroken
word (including CJK text) is split only at UTF-8 codepoint boundaries. A zero
`maxLines` means unlimited. When a finite non-zero limit removes lines, or a
single unwrapped line exceeds its available width, `TextOverflow::Ellipsis`
replaces the final visible suffix with a fitted `...`; `Clip` leaves the text
unchanged for the backend clip to handle. The measured width is the widest
resolved line, and height is `lineHeight × lineCount`, both clamped by incoming
constraints. Paint emits one baseline per resolved line using that same line
height, so measurement and drawing cannot diverge.
