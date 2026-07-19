#include <memory>
#include <cstdio>
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

class DirtyProbeNode final : public DummyNode {
public:
    void invalidate(wui::DirtyFlag flag) noexcept
    {
        markDirty(flag);
    }
};

// Intentionally violates the PaintContext save/restore contract.  The
// container boundary must contain this failure so a dynamically removed branch
// cannot leave a clip active for the next frame's mounted sibling.
class LeakyPaintStateNode final : public DummyNode {
public:
    void paint(wui::PaintContext& context) override
    {
        (void)context.save();
        context.clipRect({0.0f, 0.0f, 1.0f, 1.0f});
        clearDirty(wui::DirtyFlag::Paint);
    }
};

class EventProbe final : public wui::ContainerNode {
public:
    EventProbe(std::string name, std::vector<std::string>& trace)
        : name_(std::move(name)), trace_(trace) {}

    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        return constraints.clamp({10.0f, 10.0f});
    }

    wui::EventResult onPointerEvent(const wui::PointerEvent&, wui::EventContext& context) override
    {
        const char* phase = "target";
        if (context.phase() == wui::EventPhase::Capture) phase = "capture";
        if (context.phase() == wui::EventPhase::Bubble) phase = "bubble";
        trace_.push_back(name_ + ":" + phase);
        if (stopWithContext_) context.stopPropagation();
        if (focusWithContext_) context.requestFocus(focusTarget_);
        if (captureWithContext_) context.capturePointer();
        return result_;
    }

    wui::EventResult result_{wui::EventResult::Ignored};
    bool stopWithContext_{false};
    bool focusWithContext_{false};
    bool captureWithContext_{false};
    wui::Node* focusTarget_{nullptr};

private:
    std::string name_;
    std::vector<std::string>& trace_;
};

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testInvalidationReachesTheRootAndPaintsAfterLayout()
{
    auto root = std::make_unique<DirtyProbeNode>();
    auto child = std::make_unique<DirtyProbeNode>();
    auto* childRaw = child.get();
    root->appendChild(std::move(child));
    root->clearDirty();
    childRaw->clearDirty();

    int redraws = 0;
    root->setInvalidationHandler([&] { ++redraws; });

    childRaw->invalidate(wui::DirtyFlag::Paint);
    expect(childRaw->isDirty(wui::DirtyFlag::Paint), "Paint invalidation should mark the changed node");
    expect(root->isDirty(wui::DirtyFlag::Paint), "Paint invalidation should reach the root paint boundary");
    expect(!childRaw->isDirty(wui::DirtyFlag::Layout) && !root->isDirty(wui::DirtyFlag::Layout),
           "Paint-only invalidation must not invalidate layout");
    expect(redraws == 1, "A descendant paint invalidation should notify the root once");

    root->clearDirty();
    childRaw->clearDirty();
    childRaw->invalidate(wui::DirtyFlag::Layout);
    expect(childRaw->isDirty(wui::DirtyFlag::Layout) && root->isDirty(wui::DirtyFlag::Layout),
           "Layout invalidation should reach the root layout boundary");
    expect(childRaw->isDirty(wui::DirtyFlag::Paint) && root->isDirty(wui::DirtyFlag::Paint),
           "Layout invalidation must also invalidate paint along the path to the root");
    expect(redraws == 2, "A descendant layout invalidation should notify the root once");
}

void testContainerPaintStateIsolation()
{
    wui::Container root;
    root.appendChild(std::make_unique<LeakyPaintStateNode>());
    root.appendChild(std::make_unique<DummyNode>());
    root.layout({0.0f, 0.0f, 100.0f, 100.0f});

    wui::PaintContext context;
    const int frameState = context.saveCount();
    root.paint(context);
    expect(context.saveCount() == frameState,
           "A child paint-state leak must be restored at the container boundary");
}

