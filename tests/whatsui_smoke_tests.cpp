#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "wui/wui.h"

namespace {

class DummyNode : public wui::Node {
public:
    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        return constraints.clamp({10.0f, 10.0f});
    }

    void paint(wui::PaintContext& context) override
    {
        (void)context;
        clearDirty(wui::DirtyFlag::Paint);
    }
};

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class FixedMeasurer : public wui::TextMeasurer {
public:
    [[nodiscard]] wui::TextExtents measureText(const std::string& text, float fontSize) const override
    {
        // Deterministic and distinct from the built-in heuristic.
        return {static_cast<float>(text.size()) * fontSize, fontSize * 2.0f, fontSize * 0.75f};
    }
};

void testPluggableTextMeasurement()
{
    FixedMeasurer measurer;
    wui::setTextMeasurer(&measurer);

    wui::Text text{"abcd"};
    const auto measured = text.measure(wui::Constraints{});
    expect(measured.width == 4.0f * 16.0f, "Text::measure should use the installed measurer's width");
    expect(measured.height == 32.0f, "Text::measure should use the installed measurer's height");

    wui::setTextMeasurer(nullptr);
    const auto fallback = text.measure(wui::Constraints{});
    expect(fallback.width == 4.0f * 8.0f, "Text::measure should fall back to the heuristic without a measurer");
}

void testState()
{
    wui::State<int> state{1};
    int observed = 0;
    const auto subscription = state.subscribe([&observed](const int& value) {
        observed = value;
    });

    const bool changed = state.set(3);
    expect(changed, "State should report change when value changes");
    expect(state.get() == 3, "State should store the latest value");
    expect(observed == 3, "State subscriber should observe updated value");

    state.unsubscribe(subscription);
}

void testNavigator()
{
    wui::Navigator navigator;
    navigator.setRoot("home", std::make_unique<DummyNode>());
    navigator.push("settings", std::make_unique<DummyNode>());

    expect(navigator.size() == 2, "Navigator should hold root and pushed page");
    expect(navigator.canPop(), "Navigator should allow popping when more than one page exists");
    expect(navigator.currentKey() != nullptr && *navigator.currentKey() == "settings", "Navigator should expose current page key");

    auto popped = navigator.pop();
    expect(static_cast<bool>(popped), "Navigator should return the popped page");
    expect(navigator.size() == 1, "Navigator should keep root page after pop");
}

void testTextInputModel()
{
    wui::TextInputModel model;
    model.setText("abc");
    model.setSelection({1, 3});
    model.commit("z");

    expect(model.text() == "az", "TextInputModel should replace the active selection on commit");
    expect(model.selection().start == 2 && model.selection().end == 2, "Caret should collapse at the end of committed text");
}

void testInputRouterAndButton()
{
    auto button = std::make_unique<wui::Button>("Open");
    bool clicked = false;
    button->onClick([&clicked]() {
        clicked = true;
    });
    button->layout({0.0f, 0.0f, 120.0f, 32.0f});

    wui::FocusManager focusManager;
    wui::InputRouter router(&focusManager);
    router.setRoot(button.get());

    const wui::PointerEvent moveInside{0, wui::PointerType::Mouse, wui::PointerAction::Move, wui::MouseButton::None, {10.0f, 10.0f}, 0};
    const wui::PointerEvent down{0, wui::PointerType::Mouse, wui::PointerAction::Down, wui::MouseButton::Left, {10.0f, 10.0f}, 0};
    const wui::PointerEvent up{0, wui::PointerType::Mouse, wui::PointerAction::Up, wui::MouseButton::Left, {10.0f, 10.0f}, 0};
    const wui::PointerEvent moveOutside{0, wui::PointerType::Mouse, wui::PointerAction::Move, wui::MouseButton::None, {200.0f, 200.0f}, 0};

    expect(router.dispatchPointer(moveInside), "Pointer move inside should be handled by the button");
    expect((button->visualStates() & wui::toMask(wui::ControlVisualState::Hovered)) != 0, "Button should enter hovered state");
    expect(router.dispatchPointer(down), "Pointer down should be handled by the button");
    expect(focusManager.focused() == button.get(), "Pointer down should focus the button");
    expect(router.dispatchPointer(up), "Pointer up should be handled by the button");
    expect(clicked, "Button click handler should run after pointer down/up");
    expect(!router.dispatchPointer(moveOutside), "Pointer move outside should not target the button");
    expect((button->visualStates() & wui::toMask(wui::ControlVisualState::Hovered)) == 0, "Button should clear hovered state after leave");
}

