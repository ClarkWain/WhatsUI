#include <cmath>
#include <stdexcept>
#include <string>

#include "wui/basic_controls.h"
#include "wui/overlays.h"
#include "wui/paint_context.h"
#include "wui/runtime.h"
#include "wui/declarative.h"

namespace {

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

wui::PointerEvent pointer(wui::PointerAction action, float x = 4.0f)
{
    return {0, wui::PointerType::Mouse, action, wui::MouseButton::Left, {x, 8.0f}};
}

void testRadioSelectsWithoutTogglingOff()
{
    wui::RadioNode radio("Daily");
    radio.layout({0, 0, 120, 24});
    int notifications = 0;
    radio.onChange([&](bool selected) { expect(selected, "Radio must notify true when selected"); ++notifications; });
    expect(radio.onPointerEvent(pointer(wui::PointerAction::Down)), "Radio should handle primary down");
    radio.onPointerEvent(pointer(wui::PointerAction::Up));
    expect(radio.isSelected() && notifications == 1, "Radio should select once on click");
    radio.onPointerEvent(pointer(wui::PointerAction::Down));
    radio.onPointerEvent(pointer(wui::PointerAction::Up));
    expect(radio.isSelected() && notifications == 1, "Selected radio must not toggle off or duplicate notification");
}

void testSwitchBindingAndKeyboard()
{
    wui::State<bool> enabled{false};
    wui::SwitchNode toggle("Notifications");
    toggle.bind(enabled);
    int notifications = 0;
    toggle.onChange([&](bool value) { expect(value, "Space should turn switch on"); ++notifications; });
    expect(toggle.onKeyEvent({0, wui::KeyAction::Down, 32}), "Space should activate a switch");
    expect(enabled.get() && toggle.isOn() && notifications == 1, "Bound switch should update its state and callback");
    enabled.set(false);
    expect(!toggle.isOn(), "External state update should be visible through switch binding");
}

void testSliderClampsSnapsAndSupportsKeys()
{
    wui::SliderNode slider(0.0f, 10.0f, 0.0f);
    slider.step(2.0f);
    slider.layout({0, 0, 116, 32});
    slider.setValue(3.1f);
    expect(std::abs(slider.value() - 4.0f) < 0.001f, "Slider should snap direct values to its step");
    slider.onKeyEvent({0, wui::KeyAction::Down, 39});
    expect(std::abs(slider.value() - 6.0f) < 0.001f, "Right should advance slider by one step");
    slider.onKeyEvent({0, wui::KeyAction::Down, 36});
    expect(slider.value() == 0.0f, "Home should set slider to minimum");
    slider.onKeyEvent({0, wui::KeyAction::Down, 35});
    expect(slider.value() == 10.0f, "End should set slider to maximum");
    slider.onPointerEvent(pointer(wui::PointerAction::Down, 2.0f));
    slider.onPointerEvent(pointer(wui::PointerAction::Move, 114.0f));
    slider.onPointerEvent(pointer(wui::PointerAction::Up, 114.0f));
    expect(slider.value() == 10.0f, "Captured slider drag should reach maximum");
}

void testProgressAndDividerRanges()
{
    wui::ProgressBarNode progress(0.0f, 1.0f, 3.0f);
    expect(progress.value() == 1.0f, "Progress should clamp initial value");
    progress.setValue(-2.0f);
    expect(progress.value() == 0.0f, "Progress should clamp updates");
    wui::DividerNode horizontal;
    wui::DividerNode vertical(wui::DividerOrientation::Vertical);
    expect(horizontal.measure({}).height == 1.0f, "Horizontal divider should reserve thickness vertically");
    expect(vertical.measure({}).width == 1.0f, "Vertical divider should reserve thickness horizontally");
}

void testDeclarativeBuildersRemainMoveOnly()
{
    auto root = wui::Column().children(
        wui::Radio("Today").selected(true),
        wui::Switch("Remind me").on(true),
        wui::Slider(0.0f, 10.0f, 3.0f).step(1.0f),
        wui::ProgressBar(0.0f, 1.0f, 0.5f),
        wui::Divider());
    expect(root.node() != nullptr, "Fluent controls must participate in the declarative builder API");
}

void testFluentButtonAppearanceSizeAndShape()
{
    wui::ButtonNode button("Save");
    button.setAppearance(wui::ButtonAppearance::Outline);
    button.setSize(wui::ButtonSize::Small);
    button.setShape(wui::ButtonShape::Square);
    expect(button.appearance() == wui::ButtonAppearance::Outline &&
               button.size() == wui::ButtonSize::Small &&
               button.shape() == wui::ButtonShape::Square,
           "Button must retain Fluent appearance, size and shape choices");
    expect(button.measure({}).height == 24.0f,
           "Fluent small buttons must reserve a 24 DIP hit target");
    button.setSize(wui::ButtonSize::Medium);
    expect(button.measure({}).height == 32.0f,
           "Fluent medium buttons must reserve a 32 DIP hit target");
    button.setSize(wui::ButtonSize::Large);
    expect(button.measure({}).height == 40.0f,
           "Fluent large buttons must reserve a 40 DIP hit target");
    button.setAppearance(wui::ButtonAppearance::Outline);
    expect(button.appearance() == wui::ButtonAppearance::Outline,
           "Legacy Ghost must retain its bordered visual contract through Outline");
}

void testFluentLabelAssociatesInputAndUsesOfficialScale()
{
    wui::TextFieldNode input("Placeholder only");
    wui::LabelNode label("Task title");
    label.setForControl(&input);
    expect(input.accessibleLabel() == "Task title" && label.forControl() == &input,
           "A Fluent Label must provide the input's accessible name instead of relying on placeholder text");
    label.setSize(wui::LabelSize::Small);
    expect(label.measure({}).height == 16.0f, "Small Fluent labels must use caption1's 16 DIP line height");
    label.setSize(wui::LabelSize::Medium);
    expect(label.measure({}).height == 20.0f, "Medium Fluent labels must use body1Strong's 20 DIP line height");
    label.setSize(wui::LabelSize::Large);
    expect(label.measure({}).height == 22.0f, "Large Fluent labels must use subtitle2's 22 DIP line height");
}

void testFluentCardSizingAndAppearance()
{
    wui::CardNode card;
    card.setAppearance(wui::CardAppearance::Outline);
    card.setSize(wui::CardSize::Large);
    card.child(std::make_unique<wui::SpacerNode>(wui::SizeF{40.0f, 20.0f}));
    const auto size = card.measure({});
    expect(card.appearance() == wui::CardAppearance::Outline && card.size() == wui::CardSize::Large,
           "Card must retain its Fluent appearance and size");
    expect(size.width == 72.0f && size.height == 52.0f,
           "Large cards must reserve their documented 16 DIP padding on all sides");
}

void testFluentCardSlotsLayOutContentAndActions()
{
    wui::CardHeaderNode header("Task summary", "Three items due today");
    header.media(std::make_unique<wui::SpacerNode>(wui::SizeF{24.0f, 24.0f}));
    header.action(std::make_unique<wui::ButtonNode>("More"));
    const auto headerSize = header.measure({0.0f, 240.0f, 0.0f, 100.0f});
    expect(headerSize.height >= 32.0f && headerSize.width > 100.0f,
           "CardHeader must reserve two text rows and its trailing action slot");
    header.layout({0.0f, 0.0f, 240.0f, headerSize.height});
    expect(header.children().size() == 2 && header.children()[0]->bounds().x < header.children()[1]->bounds().x,
           "CardHeader media and trailing action must occupy independent slots");
    wui::CardFooterNode footer;
    footer.child(std::make_unique<wui::TextNode>("Updated now"));
    footer.child(std::make_unique<wui::ButtonNode>("Open"));
    footer.layout({0.0f, 0.0f, 240.0f, 32.0f});
    expect(footer.children().size() == 2 && footer.children()[1]->bounds().x > footer.children()[0]->bounds().x,
           "CardFooter must lay out its action slots in stable left-to-right order");
}

void testFluentCardOrientationAndSelection()
{
    wui::CardNode card;
    card.setOrientation(wui::CardOrientation::Horizontal);
    card.selectable();
    card.child(std::make_unique<wui::SpacerNode>(wui::SizeF{20.0f, 12.0f}));
    card.child(std::make_unique<wui::SpacerNode>(wui::SizeF{30.0f, 12.0f}));
    card.layout({0.0f, 0.0f, 100.0f, 40.0f});
    expect(card.children()[1]->bounds().x > card.children()[0]->bounds().x,
           "Horizontal Card must lay out children side-by-side rather than overlap them");
    expect(card.onKeyEvent({0, wui::KeyAction::Down, 32}) && card.isSelected(),
           "Selectable Card must expose a keyboard-operable selected state");
}

void testCardPreviewUsesGeometricRoundedClip()
{
    wui::CardPreviewNode preview;
    preview.setHeight(48.0f);
    preview.child(std::make_unique<wui::SpacerNode>(wui::SizeF{80.0f, 48.0f}));
    preview.layout({0.0f, 0.0f, 80.0f, 48.0f});
    wui::PaintContext context;
    preview.paint(context);
    expect(context.paintStats().clipRectCalls == 1,
           "CardPreview must establish a real clip before painting its media child");
}

void testFluentTextAreaUsesEditingControllerAndVisualLines()
{
    wui::TextAreaNode area("Describe the task");
    area.setRows(3);
    area.layout({0.0f, 0.0f, 180.0f, 80.0f});
    expect(area.isMultiline() && area.rows() == 3 && area.measure({}).height >= 60.0f,
           "TextArea must reserve multiple Fluent text lines instead of masquerading as a single-line input");
    expect(area.onTextInput({0, "first"}), "TextArea must accept native text commits");
    expect(area.onKeyEvent({0, wui::KeyAction::Down, 13}), "Enter must create a line break in TextArea");
    expect(area.onTextInput({0, "second"}), "TextArea must accept text after an explicit line break");
    expect(area.controller().text() == "first\nsecond", "TextArea must retain multi-line editing state in its controller");
    expect(area.onKeyEvent({0, wui::KeyAction::Down, 265}), "Up must navigate visual TextArea lines");
    expect(area.controller().selection().end < area.controller().text().size(),
           "Vertical navigation must move the TextArea caret onto the preceding visual line");
}

void testFluentToggleButtonOwnsButtonStyleToggleState()
{
    wui::State<bool> pressed{false};
    wui::ToggleButtonNode toggle("Bold");
    toggle.bind(pressed);
    toggle.setSize(wui::ButtonSize::Small);
    toggle.setShape(wui::ButtonShape::Rounded);
    toggle.setAppearance(wui::ButtonAppearance::Secondary);
    toggle.setIcon(wui::IconName::Edit);
    int notifications = 0;
    toggle.onChange([&](bool value) { expect(value, "ToggleButton keyboard activation must select the control"); ++notifications; });
    expect(toggle.measure({}).height == 24.0f, "ToggleButton must share Fluent Button size tokens");
    expect(toggle.appearance() == wui::ButtonAppearance::Secondary &&
               toggle.icon() == wui::IconName::Edit,
           "ToggleButton must expose the Figma Button appearance and semantic icon contract");
    expect(toggle.onKeyEvent({0, wui::KeyAction::Down, 32}) && pressed.get() && toggle.isChecked(),
           "ToggleButton must toggle through its bound source of truth");
    expect(notifications == 1 && toggle.accessibilityActions().toggle,
           "ToggleButton must expose an actual toggle action, not an invoke-only button");
}

void testFluentCompoundButtonUsesOneCommandSurface()
{
    wui::CompoundButtonNode button("Create list", "Organize related tasks");
    button.setSize(wui::ButtonSize::Medium);
    int invoked = 0;
    button.onClick([&] { ++invoked; });
    button.layout({0.0f, 0.0f, 220.0f, 52.0f});
    expect(button.measure({}).height == 52.0f && button.secondaryContent() == "Organize related tasks",
           "CompoundButton must reserve its two-line Fluent command layout");
    expect(button.onPointerEvent(pointer(wui::PointerAction::Down)) && button.onPointerEvent(pointer(wui::PointerAction::Up)),
           "CompoundButton must treat both title and description as one command surface");
    expect(invoked == 1 && button.accessibilityActions().invoke,
           "CompoundButton must expose a single accessible invoke action");
}

void testFluentMenuButtonOpensExistingMenuOverlay()
{
    wui::OverlayHost overlays;
    wui::MenuButtonNode button("More");
    button.bindOverlayHost(overlays).addItem({"Rename", "", true, {}});
    button.layout({0.0f, 0.0f, 80.0f, 32.0f});
    expect(button.onPointerEvent(pointer(wui::PointerAction::Down)) && button.onPointerEvent(pointer(wui::PointerAction::Up)),
           "MenuButton must use its primary surface to open a menu");
    expect(button.isOpen() && overlays.size() == 1,
           "MenuButton must delegate transient menu ownership to OverlayHost");
    auto* menu = dynamic_cast<wui::MenuNode*>(overlays.top()->content.get());
    expect(menu != nullptr && menu->onKeyEvent({0, wui::KeyAction::Down, 27}),
           "The hosted Fluent menu must retain its Escape dismissal contract");
    expect(!button.isOpen() && overlays.empty(),
           "Menu dismissal must restore MenuButton's collapsed state");
}

void testFluentSplitButtonSeparatesPrimaryAndMenuActions()
{
    wui::OverlayHost overlays;
    wui::SplitButtonNode button("Save");
    int primary = 0;
    button.bindOverlayHost(overlays).addItem({"Save as", "", true, {}}).onClick([&] { ++primary; });
    button.layout({0.0f, 0.0f, 100.0f, 32.0f});
    expect(button.onPointerEvent(pointer(wui::PointerAction::Down, 8.0f)) && button.onPointerEvent(pointer(wui::PointerAction::Up, 8.0f)),
           "SplitButton primary region must execute its primary command");
    expect(primary == 1 && overlays.empty(), "SplitButton primary region must not accidentally open its menu");
    expect(button.onPointerEvent(pointer(wui::PointerAction::Down, 92.0f)) && button.onPointerEvent(pointer(wui::PointerAction::Up, 92.0f)),
           "SplitButton disclosure region must open its menu");
    expect(primary == 1 && button.isOpen() && overlays.size() == 1,
           "SplitButton disclosure region must remain independent from the primary action");
}

} // namespace

int main()
{
    testRadioSelectsWithoutTogglingOff();
    testSwitchBindingAndKeyboard();
    testSliderClampsSnapsAndSupportsKeys();
    testProgressAndDividerRanges();
    testDeclarativeBuildersRemainMoveOnly();
    testFluentButtonAppearanceSizeAndShape();
    testFluentLabelAssociatesInputAndUsesOfficialScale();
    testFluentCardSizingAndAppearance();
    testFluentCardSlotsLayOutContentAndActions();
    testFluentCardOrientationAndSelection();
    testCardPreviewUsesGeometricRoundedClip();
    testFluentTextAreaUsesEditingControllerAndVisualLines();
    testFluentToggleButtonOwnsButtonStyleToggleState();
    testFluentCompoundButtonUsesOneCommandSurface();
    testFluentMenuButtonOpensExistingMenuOverlay();
    testFluentSplitButtonSeparatesPrimaryAndMenuActions();
    return 0;
}