void testStructuralPaintStateIsolation()
{
    using namespace wui::ui;

    wui::State<bool> oldBranch{true};
    wui::State<bool> newBranch{false};
    wui::Container root;
    root.appendChild(asNode(If(oldBranch).then([] { return std::make_unique<LeakyPaintStateNode>(); })));
    root.appendChild(asNode(If(newBranch).then([] { return std::make_unique<DummyNode>(); })));

    wui::PaintContext context;
    root.layout({0.0f, 0.0f, 100.0f, 100.0f});
    root.paint(context);
    expect(context.saveCount() == 1, "Mounted branches must not leak paint state after a frame");

    oldBranch.set(false);
    newBranch.set(true);
    wui::flushStructuralUpdates();
    root.layout({0.0f, 0.0f, 100.0f, 100.0f});
    root.paint(context);
    expect(context.saveCount() == 1,
           "A replacement structural branch must start from an unclipped paint state");
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
    wui::Node* observedPage = nullptr;
    navigator.setOnChange([&observedPage](wui::Node* page) { observedPage = page; });
    navigator.setRoot("home", std::make_unique<DummyNode>());
    expect(observedPage == navigator.current(), "Navigator should publish its active root page");
    navigator.push("settings", std::make_unique<DummyNode>());

    expect(navigator.size() == 2, "Navigator should hold root and pushed page");
    expect(navigator.canPop(), "Navigator should allow popping when more than one page exists");
    expect(navigator.currentKey() != nullptr && *navigator.currentKey() == "settings", "Navigator should expose current page key");

    auto popped = navigator.pop();
    expect(static_cast<bool>(popped), "Navigator should return the popped page");
    expect(navigator.size() == 1, "Navigator should keep root page after pop");
    expect(observedPage == navigator.current(), "Navigator should publish the revealed page after pop");
}

void testNavigatorPageRetention()
{
    wui::Navigator navigator;
    int disposedFactoryCalls = 0;
    navigator.setRoot("disposable", [&disposedFactoryCalls] {
        ++disposedFactoryCalls;
        return std::make_unique<DummyNode>();
    }, wui::PageRetention::DisposeOnHide);
    expect(navigator.current() != nullptr, "Factory root should create its initial page");

    auto keepAlive = std::make_unique<DummyNode>();
    auto* keepAliveInstance = keepAlive.get();
    navigator.push("kept", std::move(keepAlive));
    expect(navigator.pages().front().content == nullptr,
           "DisposeOnHide should release a page when another page covers it");
    expect(navigator.current() == keepAliveInstance, "KeepAlive page should remain the active instance");

    auto popped = navigator.pop();
    expect(popped.get() == keepAliveInstance, "Pop should return the active KeepAlive page");
    expect(navigator.current() != nullptr, "Returning to a disposed page should create a fresh instance");
    expect(disposedFactoryCalls == 2, "DisposeOnHide factory should run once per visible instance");

    bool rejectedOneShotDisposable = false;
    try {
        navigator.push("invalid", std::make_unique<DummyNode>(), wui::PageRetention::DisposeOnHide);
    } catch (const std::invalid_argument&) {
        rejectedOneShotDisposable = true;
    }
    expect(rejectedOneShotDisposable, "DisposeOnHide should require a recreatable page factory");
}

void testPaintContextScaleFactor()
{
    wui::PaintContext context(2.0f);
    expect(context.scaleFactor() == 2.0f, "PaintContext should preserve a valid DPR");
    context.setScaleFactor(0.0f);
    expect(context.scaleFactor() == 1.0f, "PaintContext should normalize invalid DPR to 1");
}

void testImageSourcesAreInternedAcrossRebuiltNodes()
{
    const std::vector<unsigned char> pixels{
        255, 0, 0, 255,
        0, 255, 0, 255,
    };
    wui::Image first(pixels, 2, 1);
    // Declarative rebuilding normally creates a new input vector each time.
    wui::Image rebuilt(std::vector<unsigned char>(pixels), 2, 1);
    expect(first.imageSource() == rebuilt.imageSource(),
           "Equivalent image data should share an immutable interned resource");

    wui::Image different({255, 0, 0, 255, 0, 0, 255, 255}, 2, 1);
    expect(first.imageSource() != different.imageSource(),
           "Different image bytes must not share an image resource");

    const auto reusable = first.imageSource();
    wui::Image fromReusable(reusable);
    expect(fromReusable.imageSource() == reusable,
           "Image should accept a reusable immutable image source without copying it");
}

