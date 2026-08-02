# Declarative API Inventory

状态：Implemented

本文是 ADR-006 的实现清单。公开声明式类型位于 `wui`，运行时 retained-tree 类型统一使用 `*Node` 后缀。完整聚合入口是 `wui/declarative.h`。

## 通用契约

- 所有 Builder 都是 move-only，并通过 `node_type` 声明对应运行时类型。
- 通用 modifier：`flex()`、`automationId()`、`debugName()`；均提供 `&` 与 `&&` 重载。
- 具有可访问名称能力的控件提供 `accessibleLabel()`，不与 automation ID 互相回退。
- 观察入口：`empty()`、仅左值可用的 `node()`。
- 应用作者把 Builder 或 `body()` 组件直接交给组合/页面边界，不调用 `build()`。
- `ViewLike` 包含 Builder、`body()` 组件、低层 `unique_ptr<NodeT>` 和动态 `View`。
- `build() &&` 仅作为控件实现与节点级测试的低层所有权出口。
- `AnyChildren` Builder 才有 `children()`；重复调用按顺序追加，空节点被拒绝，失败批次不会部分修改父节点。
- `SingleContent` Builder 使用 `content()`，重复调用替换原内容，运行时节点也验证最多一个 child。
- 组合、窗口、导航、Dialog 和 Overlay 在内部统一物化；公开作者 API 不再提供 `asNode()`。
- `View` 是 move-only、一次性动态类型擦除，只用于路由、跨模块工厂和异构运行时分支。
- `ButtonVariant`、`variant()`、`setVariant()` 已删除，只保留 `ButtonAppearance`。
- `Image` 可直接从不可变共享 `ImageSource` 构造，`source()` 与 `fallback()` 也接受
  `ImageSource`，应用组件无需为复用图片资源手工创建 `ImageNode`。

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
| `RadioGroup` | `RadioGroupNode` | `children(Radio...)` typed children |
| `Switch` | `SwitchNode` | leaf |
| `Slider` | `SliderNode` | leaf |

### 内容、反馈与身份

| Builder | Node | 能力 |
| --- | --- | --- |
| `Card` | `CardNode` | children |
| `CardHeader` | `CardHeaderNode` | `media()` / `action()` 具名槽位 |
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
| `AvatarGroup` | `AvatarGroupNode` | `children(Avatar...)` typed children |
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
| `Accordion` | `AccordionNode` | `children(AccordionItem...)` typed children |
| `Drawer` | `DrawerNode` | `content()` 专用组合 |
| `Popover` | `PopoverNode` | leaf |
| `PopoverButton` | `PopoverButtonNode` | leaf |
| `TeachingPopover` | `TeachingPopoverNode` | leaf |
| `Toolbar` | `ToolbarNode` | `item()` factory only |
| `TabList` | `TabListNode` | `tab()` factory only |
| `TabPanel` | `TabPanelNode` | children |
| `Link` | `LinkNode` | leaf |
| `Breadcrumb` | `BreadcrumbNode` | `item()` factory only |
| `Dialog` | `DialogNode` | `content()` 专用组合 |
| `If` | `IfNode` | `then()` 结构工厂 |
| `ForEach<T>` | `ForEachNode` | 非 keyed 结构集合 |
| `KeyedForEach<T>` | `ForEachNode` | keyed 结构集合 |

## 自动门禁

`declarative_api_contract_tests.cpp` 编译期覆盖全部 66 个映射及其
`children()`/`content()`/typed children/item factory/slot capability，并验证左值/右值
modifier、`ViewLike`、`body()`、动态 `View`、低层具体类型 `build()`、`node()` 和
move-only 约束。运行期覆盖组件单次物化、View 二次消费、Navigator factory、
If/ForEach、具名槽位、empty Builder、原始 `unique_ptr<DerivedNode>`、重复 children、
单内容替换、非法槽位插入和批量失败的父节点强保证。

`WhatsUIDeclarativeDomainHeaderTests` 以干净消费者分别 include 每个领域头，并包含一个
仅依赖公开 API 的自定义 Node/Builder；这些干净消费者通过 `View` 验证，无需调用
`build()`。身份拆分、强类型 `NodeKey` 和关键复杂 modifier
的左右值配对也由编译期断言锁定。完整 modifier 规则见
`DECLARATIVE_MODIFIER_INVENTORY.md`。
