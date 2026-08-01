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
struct HasContent : std::false_type {
};

template <typename T>
struct HasContent<
    T,
    std::void_t<decltype(std::declval<T&&>().content(wui::Text()))>>
    : std::true_type {
};

template <typename T, typename = void>
struct HasAutomationId : std::false_type {
};

template <typename T>
struct HasAutomationId<
    T,
    std::void_t<decltype(std::declval<T&&>().automationId("id"))>>
    : std::true_type {
};

template <typename T, typename = void>
struct HasDebugName : std::false_type {
};

template <typename T>
struct HasDebugName<
    T,
    std::void_t<decltype(std::declval<T&&>().debugName("name"))>>
    : std::true_type {
};

template <typename T, typename = void>
struct HasMedia : std::false_type {
};

template <typename T>
struct HasMedia<
    T,
    std::void_t<decltype(std::declval<T&&>().media(wui::Text()))>>
    : std::true_type {
};

template <typename T, typename = void>
struct HasAction : std::false_type {
};

template <typename T>
struct HasAction<
    T,
    std::void_t<decltype(std::declval<T&&>().action(wui::Button()))>>
    : std::true_type {
};

template <typename Parent, typename Child, typename = void>
struct AcceptsMedia : std::false_type {
};

template <typename Parent, typename Child>
struct AcceptsMedia<
    Parent,
    Child,
    std::void_t<decltype(std::declval<Parent&&>().media(
        std::declval<Child&&>()))>> : std::true_type {
};

template <typename Parent, typename Child, typename = void>
struct AcceptsChild : std::false_type {
};