void testAnimationUsesElapsedTime()
{
    float value = -1.0f;
    bool completed = false;
    wui::Animation animation(1.0f, [&value](float next) { value = next; });
    animation.onComplete([&completed] { completed = true; });

    expect(animation.tick(0.25f), "Animation should remain active before its duration elapses");
    expect(value == 0.25f, "Animation progress should use the supplied elapsed time");
    expect(!animation.tick(0.75f), "Animation should finish at its exact duration");
    expect(value == 1.0f && completed, "Animation should publish its final value and completion");
}

void testTextInputModel()
{
    wui::TextEditingController model;
    model.setValue({"abc", {1, 3}, {}});
    model.commit("z");

    expect(model.value().text == "az", "EditingValue should replace the active selection on commit");
    expect(model.selection().start == 2 && model.selection().end == 2, "Caret should collapse at the end of committed text");

    model.setText("alpha beta");
    model.moveToEnd();
    model.moveCaret(-1, true);
    expect(model.selectedText() == "a", "Shift+Arrow should extend the selection from its anchor");
    model.moveToStart();
    expect(model.selection().empty(), "Home should collapse a selection when Shift is not held");
    model.moveToEnd(true);
    expect(model.selectedText() == "alpha beta", "Shift+End should select to the editing boundary");

    model.setText("alpha beta gamma");
    model.moveToEnd();
    model.backspace(true);
    expect(model.text() == "alpha beta ", "Ctrl+Backspace should delete the previous word");
    expect(model.undo(), "Text edits should be undoable");
    expect(model.text() == "alpha beta gamma", "Undo should restore the prior EditingValue");
    expect(model.redo(), "Undone text edits should be redoable");
    expect(model.text() == "alpha beta ", "Redo should restore the edited value");

    model.setText("alpha beta");
    model.moveToStart();
    model.deleteForward(true);
    expect(model.text() == " beta", "Ctrl+Delete should delete the next word");
}

class TestClipboard final : public wui::Clipboard {
public:
    void setText(std::string_view text) override { text_ = text; }
    [[nodiscard]] std::string getText() const override { return text_; }
    [[nodiscard]] bool hasText() const override { return !text_.empty(); }
private:
    std::string text_;
};

void testTextInputPointerSelectionAndClipboard()
{
    auto input = std::make_unique<wui::TextInput>();
    input->text("abcd");
    input->layout({0.0f, 0.0f, 160.0f, 32.0f});
    const auto characterWidth = wui::theme().typography.body * 0.56f;
    const auto xAt = [characterWidth](std::size_t index) {
        return wui::theme().controls.horizontalPadding + static_cast<float>(index) * characterWidth;
    };

    wui::FocusManager focusManager;
    wui::InputRouter router(&focusManager);
    router.setRoot(input.get());
    const wui::PointerEvent down{0, wui::PointerType::Mouse, wui::PointerAction::Down, wui::MouseButton::Left, {xAt(1), 12.0f}, 0};
    const wui::PointerEvent move{0, wui::PointerType::Mouse, wui::PointerAction::Move, wui::MouseButton::None, {xAt(3), 12.0f}, 0};
    const wui::PointerEvent up{0, wui::PointerType::Mouse, wui::PointerAction::Up, wui::MouseButton::Left, {xAt(3), 12.0f}, 0};
    expect(router.dispatchPointer(down) && router.dispatchPointer(move) && router.dispatchPointer(up),
           "TextInput should capture and handle a pointer selection gesture");
    expect(input->controller().selectedText() == "bc", "Pointer drag should select the text between down and up positions");

    TestClipboard clipboard;
    expect(input->copySelection(clipboard) && clipboard.getText() == "bc", "Copy should publish the active selection to the clipboard");
    expect(input->cutSelection(clipboard) && input->controller().text() == "ad", "Cut should copy and delete the active selection");
    clipboard.setText("BC");
    expect(input->paste(clipboard) && input->controller().text() == "aBCd", "Paste should replace the active caret selection");

    input->text("alpha beta");
    input->controller().moveToEnd();
    expect(input->onKeyEvent({0, wui::KeyAction::Down, 268, 0, false}), "TextInput should accept GLFW Home");
    expect(input->controller().selection().start == 0, "GLFW Home should move the caret to the start");
    expect(input->onKeyEvent({0, wui::KeyAction::Down, 269, wui::KeyModifierShift, false}), "TextInput should accept GLFW Shift+End");
    expect(input->controller().selectedText() == "alpha beta", "GLFW Shift+End should extend the selection");
    input->controller().moveToEnd();
    expect(input->onKeyEvent({0, wui::KeyAction::Down, 259, wui::KeyModifierControl, false}), "TextInput should accept GLFW Ctrl+Backspace");
    expect(input->controller().text() == "alpha ", "GLFW Ctrl+Backspace should delete the preceding word");
}

