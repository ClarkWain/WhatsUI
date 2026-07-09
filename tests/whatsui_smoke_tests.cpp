#include <memory>
#include <stdexcept>
#include <string>

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
    testTextInputRouting();
    return 0;
}
