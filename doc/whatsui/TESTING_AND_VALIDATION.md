# WhatsUI Testing And Validation

状态：Draft v0.1

## 1. 目标

WhatsUI 的验证策略不能只停留在“能编过”或“界面能跑起来”。

目标应当包括：

- 保证布局结果稳定
- 保证输入与导航行为正确
- 保证视觉输出不出现无意回归
- 保证多窗口、多页面和浮层行为在重构后仍然可靠

## 2. 为什么 WhatsUI 有验证优势

WhatsUI 建立在 WhatsCanvas 之上，而 WhatsCanvas 已经有 Software backend 和像素回读能力。这意味着 WhatsUI 天然适合建立确定性测试，而不必把所有验证都压在人工目测上。

这会是 WhatsUI 相比很多轻量 UI 项目最值得放大的优势之一。

## 3. 测试分层

建议把验证拆成四层：

1. 单元测试
2. 布局快照测试
3. 行为回归测试
4. 视觉回归测试

每一层解决不同问题，不互相替代。

## 4. 单元测试

单元测试适合覆盖：

- `State<T>` / `Binding<T>` 语义
- `Navigator` 栈行为
- `OverlayHost` 增删语义
- `TextInputModel` 的 selection / commit / composition 行为
- `Constraints` 与基础布局辅助类型

单元测试的价值在于：

- 快
- 定位清晰
- 对重构非常敏感

## 5. 布局快照测试

布局快照测试不关心颜色和像素细节，只关心布局树的几何结果。

建议覆盖：

- `Row` / `Column`
- `Padding`
- `Align`
- `ScrollView` viewport / content 关系
- `If` / `Switch` / `ForEach` 触发的结构变化

输出形式可以是：

- 节点树矩形结构快照
- 简单 JSON / 文本树描述
- 关键节点 bounds 的断言集合

这层测试比像素回归更稳定，也更适合先期快速建立覆盖。

## 6. 行为回归测试

行为回归测试用于验证：

- 点击命中
- 焦点切换
- 页面 `push/pop/replace`
- dialog / menu / overlay 生命周期
- 多窗口焦点与激活关系
- TextInput 的键盘控制与 commit/composition 流程

这类测试的重点不是画面，而是：

- 事件顺序对不对
- 当前活跃对象对不对
- 状态变化后 dirty flags 是否落在预期层级

## 7. 视觉回归测试

视觉回归测试建议建立在 Software backend 之上，优先覆盖高价值场景：

- 标准按钮状态
- 文字与图标对齐
- 卡片圆角与阴影
- 裁剪与滚动容器
- 对话框与浮层遮罩
- 多主题下的典型页面

建议输出方式：

- PPM 或 RGBA dump
- 像素 hash
- 必要时 fuzzy compare

视觉回归不需要覆盖所有组件所有状态，但必须覆盖最容易被无意改坏的关键场景。

## 8. 推荐的验证场景

建议长期维护一组固定的 UI 场景：

- `layout-showcase`
- `widget-states`
- `navigation-stack`
- `overlay-showcase`
- `text-input-basic`
- `multi-window-basic`

这些场景既可以作为截图回归输入，也可以作为 smoke test 的稳定入口。

## 9. 多窗口与导航专项验证

多窗口和导航是最容易在重构中被忽略的区域，建议单独列专项：

- 一个窗口内 `Navigator` 的 root page 不能被错误弹空
- `OverlayHost` 的 dismiss 不应影响 page stack
- 窗口失焦时 TextInput 会话应正确停用
- 子窗口打开/关闭不应污染主窗口焦点域
- 页面保活策略 `KeepAlive` / `DisposeOnHide` 行为应可断言

## 10. 文本输入专项验证

TextInput 这条线建议拆成独立测试组：

- selection 变化
- replaceSelection 语义
- commit 后 caret 位置
- composition update / commit / clear 顺序
- caret rect 与 surrounding text 同步

这是因为文本输入的复杂度高，而且 bug 往往不是视觉上立即显现的。

## 11. 验证入口建议

当代码骨架继续成长后，建议至少保留这些验证入口：

- 快速单元测试
- 纯布局快照测试
- headless Software backend 视觉回归
- 最小 smoke 场景

不要把所有验证都压在一个巨大的“全量 UI demo”上，那会让失败定位变得很差。

## 12. CI 思路

从长期看，比较理想的 CI 分层是：

- PR 级：单元测试 + 布局快照 + 少量 smoke
- 主干级：扩大行为回归和视觉回归范围
- 发布前：全量回归场景 + 关键平台手工检查

这能平衡反馈速度和覆盖深度。

### Sanitizer gate

`WHATSUI_ENABLE_SANITIZERS=ON` is the reproducible memory-safety gate for the
headless runtime.  The option instruments both `WhatsUI` and targets that link
it, so test code and the runtime are checked together.

- Windows/MSVC runs AddressSanitizer. MSVC does not provide UBSan, so CI names
  this limitation explicitly rather than silently claiming equivalent coverage.
- Linux/Clang runs AddressSanitizer and UndefinedBehaviorSanitizer together.
- The sanitizer jobs intentionally leave `WHATSUI_WITH_WHATSCANVAS=OFF`:
  WhatsCanvas is an in-tree third-party integration with a separate validation
  contract and must not make the core runtime gate non-reproducible.

Run the commands in the root `README.md` locally before changing ownership,
deferred mutation, event dispatch, or teardown code.

## 13. 非目标

测试系统当前不追求：

- 一开始就做到全组件全状态像素基线
- 把所有行为都做成端到端黑盒 UI 自动化
- 为了测试系统本身引入比框架更复杂的外部依赖

先把最值钱、最容易回归的路径守住，比追求“测试面很大”更重要。