template <typename Parent, typename Child>
struct AcceptsChild<
    Parent,
    Child,
    std::void_t<decltype(std::declval<Parent&&>().children(
        std::declval<Child&&>()))>> : std::true_type {
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

struct GreetingComponent {
    int* bodyCalls{nullptr};

    auto body()
    {
        if (bodyCalls != nullptr) {
            ++*bodyCalls;
        }
        return wui::Column()
            .children(
                wui::Text("Hello"),
                wui::Button("Continue"));
    }
};

struct RadioComponent {
    auto body()
    {
        return wui::Radio("Choice").selected(true);
    }
};

struct InvalidComponent {
    int body() { return 0; }
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
static_assert(HasAutomationId<wui::Button>::value);
static_assert(HasDebugName<wui::Button>::value);
static_assert(std::is_same_v<
              decltype(wui::IconButton().accessibleLabel("Icon")),
              wui::IconButton&&>);
static_assert(!std::is_convertible_v<std::string, wui::NodeKey>);
static_assert(wui::isViewLikeV<wui::Button>);
static_assert(wui::isViewLikeV<GreetingComponent>);
static_assert(wui::isViewLikeV<std::unique_ptr<wui::ButtonNode>>);
static_assert(!wui::isViewLikeV<int>);
static_assert(!wui::isViewLikeV<InvalidComponent>);
static_assert(std::is_constructible_v<wui::View, GreetingComponent>);
static_assert(std::is_move_constructible_v<wui::View>);
static_assert(!std::is_copy_constructible_v<wui::View>);
static_assert(AcceptsChild<wui::Column, GreetingComponent>::value);
static_assert(AcceptsChild<wui::RadioGroup, RadioComponent>::value);
static_assert(AcceptsMedia<wui::CardHeader, GreetingComponent>::value);
static_assert(std::is_same_v<
              decltype(wui::Button("Save").build()),
              std::unique_ptr<wui::ButtonNode>>);
static_assert(std::is_same_v<
              decltype(wui::Dialog().build()),
              std::unique_ptr<wui::DialogNode>>);

#define WUI_ASSERT_LEAF_BUILDER(Builder, RuntimeNode)                         \
    static_assert(std::is_same_v<typename wui::Builder::node_type,            \
                                 wui::RuntimeNode>);                           \
    static_assert(!HasChildren<wui::Builder>::value)

#define WUI_ASSERT_CONTAINER_BUILDER(Builder, RuntimeNode)                    \
    static_assert(std::is_same_v<typename wui::Builder::node_type,            \
                                 wui::RuntimeNode>);                           \
    static_assert(HasChildren<wui::Builder>::value)

#define WUI_ASSERT_SINGLE_CONTENT_BUILDER(Builder, RuntimeNode)               \
    static_assert(std::is_same_v<typename wui::Builder::node_type,            \
                                 wui::RuntimeNode>);                           \
    static_assert(!HasChildren<wui::Builder>::value);                         \
    static_assert(HasContent<wui::Builder>::value)

WUI_ASSERT_LEAF_BUILDER(Text, TextNode);
WUI_ASSERT_LEAF_BUILDER(Icon, IconNode);
WUI_ASSERT_LEAF_BUILDER(Image, ImageNode);
WUI_ASSERT_CONTAINER_BUILDER(Box, BoxNode);
WUI_ASSERT_LEAF_BUILDER(Spacer, SpacerNode);
WUI_ASSERT_LEAF_BUILDER(TextField, TextFieldNode);
WUI_ASSERT_LEAF_BUILDER(TextArea, TextAreaNode);
WUI_ASSERT_CONTAINER_BUILDER(Card, CardNode);
WUI_ASSERT_LEAF_BUILDER(CardHeader, CardHeaderNode);
static_assert(HasMedia<wui::CardHeader>::value);
static_assert(HasAction<wui::CardHeader>::value);
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
static_assert(AcceptsChild<wui::RadioGroup, wui::Radio>::value);
static_assert(!AcceptsChild<wui::RadioGroup, wui::Text>::value);
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
static_assert(AcceptsChild<wui::AvatarGroup, wui::Avatar>::value);
static_assert(!AcceptsChild<wui::AvatarGroup, wui::Button>::value);
WUI_ASSERT_LEAF_BUILDER(Persona, PersonaNode);
WUI_ASSERT_LEAF_BUILDER(Calendar, CalendarNode);
WUI_ASSERT_LEAF_BUILDER(DatePicker, DatePickerNode);
WUI_ASSERT_LEAF_BUILDER(TimePicker, TimePickerNode);
WUI_ASSERT_LEAF_BUILDER(Table, TableNode);
WUI_ASSERT_LEAF_BUILDER(DataGrid, DataGridNode);
WUI_ASSERT_LEAF_BUILDER(Tree, TreeNode);
WUI_ASSERT_LEAF_BUILDER(AccordionItem, AccordionItemNode);
WUI_ASSERT_CONTAINER_BUILDER(Accordion, AccordionNode);
static_assert(AcceptsChild<wui::Accordion, wui::AccordionItem>::value);
static_assert(!AcceptsChild<wui::Accordion, wui::Text>::value);
WUI_ASSERT_LEAF_BUILDER(Drawer, DrawerNode);
WUI_ASSERT_LEAF_BUILDER(Popover, PopoverNode);
WUI_ASSERT_LEAF_BUILDER(PopoverButton, PopoverButtonNode);
WUI_ASSERT_LEAF_BUILDER(TeachingPopover, TeachingPopoverNode);
WUI_ASSERT_LEAF_BUILDER(Toolbar, ToolbarNode);
WUI_ASSERT_LEAF_BUILDER(TabList, TabListNode);
WUI_ASSERT_CONTAINER_BUILDER(TabPanel, TabPanelNode);
WUI_ASSERT_LEAF_BUILDER(Link, LinkNode);
WUI_ASSERT_LEAF_BUILDER(Breadcrumb, BreadcrumbNode);
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
WUI_ASSERT_SINGLE_CONTENT_BUILDER(ScrollView, ScrollViewNode);
WUI_ASSERT_LEAF_BUILDER(Dialog, DialogNode);
WUI_ASSERT_LEAF_BUILDER(If, IfNode);
WUI_ASSERT_LEAF_BUILDER(ForEach<int>, ForEachNode);
WUI_ASSERT_LEAF_BUILDER(KeyedForEach<int>, ForEachNode);

#undef WUI_ASSERT_CONTAINER_BUILDER
#undef WUI_ASSERT_LEAF_BUILDER
#undef WUI_ASSERT_SINGLE_CONTENT_BUILDER

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
        View nullView = std::unique_ptr<TextNode>{};
        Navigator navigator;
        navigator.setRoot("null", std::move(nullView));
    } catch (const std::invalid_argument&) {
        rejectedNullNode = true;
    }
    expect(rejectedNullNode,
           "A public ViewLike boundary should reject null unique_ptr values");
}

void testSingleContentBuilderReplacesContent()
{
    using namespace wui;

    ScrollView scrollView;
    scrollView.content(Text("First"));
    scrollView.content(Text("Second"));

    expect(scrollView.node()->children().size() == 1,
           "SingleContent Builder should retain exactly one content node");
    const auto* content = dynamic_cast<const TextNode*>(
        scrollView.node()->content());
    expect(content != nullptr && content->value() == "Second",
           "A repeated content() call should replace the previous content");
}