void testDeclarativeBuilderAndCounter()
{
    using namespace wui::ui;

    wui::State<int> count{0};

    // Declarative authoring: config first (padding/gap), children last.
    std::unique_ptr<wui::Node> root =
        Column()
            .padding(16)
            .gap(8)
            .children(
                Text("Counter"),
                Row().gap(8).children(
                    Text("Value:"),
                    Button("Increment").onClick([&count] { count.set(count.get() + 1); })
                )
            );

    auto* column = dynamic_cast<wui::Column*>(root.get());
    expect(column != nullptr, "Builder root should be a Column node");
    expect(column->children().size() == 2, "Column should have exactly two children");
    expect(column->padding().left == 16.0f && column->padding().top == 16.0f,
           "padding(16) should apply uniform insets to the Column node");
    expect(column->gap() == 8.0f, "gap(8) should apply to the Column node");

    auto* row = dynamic_cast<wui::Row*>(column->children()[1].get());
    expect(row != nullptr, "Second child should be the nested Row");
    expect(row->children().size() == 2, "Row should hold the label and the button");

    auto* button = dynamic_cast<wui::Button*>(row->children()[1].get());
    expect(button != nullptr, "Row's second child should be a Button");

    // Lay the tree out so node bounds are valid, then click the button.
    root->layout({0.0f, 0.0f, 300.0f, 200.0f});
    const wui::PointerEvent down{0, wui::PointerType::Mouse, wui::PointerAction::Down, wui::MouseButton::Left, {0.0f, 0.0f}, 0};
    const wui::PointerEvent up{0, wui::PointerType::Mouse, wui::PointerAction::Up, wui::MouseButton::Left, {0.0f, 0.0f}, 0};
    button->onPointerEvent(down);
    button->onPointerEvent(up);

    expect(count.get() == 1, "Clicking the declaratively-built button should increment the counter state");
}

void testReactiveText()
{
    using namespace wui::ui;

    wui::State<int> count{0};
    std::unique_ptr<wui::Node> node =
        Text().bind(count, [](const int& value) { return std::string("Count: ") + std::to_string(value); });

    auto* text = dynamic_cast<wui::Text*>(node.get());
    expect(text != nullptr, "Bound builder should yield a Text node");
    expect(text->value() == "Count: 0", "Reactive Text should render the initial state");

    count.set(5);
    expect(text->value() == "Count: 5", "Reactive Text should update when the bound state changes");
}

void testComputed()
{
    wui::State<int> a{2};
    wui::State<int> b{3};
    wui::Computed<int> sum([&a, &b] { return a.get() + b.get(); }, a, b);
    expect(sum.get() == 5, "Computed should hold the initial derived value");

    int observed = 0;
    sum.subscribe([&observed](const int& value) { observed = value; });
    a.set(10);
    expect(sum.get() == 13, "Computed should recompute when a source State changes");
    expect(observed == 13, "Computed should notify its own observers on change");
}

void testTheme()
{
    wui::Theme custom;
    custom.colors.accent = wui::Color{10, 20, 30, 255};
    wui::setTheme(custom);
    expect(wui::theme().colors.accent.r == 10, "setTheme should install the theme");

    wui::setTheme(wui::Theme{});
    expect(wui::theme().colors.accent.r == 34, "default theme accent should be restored");
}

