# Fluent 组件完成度追踪

状态：已完成。此清单是当前交付边界；全部条目已完成、自动测试通过并经截图审查。

## 验收规则

- 每个交互控件都必须覆盖 rest、hover、pressed、focused、disabled，以及适用的 selected/open 状态。
- 每个可编辑组件必须覆盖键盘、指针、焦点、无障碍动作与长内容。
- 每个视觉组件必须有 Software 截图基线；人工审查截图不能由测试通过替代。
- 所有状态必须使用 Fluent 语义 token，不得用固定颜色遮盖问题。

## A. TextArea

- [x] 复用 TextInput 的 UTF-8 编辑、选择、撤销、剪贴板和 IME 会话。
- [x] 多行换行、视觉行上下导航、固定最小行数和输入外观。
- [x] 内部纵向滚动、滚轮剩余 delta 向父 ScrollView 交接、长文本 caret 跟随。
- [x] 多行选择、composition underline、滚动后的 pointer 命中自动测试。
- [x] Software 截图：placeholder、长文本、selection、composition、focus、invalid、disabled。

## B. MenuButton / SplitButton

- [x] MenuButton 通过 OverlayHost 打开锚定 Menu，Esc/外部点击关闭。
- [x] SplitButton 区分主命令区与 disclosure 区；键盘主操作与展开操作分流。
- [x] 菜单打开后将焦点移至第一个可用 MenuItem；关闭后恢复到触发按钮。
- [x] Expand/Collapse 无障碍能力、expanded 状态和 UIA 属性映射。
- [x] 菜单打开、hover、pressed、disabled、键盘、焦点恢复 Software 截图与回归。

## C. Card 系列

- [x] Filled / FilledAlternative / Outline / Subtle、三档 padding、纵向/横向布局、selected 状态。
- [x] Header 的媒体、标题、描述、trailing action；Footer 左内容/右操作；Preview 固定高度。
- [x] Preview 真实圆角 path clip，不使用遮罩伪造。
- [x] Header 媒体/action 任意调用顺序和动态替换，不依赖调用顺序。
- [x] selectable Card 的 hover、pressed、focused、disabled、elevation 与无障碍 checked/selection 语义。
- [x] Card / Header / Preview / Footer 组合 Software 截图基线与像素审查。

## D. 既有基础控件视觉基线

- [x] Button / CompoundButton / ToggleButton：全状态、尺寸、shape、文本居中截图。
- [x] Checkbox / Radio / Switch：几何、状态叠加、对齐与裁剪截图。
- [x] TextField / SearchField / Label：baseline、focus indicator、caret blink、invalid、disabled 截图。
- [x] ProgressBar / Slider / Divider：极值、空值、禁用、不同 DPR 截图。
- [x] Todo 在窄屏、常规、宽屏上重新生成并人工审查所有受影响状态。

## E. 最终质量门禁

- [x] 基础控件、无障碍、文本 composition、Todo interaction 与视觉 review 全部通过。
- [x] 新增截图 artifact 已逐张审查，无裁切、重叠、错误 focus、错误像素对齐或状态混淆。
- [x] `git diff --check` 通过；未处理的编译警告仅限已记录的无关项。
