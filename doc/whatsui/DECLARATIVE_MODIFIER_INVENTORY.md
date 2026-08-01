# Declarative Modifier Inventory

状态：Implemented

本文记录公开 fluent modifier 的一致性规则。逐控件的 Node 映射与组合能力见
`DECLARATIVE_API_INVENTORY.md`；本文件按能力分组，避免复制数百个简单属性名后失去同步。

## 全局规则

每个会返回 Builder 的 modifier 必须同时提供：

```cpp
Self& value(Value value) &;
Self&& value(Value value) &&;
```

两者必须调用同一行为实现。简单 setter 可以直接各转发一次；包含订阅、转换、批量验证
或结构替换的 modifier 必须使用私有 `applyValue()`。`build()` 只有 `&&`；`node()` 只有
`&`/`const &`；这两个所有权入口不属于配对规则。

## 通用 modifier

| 能力 | modifier | 适用范围 |
| --- | --- | --- |
| 布局 | `flex()` | 所有 Builder |
| 自动化 | `automationId()` | 所有 Builder |
| 诊断 | `debugName()` | 所有 Builder |
| 无障碍名称 | `accessibleLabel()` | 支持名称语义的控件；由公开 mixin 提供 |

## 组合 modifier

| 能力 | modifier | 代表 Builder |
| --- | --- | --- |
| Any children | `children(...)` | Box、Row、Column、Card、CardPreview、CardFooter、TabPanel |
| Single content | `content(...)` | ScrollView、Dialog、Drawer、AccordionItem |
| Typed children | `children(T...)` | RadioGroup、Accordion、AvatarGroup |
| Item factory | `item(...)` / `tab(...)` | Toolbar、Breadcrumb、TabList |
| Named slots | `media(...)` / `action(...)` | CardHeader |
| Structural | `then(...)` | If |

## 带共享实现的复杂 modifier

- `Text::bind(State/Observable)`：订阅与 teardown 分别集中在内部 binding helper。
- `If::then(...)`：结构 factory 的消费和验证集中在 `applyThen()`。
- `content(...)`：由 `SingleContentBuilderBase` 统一替换语义与错误诊断。
- `children(...)`：NodeLike 转换先在局部完成，通过后才提交父节点，保证失败批次不部分挂接。

## 已锁定的历史缺口

以下 modifier 曾只提供单一值类别或有重复实现，现已配对并由编译契约覆盖其所属能力：

- `Accordion::item`
- `PopoverButton::popover`
- `TeachingPopover::primaryAction` / `secondaryAction`
- `Table::rowProvider` / `DataGrid::rowProvider`
- `ListView::itemProvider`
- `CardHeader::media` / `action`

新增或修改 Builder 时，评审必须检查：返回类型配对、异常安全、NodeLike 消费、空值行为、
线程所有权以及领域头能否独立编译。契约测试负责代表性表达式与全部 Builder/capability；
编译器/代码审查负责防止简单 setter 清单漂移。
