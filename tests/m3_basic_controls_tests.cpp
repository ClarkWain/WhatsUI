#include <cmath>
#include <stdexcept>
#include <string>

#include "wui/basic_controls.h"
#include "wui/ui.h"

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
    wui::Radio radio("Daily");
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
    wui::Switch toggle("Notifications");
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
    wui::Slider slider(0.0f, 10.0f, 0.0f);
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
    wui::ProgressBar progress(0.0f, 1.0f, 3.0f);
    expect(progress.value() == 1.0f, "Progress should clamp initial value");
    progress.setValue(-2.0f);
    expect(progress.value() == 0.0f, "Progress should clamp updates");
    wui::Divider horizontal;
    wui::Divider vertical(wui::DividerOrientation::Vertical);
    expect(horizontal.measure({}).height == 1.0f, "Horizontal divider should reserve thickness vertically");
    expect(vertical.measure({}).width == 1.0f, "Vertical divider should reserve thickness horizontally");
}

void testDeclarativeBuildersRemainMoveOnly()
{
    auto root = wui::ui::Column().children(
        wui::ui::Radio("Today").selected(true),
        wui::ui::Switch("Remind me").on(true),
        wui::ui::Slider(0.0f, 10.0f, 3.0f).step(1.0f),
        wui::ui::ProgressBar(0.0f, 1.0f, 0.5f),
        wui::ui::Divider());
    expect(root.get() != nullptr, "Fluent controls must participate in the declarative builder API");
}

} // namespace

int main()
{
    testRadioSelectsWithoutTogglingOff();
    testSwitchBindingAndKeyboard();
    testSliderClampsSnapsAndSupportsKeys();
    testProgressAndDividerRanges();
    testDeclarativeBuildersRemainMoveOnly();
    return 0;
}
