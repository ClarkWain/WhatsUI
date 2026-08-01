# Declarative API Inventory

状态：Phase 1 implemented

本文是 ADR-006 的实现清单。公开声明式类型位于 `wui`，运行时 retained-tree 类型统一使用 `*Node` 后缀。完整聚合入口是 `wui/declarative.h`。

## 通用契约

- 所有 Builder 都是 move-only，并通过 `node_type` 声明对应运行时类型。
- 通用 modifier：`flex()`、`accessibilityId()`；两者都提供 `&` 与 `&&` 重载。
- 观察入口：`empty()`、仅左值可用的 `node()`。
- 所有权出口：仅右值可用的 `build() &&`；具名 Builder 必须写 `std::move(builder).build()`。
- `AnyChildren` Builder 才有 `children()`；重复调用按顺序追加，空节点被拒绝，失败批次不会部分修改父节点。
- `SingleContent` Builder 使用 `content()`，重复调用替换原内容，运行时节点也验证最多一个 child。
- `Dialog::build()` 返回 `std::unique_ptr<DialogNode>`，其它 Builder 返回 `NodePtr`。
- `ButtonVariant`、`variant()`、`setVariant()` 已删除，只保留 `ButtonAppearance`。

## Builder / Node 映射

### 基础与布局

| Builder | Node | 能力 |
| --- | --- | --- |
| `Text` | `TextNode` | leaf |
| `Icon` | `IconNode` | leaf |
| `Image` | `ImageNode` | leaf |
| `Spacer` | `SpacerNode` | leaf |
| `Box` | `BoxNode` | children |
| `Row` | `RowNode` | children |
| `Column` | `ColumnNode` | children |
| `ScrollView` | `ScrollViewNode` | `content()` 单内容 |

### 表单与基础控件

| Builder | Node | 能力 |
| --- | --- | --- |
| `TextField` | `TextFieldNode` | leaf |
| `TextArea` | `TextAreaNode` | leaf |
| `SearchField` | `SearchFieldNode` | leaf |
| `Label` | `LabelNode` | leaf |
| `Field` | `FieldNode` | `control()` 专用组合 |
| `MessageBar` | `MessageBarNode` | leaf |
| `Button` | `ButtonNode` | leaf |
| `IconButton` | `IconButtonNode` | leaf |
| `MenuButton` | `MenuButtonNode` | leaf |
| `SplitButton` | `SplitButtonNode` | leaf |
| `CompoundButton` | `CompoundButtonNode` | leaf |
| `ToggleButton` | `ToggleButtonNode` | leaf |
| `Checkbox` | `CheckboxNode` | leaf |
| `Radio` | `RadioNode` | leaf |
| `RadioGroup` | `RadioGroupNode` | children |
| `Switch` | `SwitchNode` | leaf |
| `Slider` | `SliderNode` | leaf |

### 内容、反馈与身份

| Builder | Node | 能力 |
| --- | --- | --- |
| `Card` | `CardNode` | children |
| `CardHeader` | `CardHeaderNode` | children |
| `CardPreview` | `CardPreviewNode` | children |
| `CardFooter` | `CardFooterNode` | children |
| `ProgressBar` | `ProgressBarNode` | leaf |
| `Toast` | `ToastNode` | leaf |
| `Spinner` | `SpinnerNode` | leaf |
| `Divider` | `DividerNode` | leaf |
| `Badge` | `BadgeNode` | leaf |
| `CounterBadge` | `CounterBadgeNode` | leaf |
| `PresenceBadge` | `PresenceBadgeNode` | leaf |
| `Avatar` | `AvatarNode` | leaf |
| `AvatarGroup` | `AvatarGroupNode` | children |
| `Persona` | `PersonaNode` | leaf |

### 数据、日期与集合

| Builder | Node | 能力 |
| --- | --- | --- |
| `Calendar` | `CalendarNode` | leaf |
| `DatePicker` | `DatePickerNode` | leaf |
| `TimePicker` | `TimePickerNode` | leaf |
| `Table` | `TableNode` | 专用行模型 |
| `DataGrid` | `DataGridNode` | 专用行列模型 |
| `Tree` | `TreeNode` | 专用 item 模型 |
| `ListView` | `ListViewNode` | 专用 item 模型 |
| `ListBox` | `ListBoxNode` | 专用 option 模型 |
| `Combobox` | `ComboboxNode` | 专用 option 模型 |
| `Dropdown` | `DropdownNode` | 专用 option 模型 |
| `Rating` | `RatingNode` | leaf |
| `RatingDisplay` | `RatingDisplayNode` | leaf |

### 导航、浮层与结构

| Builder | Node | 能力 |
| --- | --- | --- |
| `AccordionItem` | `AccordionItemNode` | `content()` 专用组合 |
| `Accordion` | `AccordionNode` | children |
| `Drawer` | `DrawerNode` | `content()` 专用组合 |
| `Popover` | `PopoverNode` | leaf |
| `PopoverButton` | `PopoverButtonNode` | leaf |
| `TeachingPopover` | `TeachingPopoverNode` | leaf |
| `Toolbar` | `ToolbarNode` | children |
| `TabList` | `TabListNode` | children |
| `TabPanel` | `TabPanelNode` | children |
| `Link` | `LinkNode` | leaf |
| `Breadcrumb` | `BreadcrumbNode` | children |
| `Dialog` | `DialogNode` | `content()` 专用组合 |
| `If` | `IfNode` | `then()` 结构工厂 |
| `ForEach<T>` | `ForEachNode` | 非 keyed 结构集合 |
| `KeyedForEach<T>` | `ForEachNode` | keyed 结构集合 |

## 自动门禁

`declarative_api_contract_tests.cpp` 编译期覆盖全部 66 个映射及其 `children()`/`content()` capability，并验证左值/右值 modifier、`build()`、`node()` 和 move-only 约束。运行期覆盖 empty Builder、二次 build、move/self-move、原始 `unique_ptr<DerivedNode>`、重复 children、单内容替换和批量失败的父节点强保证。

仍待后续阶段加入自动门禁的内容：逐 modifier 清单比对、`automationId`/`accessibleLabel`/`debugName` 身份拆分，以及声明式领域头拆分后的 external-consumer 独立 include 测试。