class VariableWidthTextMeasurer final : public wui::TextMeasurer {
public:
    [[nodiscard]] wui::TextExtents measureText(const std::string& text, float fontSize) const override
    {
        float width = 0.0f;
        for (const char character : text) {
            width += character == 'W' ? fontSize : fontSize * 0.25f;
        }
        return {width, fontSize * 1.25f, fontSize * 0.8f, fontSize * 0.2f};
    }
};

void testTextInputUsesMeasuredGlyphPositions()
{
    VariableWidthTextMeasurer measurer;
    wui::TextMeasurer* const previousMeasurer = wui::textMeasurer();
    wui::setTextMeasurer(&measurer);

    auto input = std::make_unique<wui::TextInput>();
    input->text("Wi");
    input->layout({0.0f, 0.0f, 160.0f, 32.0f});
    wui::FocusManager focusManager;
    wui::InputRouter router(&focusManager);
    router.setRoot(input.get());

    // The old fixed-width model would place this x coordinate after two
    // characters. The renderer gives W a full em and i a quarter-em, so it
    // lies unambiguously within W and must place the caret after its first
    // glyph instead.
    const wui::PointerEvent down{0, wui::PointerType::Mouse, wui::PointerAction::Down,
                                 wui::MouseButton::Left,
                                 {wui::theme().controls.horizontalPadding + 12.0f, 12.0f}, 0};
    expect(router.dispatchPointer(down), "TextInput should accept a measured caret hit test");
    expect(input->controller().selection().start == 1,
           "TextInput caret hit testing must follow measured variable-width glyph advances");

    wui::setTextMeasurer(previousMeasurer);
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

void testPointerCaptureTargetBubbleRoutingContract()
{
    std::vector<std::string> trace;
    auto root = std::make_unique<EventProbe>("root", trace);
    auto parent = std::make_unique<EventProbe>("parent", trace);
    auto target = std::make_unique<EventProbe>("target", trace);
    auto* rootRaw = root.get();
    auto* parentRaw = parent.get();
    auto* targetRaw = target.get();
    targetRaw->result_ = wui::EventResult::Handled;
    parentRaw->appendChild(std::move(target));
    rootRaw->appendChild(std::move(parent));
    rootRaw->layout({0.0f, 0.0f, 100.0f, 100.0f});
    parentRaw->layout({0.0f, 0.0f, 100.0f, 100.0f});
    targetRaw->layout({0.0f, 0.0f, 100.0f, 100.0f});

    wui::FocusManager focus;
    wui::InputRouter router(&focus);
    router.setRoot(rootRaw);
    const wui::PointerEvent down{0, wui::PointerType::Mouse, wui::PointerAction::Down,
                                 wui::MouseButton::Left, {10.0f, 10.0f}, 0};
    const wui::PointerEvent move{0, wui::PointerType::Mouse, wui::PointerAction::Move,
                                 wui::MouseButton::None, {10.0f, 10.0f}, 0};
    (void)router.dispatchPointer(move); // Establish hover; Enter is not part of the Down route contract.
    trace.clear();
    expect(router.dispatchPointer(down), "Target handler should mark the routed pointer event handled");
    expect(trace == std::vector<std::string>{"root:capture", "parent:capture", "target:target", "parent:bubble", "root:bubble"},
           "Pointer routing must follow Capture -> Target -> Bubble exactly once per path node");

    router.releasePointer();
    trace.clear();
    parentRaw->stopWithContext_ = true;
    expect(router.dispatchPointer(down), "Stopping propagation should count as handled");
    expect(trace == std::vector<std::string>{"root:capture", "parent:capture"},
           "Capture stopPropagation must prevent Target and Bubble delivery");

    parentRaw->stopWithContext_ = false;
    trace.clear();
    parentRaw->focusWithContext_ = true;
    parentRaw->focusTarget_ = parentRaw;
    expect(router.dispatchPointer(down), "Context focus request should handle the event");
    expect(focus.focused() == parentRaw, "EventContext::requestFocus should override default target focus");

    router.releasePointer();
    parentRaw->focusWithContext_ = false;
    parentRaw->captureWithContext_ = true;
    trace.clear();
    expect(router.dispatchPointer(down), "Context pointer capture request should handle the event");
    expect(router.capturedPointer() == parentRaw,
           "EventContext::capturePointer should delegate to the router capture API without storing capture in the context");

    parentRaw->captureWithContext_ = false;
    parentRaw->stopWithContext_ = true;
    const wui::PointerEvent upOutside{0, wui::PointerType::Mouse, wui::PointerAction::Up,
                                      wui::MouseButton::Left, {150.0f, 150.0f}, 0};
    expect(router.dispatchPointer(upOutside), "A stopped captured Up should still report handled");
    expect(router.capturedPointer() == nullptr,
           "Stopping propagation must not retain pointer capture after the gesture's Up event");
}

void testCheckboxPointerKeyboardBindingAndDisabledState()
{
    wui::State<bool> value{false};
    auto checkbox = std::make_unique<wui::Checkbox>("Receive updates");
    int changes = 0;
    checkbox->bind(value).onChange([&changes](bool) { ++changes; });
    checkbox->layout({0.0f, 0.0f, 180.0f, 24.0f});
    wui::FocusManager focus;
    wui::InputRouter router(&focus);
    router.setRoot(checkbox.get());
    const wui::PointerEvent down{0, wui::PointerType::Mouse, wui::PointerAction::Down, wui::MouseButton::Left, {10.0f, 10.0f}, 0};
    const wui::PointerEvent up{0, wui::PointerType::Mouse, wui::PointerAction::Up, wui::MouseButton::Left, {10.0f, 10.0f}, 0};
    expect(router.dispatchPointer(down) && router.dispatchPointer(up), "Checkbox should consume a pointer activation");
    expect(value.get() && checkbox->isChecked() && changes == 1, "Checkbox pointer activation should update its bound State");
    expect(router.dispatchKey({0, wui::KeyAction::Down, 32, 0, false}), "Focused Checkbox should consume Space");
    expect(!value.get() && changes == 2, "Space should toggle the bound Checkbox value");
    expect(!router.dispatchKey({0, wui::KeyAction::Down, 13, 0, false}),
           "Focused Checkbox must not consume Enter");
    expect(!value.get() && changes == 2, "Enter must not toggle the bound Checkbox value");
    checkbox->setEnabled(false);
    expect(!router.dispatchKey({0, wui::KeyAction::Down, 32, 0, false}), "Disabled Checkbox should not consume keyboard activation");
    expect(!value.get() && changes == 2, "Disabled Checkbox should not change its bound State");

    std::unique_ptr<wui::Node> declarative = wui::ui::Checkbox("Builder bound").bind(value).enabled(true);
    auto* builderCheckbox = dynamic_cast<wui::Checkbox*>(declarative.get());
    expect(builderCheckbox != nullptr && !builderCheckbox->isChecked(),
           "Checkbox builder should retain the strong State<bool> binding");

    wui::Slider slider(0.0f, 10.0f, 6.0f);
    slider.layout({0.0f, 0.0f, 160.0f, 32.0f});
    router.setRoot(&slider);
    focus.setFocused(&slider);
    expect(!router.dispatchKey({0, wui::KeyAction::Down, 32, 0, false}) && slider.value() == 6.0f,
           "Slider Space must not synthesize a pointer click or change its value");
    expect(!router.dispatchKey({0, wui::KeyAction::Down, 13, 0, false}) && slider.value() == 6.0f,
           "Slider Enter must not synthesize a pointer click or change its value");

    int buttonInvocations = 0;
    wui::Button button("Apply");
    button.onClick([&buttonInvocations] { ++buttonInvocations; });
    router.setRoot(&button);
    focus.setFocused(&button);
    expect(router.dispatchKey({0, wui::KeyAction::Down, 13, 0, false}) && buttonInvocations == 1,
           "Button Enter must invoke through its accessibility action");
    expect(router.dispatchKey({0, wui::KeyAction::Down, 32, 0, false}) && buttonInvocations == 2,
           "Button Space must invoke through its accessibility action");
}

void testOverlayHitTestingAndRouting()
{
    wui::OverlayHost overlays;
    auto overlayButton = std::make_unique<wui::Button>("Overlay");
    auto* rawButton = overlayButton.get();
    bool clicked = false;
    overlayButton->onClick([&clicked] { clicked = true; });
    (void)overlays.show(std::move(overlayButton));
    overlays.layout({0.0f, 0.0f, 160.0f, 40.0f});

    wui::FocusManager focus;
    wui::InputRouter router(&focus);
    const wui::PointerEvent down{0, wui::PointerType::Mouse, wui::PointerAction::Down,
                                 wui::MouseButton::Left, {10.0f, 10.0f}, 0};
    const wui::PointerEvent up{0, wui::PointerType::Mouse, wui::PointerAction::Up,
                               wui::MouseButton::Left, {10.0f, 10.0f}, 0};
    expect(overlays.hitTest(down.position) == rawButton, "Top overlay should win hit testing");
    expect(router.dispatchPointerTo(overlays.hitTest(down.position), down), "Overlay pointer down should route");
    expect(router.dispatchPointerTo(overlays.hitTest(up.position), up), "Overlay pointer up should route");
    expect(clicked, "Overlay button should receive routed click");
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
    const auto& buttonBounds = button->bounds();
    const wui::PointF buttonCenter{buttonBounds.x + buttonBounds.width * 0.5f,
                                   buttonBounds.y + buttonBounds.height * 0.5f};
    const wui::PointerEvent down{0, wui::PointerType::Mouse, wui::PointerAction::Down, wui::MouseButton::Left, buttonCenter, 0};
    const wui::PointerEvent up{0, wui::PointerType::Mouse, wui::PointerAction::Up, wui::MouseButton::Left, buttonCenter, 0};
    (void)button->onPointerEvent(down);
    (void)button->onPointerEvent(up);

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
    (void)sum.subscribe([&observed](const int& value) { observed = value; });
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
    expect(wui::theme().colors.accent.r == 15, "default Fluent accent should be restored");
    expect(wui::theme().controls.height == 32.0f
               && wui::theme().controls.compactHeight == 24.0f
               && wui::theme().typography.body == wui::theme().typography.body2.size
               && wui::theme().typography.bodyLineHeight >= wui::theme().typography.body,
           "default Fluent aliases should remain aligned with semantic control and typography tokens");
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

void testDestroyedStructuralNodeSkipsQueuedUpdate()
{
    using namespace wui::ui;

    wui::State<bool> show{false};
    int factoryCalls = 0;
    std::unique_ptr<wui::Node> node = If(show).then([&] {
        ++factoryCalls;
        return Text("Advanced");
    });
    show.set(true);
    node.reset();
    wui::flushStructuralUpdates();
    expect(factoryCalls == 0, "Queued structural work must not access a node destroyed before the frame boundary");
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

void testKeyedForEachRetainsUnchangedRows()
{
    using namespace wui::ui;
    struct Item {
        int id{0};
        std::string label;
        [[nodiscard]] bool operator==(const Item& other) const noexcept
        {
            return id == other.id && label == other.label;
        }
        [[nodiscard]] bool operator!=(const Item& other) const noexcept { return !(*this == other); }
    };

    wui::State<std::vector<Item>> items{{{1, "one"}, {2, "two"}}};
    int built = 0;
    std::unique_ptr<wui::Node> node = KeyedForEach<Item>(
        items,
        [](const Item& item) { return std::to_string(item.id); },
        [&built](const Item& item) {
            ++built;
            return Text(item.label);
        });
    auto* list = dynamic_cast<wui::ForEachNode*>(node.get());
    expect(list != nullptr && list->children().size() == 2, "KeyedForEach should build its initial rows");
    wui::Node* one = list->children()[0].get();
    wui::Node* two = list->children()[1].get();

    items.set({{1, "one"}, {2, "TWO"}, {3, "three"}});
    wui::flushStructuralUpdates();
    expect(list->children().size() == 3, "KeyedForEach should add only the new row");
    expect(list->children()[0].get() == one, "An unchanged keyed row must retain its node");
    const auto* refreshed = dynamic_cast<const wui::Text*>(list->children()[1].get());
    expect(refreshed != nullptr && refreshed->value() == "TWO",
           "A changed keyed row must refresh its rendered value");
    expect(built == 4, "Only changed and inserted keyed rows should be rebuilt");

    wui::Node* three = list->children()[2].get();
    items.set({{3, "three"}, {1, "one"}, {2, "TWO"}});
    wui::flushStructuralUpdates();
    expect(list->children()[0].get() == three && list->children()[1].get() == one,
           "Keyed reordering must retain row nodes without reconstruction");
    expect(built == 4, "Reordering stable keys must not rebuild rows");
}

void testListActionCanRemoveItsOwnRow()
{
    using namespace wui::ui;

    wui::State<std::vector<int>> items{{1, 2}};
    std::unique_ptr<wui::Node> node = ForEach<int>(items, [&items](const int& id) {
        return Button("Delete").onClick([&items, id] {
            auto next = items.get();
            next.erase(std::remove(next.begin(), next.end(), id), next.end());
            items.set(next);
        });
    });
    auto* list = dynamic_cast<wui::ForEachNode*>(node.get());
    expect(list != nullptr && list->children().size() == 2, "List should build delete buttons");

    auto* first = dynamic_cast<wui::Button*>(list->children().front().get());
    expect(first != nullptr, "First list child should be a button");
    first->layout({0.0f, 0.0f, 80.0f, 32.0f});
    const wui::PointerEvent down{0, wui::PointerType::Mouse, wui::PointerAction::Down,
                                 wui::MouseButton::Left, {10.0f, 10.0f}, 0};
    const wui::PointerEvent up{0, wui::PointerType::Mouse, wui::PointerAction::Up,
                               wui::MouseButton::Left, {10.0f, 10.0f}, 0};
    (void)first->onPointerEvent(down);
    (void)first->onPointerEvent(up);

    expect(items.get() == std::vector<int>{2}, "Click handler should remove its own item");
    expect(list->children().size() == 2, "Structural mutation must wait until the handler returns");
    wui::flushStructuralUpdates();
    expect(list->children().size() == 1, "List should rebuild safely at the frame boundary");
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

void testFocusedControlsPaintAFluentFocusRing()
{
    auto paintCommandCount = [](wui::Node& node) {
        wui::PaintContext context;
        node.paint(context);
        return context.paintStats().commandCount;
    };

    wui::Button button("Save");
    button.layout({10.0f, 10.0f, 80.0f, 32.0f});
    const auto buttonRest = paintCommandCount(button);
    button.setVisualState(wui::ControlVisualState::Focused, true);
    expect(paintCommandCount(button) == buttonRest + 2,
           "Focused Button must paint the Fluent outer and inner focus strokes");

    wui::Checkbox checkbox("Complete");
    checkbox.layout({10.0f, 10.0f, 120.0f, 24.0f});
    const auto checkboxRest = paintCommandCount(checkbox);
    checkbox.setVisualState(wui::ControlVisualState::Focused, true);
    expect(paintCommandCount(checkbox) == checkboxRest + 2,
           "Focused Checkbox must paint the Fluent outer and inner focus strokes");

    wui::IconButton icon(".", "More actions");
    icon.layout({10.0f, 10.0f, 32.0f, 32.0f});
    const auto iconRest = paintCommandCount(icon);
    icon.setVisualState(wui::ControlVisualState::Focused, true);
    expect(paintCommandCount(icon) == iconRest + 2,
           "Focused IconButton must paint the Fluent outer and inner focus strokes");
}

void testKeyboardFocusTraversalAndControlActivation()
{
    auto root = std::make_unique<wui::Column>();
    auto first = std::make_unique<wui::Button>("First");
    auto disabled = std::make_unique<wui::Button>("Disabled");
    auto last = std::make_unique<wui::Button>("Last");
    auto* firstRaw = first.get();
    auto* disabledRaw = disabled.get();
    auto* lastRaw = last.get();
    disabled->setEnabled(false);

    int firstClicks = 0;
    int disabledClicks = 0;
    int lastClicks = 0;
    first->onClick([&] { ++firstClicks; });
    disabled->onClick([&] { ++disabledClicks; });
    last->onClick([&] { ++lastClicks; });
    root->appendChild(std::move(first));
    root->appendChild(std::move(disabled));
    root->appendChild(std::move(last));
    root->layout({0.0f, 0.0f, 200.0f, 120.0f});

    wui::FocusManager focus;
    wui::InputRouter router(&focus);
    router.setRoot(root.get());
    const wui::KeyEvent tab{0, wui::KeyAction::Down, 9, 0, false};
    const wui::KeyEvent shiftTab{0, wui::KeyAction::Down, 9, wui::KeyModifierShift, false};
    const wui::KeyEvent enter{0, wui::KeyAction::Down, 13, 0, false};
    const wui::KeyEvent space{0, wui::KeyAction::Down, 32, 0, false};

    expect(router.dispatchKey(tab), "Tab should focus the first enabled control when no control is focused");
    expect(focus.focused() == firstRaw, "Tab order should begin with the first enabled control");
    expect(router.dispatchKey(space) && firstClicks == 1,
           "Space should activate the focused control through the keyboard route");
    expect(router.dispatchKey(tab), "Tab should advance focus");
    expect(focus.focused() == lastRaw, "Tab must skip disabled controls");
    expect(router.dispatchKey(enter) && lastClicks == 1,
           "Enter should activate the focused control through the keyboard route");
    expect(router.dispatchKey(tab), "Tab should wrap at the end of the focus order");
    expect(focus.focused() == firstRaw, "Forward traversal should wrap to the first control");
    expect(router.dispatchKey(shiftTab), "Shift+Tab should traverse backwards");
    expect(focus.focused() == lastRaw, "Reverse traversal should wrap to the last control");

    focus.setFocused(disabledRaw);
    expect(!router.dispatchKey(enter), "Disabled controls must reject keyboard activation");
    expect(disabledClicks == 0 && focus.focused() == nullptr,
           "Disabled keyboard targets must not activate and should be defocused");
}

} // namespace

int main()
{
    try {
    testInvalidationReachesTheRootAndPaintsAfterLayout();
    testContainerPaintStateIsolation();
    testStructuralPaintStateIsolation();
    testState();
    testNavigator();
    testNavigatorPageRetention();
    testTextInputModel();
    testTextInputPointerSelectionAndClipboard();
    testTextInputUsesMeasuredGlyphPositions();
    testInputRouterAndButton();
    testPointerCaptureTargetBubbleRoutingContract();
    testCheckboxPointerKeyboardBindingAndDisabledState();
    testOverlayHitTestingAndRouting();
    testDeclarativeBuilderAndCounter();
    testReactiveText();
    testComputed();
    testTheme();
    testStructuralIf();
    testDestroyedStructuralNodeSkipsQueuedUpdate();
    testStructuralForEach();
    testKeyedForEachRetainsUnchangedRows();
    testListActionCanRemoveItsOwnRow();
    testPluggableTextMeasurement();
    testPaintContextScaleFactor();
    testImageSourcesAreInternedAcrossRebuiltNodes();
    testAnimationUsesElapsedTime();
    testLayoutFlexAndAlign();
    testTextInputRouting();
    testFocusedControlsPaintAFluentFocusRing();
    testKeyboardFocusTraversalAndControlActivation();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "WhatsUISmokeTests failed: %s\\n", error.what());
        return 1;
    }
    return 0;
}