void testStructuralIf()
{
    using namespace wui::ui;

    wui::State<bool> show{false};
    std::unique_ptr<wui::Node> node =
        If(show).then([] { return Text("Advanced"); });

    auto* ifNode = dynamic_cast<wui::IfNode*>(node.get());
    expect(ifNode != nullptr, "If builder should yield an IfNode");
    expect(ifNode->children().empty(), "If should not mount its child while the state is false");

    show.set(true);
    wui::flushStructuralUpdates();
    expect(ifNode->children().size() == 1, "If should mount the child when the state becomes true");

    show.set(false);
    wui::flushStructuralUpdates();
    expect(ifNode->children().empty(), "If should unmount the child when the state becomes false");
}

void testStructuralForEach()
{
    using namespace wui::ui;

    wui::State<std::vector<std::string>> items{{"a", "b"}};
    std::unique_ptr<wui::Node> node =
        ForEach<std::string>(items, [](const std::string& label) { return Text(label); });

    auto* list = dynamic_cast<wui::ForEachNode*>(node.get());
    expect(list != nullptr, "ForEach builder should yield a ForEachNode");
    expect(list->children().size() == 2, "ForEach should generate one child per item");

    items.set({"a", "b", "c"});
    wui::flushStructuralUpdates();
    expect(list->children().size() == 3, "ForEach should rebuild children when the list changes");

    items.set({});
    wui::flushStructuralUpdates();
    expect(list->children().empty(), "ForEach should clear children for an empty list");
}

void testLayoutFlexAndAlign()
{
    using namespace wui::ui;

    std::unique_ptr<wui::Node> root =
        Row().align(wui::Alignment::Center).children(
            Spacer(20.0f, 10.0f),
            Spacer().flex(1),
            Spacer(30.0f, 40.0f));

    auto* row = dynamic_cast<wui::Row*>(root.get());
    expect(row != nullptr, "Builder should yield a Row node");
    row->layout({0.0f, 0.0f, 200.0f, 40.0f});

    const auto& kids = row->children();
    expect(kids.size() == 3, "Row should hold three children");
    expect(kids[0]->bounds().x == 0.0f, "First child sits at the start");
    expect(kids[2]->bounds().x == 170.0f, "Flex spacer pushes the last child to the end");
    expect(kids[0]->bounds().y == 15.0f, "Cross-axis Center centers a short child vertically");
}

void testTextInputRouting()
{
    auto input = std::make_unique<wui::TextInput>("Type here");
    input->layout({0.0f, 0.0f, 160.0f, 32.0f});

    wui::FocusManager focusManager;
    wui::InputRouter router(&focusManager);
    router.setRoot(input.get());

    const wui::PointerEvent down{0, wui::PointerType::Mouse, wui::PointerAction::Down, wui::MouseButton::Left, {10.0f, 10.0f}, 0};
    const wui::CompositionInputEvent compositionEvent{0, "ni"};
    const wui::TextInputEvent textEvent{0, "ab"};
    const wui::KeyEvent backspace{0, wui::KeyAction::Down, 8, 0, false};

    expect(router.dispatchPointer(down), "TextInput should accept pointer focus");
    expect(focusManager.focused() == input.get(), "TextInput should become the focused node after pointer down");
    expect(router.dispatchComposition(compositionEvent), "Focused TextInput should accept composition events");
    expect(input->model().text() == "ni", "Composition should update the transient text content");
    expect(!input->model().composition().empty(), "Composition range should be tracked while IME text is active");
    expect(router.dispatchTextInput(textEvent), "Focused TextInput should accept text input events");
    expect(input->model().text() == "ab", "TextInput should commit incoming text input events");
    expect(input->model().composition().empty(), "Committed text input should clear composition state");
    expect(router.dispatchKey(backspace), "Focused TextInput should accept backspace through key routing");
    expect(input->model().text() == "a", "TextInput backspace should delete one character");
}

} // namespace

int main()
{
    testState();
    testNavigator();
    testTextInputModel();
    testInputRouterAndButton();
    testDeclarativeBuilderAndCounter();
    testReactiveText();
    testComputed();
    testTheme();
    testStructuralIf();
    testStructuralForEach();
    testPluggableTextMeasurement();
    testLayoutFlexAndAlign();
    testTextInputRouting();
    return 0;
}
