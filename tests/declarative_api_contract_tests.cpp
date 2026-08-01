#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "wui/wui.h"

namespace {

template <typename T, typename = void>
struct HasChildren : std::false_type {
};

template <typename T>
struct HasChildren<T,
                   std::void_t<decltype(std::declval<T&&>().children())>>
    : std::true_type {
};

template <typename T, typename = void>
struct HasRvalueBuild : std::false_type {
};

template <typename T>
struct HasRvalueBuild<T,
                      std::void_t<decltype(std::declval<T&&>().build())>>
    : std::true_type {
};

template <typename T, typename = void>
struct HasLvalueBuild : std::false_type {
};

template <typename T>
struct HasLvalueBuild<T,
                      std::void_t<decltype(std::declval<T&>().build())>>
    : std::true_type {
};

template <typename T, typename = void>
struct HasRvalueNodeAccess : std::false_type {
};

template <typename T>
struct HasRvalueNodeAccess<T,
                           std::void_t<decltype(std::declval<T&&>().node())>>
    : std::true_type {
};

template <typename T, typename = void>
struct CanBuildAfterLvalueModifier : std::false_type {
};

template <typename T>
struct CanBuildAfterLvalueModifier<
    T,
    std::void_t<decltype(std::declval<T&>()
                             .appearance(wui::ButtonAppearance::Primary)
                             .build())>> : std::true_type {
};

static_assert(std::is_base_of_v<wui::Node, wui::ButtonNode>);
static_assert(!std::is_base_of_v<wui::Node, wui::Button>);
static_assert(!std::is_copy_constructible_v<wui::Button>);
static_assert(std::is_move_constructible_v<wui::Button>);
static_assert(!HasChildren<wui::Text>::value);
static_assert(HasChildren<wui::Column>::value);
static_assert(HasRvalueBuild<wui::Button>::value);
static_assert(!HasLvalueBuild<wui::Button>::value);
static_assert(!HasRvalueNodeAccess<wui::Button>::value);
static_assert(std::is_same_v<
              decltype(std::declval<wui::Button&>().appearance(
                  wui::ButtonAppearance::Primary)),
              wui::Button&>);
static_assert(std::is_same_v<
              decltype(std::declval<wui::Button&&>().appearance(
                  wui::ButtonAppearance::Primary)),
              wui::Button&&>);
static_assert(!CanBuildAfterLvalueModifier<wui::Button>::value);

#define WUI_ASSERT_LEAF_BUILDER(Builder, RuntimeNode)                         \
    static_assert(std::is_same_v<typename wui::Builder::node_type,            \
                                 wui::RuntimeNode>);                           \
    static_assert(!HasChildren<wui::Builder>::value)

#define WUI_ASSERT_CONTAINER_BUILDER(Builder, RuntimeNode)                    \
    static_assert(std::is_same_v<typename wui::Builder::node_type,            \
                                 wui::RuntimeNode>);                           \
    static_assert(HasChildren<wui::Builder>::value)

WUI_ASSERT_LEAF_BUILDER(Text, TextNode);
WUI_ASSERT_LEAF_BUILDER(Icon, IconNode);
WUI_ASSERT_LEAF_BUILDER(Image, ImageNode);
WUI_ASSERT_CONTAINER_BUILDER(Box, BoxNode);
WUI_ASSERT_LEAF_BUILDER(Spacer, SpacerNode);
WUI_ASSERT_LEAF_BUILDER(TextField, TextFieldNode);
WUI_ASSERT_LEAF_BUILDER(TextArea, TextAreaNode);
WUI_ASSERT_CONTAINER_BUILDER(Card, CardNode);
WUI_ASSERT_CONTAINER_BUILDER(CardHeader, CardHeaderNode);
WUI_ASSERT_CONTAINER_BUILDER(CardPreview, CardPreviewNode);
WUI_ASSERT_CONTAINER_BUILDER(CardFooter, CardFooterNode);
WUI_ASSERT_LEAF_BUILDER(Label, LabelNode);
WUI_ASSERT_LEAF_BUILDER(Field, FieldNode);
WUI_ASSERT_LEAF_BUILDER(MessageBar, MessageBarNode);
WUI_ASSERT_LEAF_BUILDER(Button, ButtonNode);
WUI_ASSERT_LEAF_BUILDER(Checkbox, CheckboxNode);
WUI_ASSERT_LEAF_BUILDER(ToggleButton, ToggleButtonNode);
WUI_ASSERT_LEAF_BUILDER(CompoundButton, CompoundButtonNode);
WUI_ASSERT_LEAF_BUILDER(Radio, RadioNode);
WUI_ASSERT_CONTAINER_BUILDER(RadioGroup, RadioGroupNode);
WUI_ASSERT_LEAF_BUILDER(Switch, SwitchNode);
WUI_ASSERT_LEAF_BUILDER(Slider, SliderNode);
WUI_ASSERT_LEAF_BUILDER(ProgressBar, ProgressBarNode);
WUI_ASSERT_LEAF_BUILDER(Toast, ToastNode);
WUI_ASSERT_LEAF_BUILDER(Spinner, SpinnerNode);
WUI_ASSERT_LEAF_BUILDER(Divider, DividerNode);
WUI_ASSERT_LEAF_BUILDER(Badge, BadgeNode);
WUI_ASSERT_LEAF_BUILDER(CounterBadge, CounterBadgeNode);
WUI_ASSERT_LEAF_BUILDER(PresenceBadge, PresenceBadgeNode);
WUI_ASSERT_LEAF_BUILDER(Avatar, AvatarNode);
WUI_ASSERT_CONTAINER_BUILDER(AvatarGroup, AvatarGroupNode);
WUI_ASSERT_LEAF_BUILDER(Persona, PersonaNode);
WUI_ASSERT_LEAF_BUILDER(Calendar, CalendarNode);
WUI_ASSERT_LEAF_BUILDER(DatePicker, DatePickerNode);
WUI_ASSERT_LEAF_BUILDER(TimePicker, TimePickerNode);
WUI_ASSERT_LEAF_BUILDER(Table, TableNode);
WUI_ASSERT_LEAF_BUILDER(DataGrid, DataGridNode);
WUI_ASSERT_LEAF_BUILDER(Tree, TreeNode);
WUI_ASSERT_LEAF_BUILDER(AccordionItem, AccordionItemNode);
WUI_ASSERT_CONTAINER_BUILDER(Accordion, AccordionNode);
WUI_ASSERT_LEAF_BUILDER(Drawer, DrawerNode);
WUI_ASSERT_LEAF_BUILDER(Popover, PopoverNode);
WUI_ASSERT_LEAF_BUILDER(PopoverButton, PopoverButtonNode);
WUI_ASSERT_LEAF_BUILDER(TeachingPopover, TeachingPopoverNode);
WUI_ASSERT_CONTAINER_BUILDER(Toolbar, ToolbarNode);
WUI_ASSERT_CONTAINER_BUILDER(TabList, TabListNode);
WUI_ASSERT_CONTAINER_BUILDER(TabPanel, TabPanelNode);
WUI_ASSERT_LEAF_BUILDER(Link, LinkNode);
WUI_ASSERT_CONTAINER_BUILDER(Breadcrumb, BreadcrumbNode);
WUI_ASSERT_LEAF_BUILDER(ListBox, ListBoxNode);
WUI_ASSERT_LEAF_BUILDER(Combobox, ComboboxNode);
WUI_ASSERT_LEAF_BUILDER(Dropdown, DropdownNode);
WUI_ASSERT_LEAF_BUILDER(Rating, RatingNode);
WUI_ASSERT_LEAF_BUILDER(RatingDisplay, RatingDisplayNode);
WUI_ASSERT_LEAF_BUILDER(ListView, ListViewNode);
WUI_ASSERT_LEAF_BUILDER(IconButton, IconButtonNode);
WUI_ASSERT_LEAF_BUILDER(MenuButton, MenuButtonNode);
WUI_ASSERT_LEAF_BUILDER(SplitButton, SplitButtonNode);
WUI_ASSERT_LEAF_BUILDER(SearchField, SearchFieldNode);
WUI_ASSERT_CONTAINER_BUILDER(Row, RowNode);
WUI_ASSERT_CONTAINER_BUILDER(Column, ColumnNode);
WUI_ASSERT_CONTAINER_BUILDER(ScrollView, ScrollViewNode);
WUI_ASSERT_LEAF_BUILDER(Dialog, DialogNode);
WUI_ASSERT_LEAF_BUILDER(If, IfNode);
WUI_ASSERT_LEAF_BUILDER(ForEach<int>, ForEachNode);
WUI_ASSERT_LEAF_BUILDER(KeyedForEach<int>, ForEachNode);

#undef WUI_ASSERT_CONTAINER_BUILDER
#undef WUI_ASSERT_LEAF_BUILDER

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testSingleNamespaceAuthoringAndBuild()
{
    using namespace wui;

    Button button{"Save"};
    button.appearance(ButtonAppearance::Primary);
    expect(!button.empty() && button.node() != nullptr,
           "A new Builder should own its runtime node");

    NodePtr node = std::move(button).build();
    expect(node != nullptr && dynamic_cast<ButtonNode*>(node.get()) != nullptr,
           "build() should return the matching runtime Node type");
    expect(button.empty() && button.node() == nullptr,
           "A successfully built Builder should become empty");

    bool rejectedSecondBuild = false;
    try {
        (void)std::move(button).build();
    } catch (const std::logic_error&) {
        rejectedSecondBuild = true;
    }
    expect(rejectedSecondBuild,
           "An empty Builder should reject a second build()");
}

void testNamedContainerSupportsConditionalComposition()
{
    using namespace wui;

    Column column;
    column.gap(8.0f);
    column.children(
        Text("Title"),
        Button("Continue").appearance(ButtonAppearance::Primary));

    NodePtr node = std::move(column).build();
    auto* columnNode = dynamic_cast<ColumnNode*>(node.get());
    expect(columnNode != nullptr && columnNode->children().size() == 2,
           "A named container Builder should append and build its children");
}

void testEmptyBuilderRejectsModifiers()
{
    using namespace wui;

    Button button{"Save"};
    NodePtr node = std::move(button).build();

    bool rejectedModifier = false;
    try {
        button.appearance(ButtonAppearance::Primary);
    } catch (const std::logic_error&) {
        rejectedModifier = true;
    }
    expect(rejectedModifier,
           "An empty Builder should reject modifier calls instead of dereferencing null");
}

void testChildrenAreTransactionalAndRepeatable()
{
    using namespace wui;

    Column column;
    column.children(Text("First"));
    column.children(Button("Second"));
    expect(column.node()->children().size() == 2,
           "Repeated children() calls should append in source order");

    auto child = std::make_unique<TextNode>("Consumed on failure");
    bool rejectedNullChild = false;
    try {
        column.children(std::move(child), NodePtr{});
    } catch (const std::invalid_argument&) {
        rejectedNullChild = true;
    }
    expect(rejectedNullChild,
           "children() should reject a null runtime node");
    expect(column.node()->children().size() == 2,
           "A failed children() batch must not partially mutate the parent");
    expect(child == nullptr,
           "children() documents consuming earlier rvalue arguments during validation");
}

void testMoveAndRawNodeConsumption()
{
    using namespace wui;

    Button source{"Move me"};
    source = std::move(source);
    expect(!source.empty(), "Builder self-move assignment should preserve its node");

    Button destination{"Discarded destination"};
    destination = std::move(source);
    expect(source.empty() && !destination.empty(),
           "Builder move assignment should transfer exactly one node owner");

    Column column;
    column.children(std::make_unique<TextNode>("Raw node"),
                    std::move(destination));
    NodePtr result = std::move(column).build();
    expect(result->children().size() == 2 && destination.empty(),
           "children() should consume both runtime nodes and named Builders explicitly");

    bool rejectedNullNode = false;
    try {
        (void)asNode(std::unique_ptr<TextNode>{});
    } catch (const std::invalid_argument&) {
        rejectedNullNode = true;
    }
    expect(rejectedNullNode, "asNode() should reject null unique_ptr values");
}

} // namespace

int main()
{
    try {
        testSingleNamespaceAuthoringAndBuild();
        testNamedContainerSupportsConditionalComposition();
        testEmptyBuilderRejectsModifiers();
        testChildrenAreTransactionalAndRepeatable();
        testMoveAndRawNodeConsumption();
    } catch (const std::exception& error) {
        std::cerr << "declarative_api_contract_tests failed: "
                  << error.what() << '\n';
        return 1;
    }
    return 0;
}