void testComponentBodyMaterializesWithoutBuildKnowledge()
{
    int bodyCalls = 0;
    wui::UiRoot root;
    root.setContent(GreetingComponent{&bodyCalls});

    auto* column = dynamic_cast<wui::ColumnNode*>(root.content());
    expect(bodyCalls == 1 && column != nullptr
               && column->children().size() == 2,
           "UiRoot should materialize a Component body exactly once");

    wui::Column parent;
    parent.children(GreetingComponent{&bodyCalls});
    expect(bodyCalls == 2 && parent.node()->children().size() == 1
               && dynamic_cast<wui::ColumnNode*>(
                      parent.node()->children().front().get()) != nullptr,
           "Container children should accept Components without build/asNode");
}

void testDynamicViewErasesOnlyAtStoredBoundaries()
{
    wui::View page = GreetingComponent{};
    expect(!page.empty(), "A View should retain one deferred description");

    wui::Navigator navigator;
    navigator.setRoot("component", std::move(page));
    expect(page.empty()
               && dynamic_cast<wui::ColumnNode*>(navigator.current()) != nullptr,
           "Navigator should consume a dynamic View without exposing NodePtr");

    navigator.replace(
        "factory",
        [] { return GreetingComponent{}; },
        wui::PageRetention::DisposeOnHide);
    expect(dynamic_cast<wui::ColumnNode*>(navigator.current()) != nullptr,
           "Navigator factories should materialize Component results internally");

    bool rejectedSecondConsumption = false;
    try {
        navigator.replace("empty", std::move(page));
    } catch (const std::logic_error&) {
        rejectedSecondConsumption = true;
    }
    expect(rejectedSecondConsumption,
           "A consumed dynamic View should reject a second materialization");
}

void testComponentsReachSlotsAndStructuralFactories()
{
    wui::CardHeader header;
    header.media(GreetingComponent{});
    expect(header.node()->children().size() == 1
               && dynamic_cast<wui::ColumnNode*>(
                      header.node()->children().front().get()) != nullptr,
           "Named slots should accept Component values");

    wui::State<bool> visible{true};
    wui::If branch(visible);
    branch.then([] { return GreetingComponent{}; });
    expect(branch.node()->children().size() == 1
               && dynamic_cast<wui::ColumnNode*>(
                      branch.node()->children().front().get()) != nullptr,
           "If factories should accept Component results");

    wui::State<std::vector<int>> items{{1, 2}};
    wui::ForEach<int> rows(
        items,
        [](const int&) { return GreetingComponent{}; });
    expect(rows.node()->children().size() == 2,
           "ForEach factories should accept Component results");
}

template <class ParentNode, class ValidChildNode, class InvalidChildNode>
void expectTypedRuntimeContainerRejectsInvalidChild(const char* message)
{
    ParentNode parent;
    parent.appendChild(std::make_unique<ValidChildNode>());
    bool rejected = false;
    try {
        parent.appendChild(std::make_unique<InvalidChildNode>());
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected && parent.children().size() == 1, message);
}

void testSemanticContainersEnforceRuntimeChildTypes()
{
    expectTypedRuntimeContainerRejectsInvalidChild<
        wui::RadioGroupNode, wui::RadioNode, wui::TextNode>(
        "RadioGroupNode should accept only RadioNode children");
    expectTypedRuntimeContainerRejectsInvalidChild<
        wui::AccordionNode, wui::AccordionItemNode, wui::TextNode>(
        "AccordionNode should accept only AccordionItemNode children");
    expectTypedRuntimeContainerRejectsInvalidChild<
        wui::AvatarGroupNode, wui::AvatarNode, wui::TextNode>(
        "AvatarGroupNode should accept only AvatarNode children");
    expectTypedRuntimeContainerRejectsInvalidChild<
        wui::ToolbarNode, wui::ToolbarItemNode, wui::TextNode>(
        "ToolbarNode should accept only ToolbarItemNode children");
    expectTypedRuntimeContainerRejectsInvalidChild<
        wui::TabListNode, wui::TabNode, wui::TextNode>(
        "TabListNode should accept only TabNode children");
    expectTypedRuntimeContainerRejectsInvalidChild<
        wui::BreadcrumbNode, wui::BreadcrumbItemNode, wui::TextNode>(
        "BreadcrumbNode should accept only BreadcrumbItemNode children");
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
        testSingleContentBuilderReplacesContent();
        testComponentBodyMaterializesWithoutBuildKnowledge();
        testDynamicViewErasesOnlyAtStoredBoundaries();
        testComponentsReachSlotsAndStructuralFactories();
        testSemanticContainersEnforceRuntimeChildTypes();
    } catch (const std::exception& error) {
        std::cerr << "declarative_api_contract_tests failed: "
                  << error.what() << '\n';
        return 1;
    }
    return 0;
}
