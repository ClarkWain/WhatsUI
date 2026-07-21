#include <cmath>
#include <stdexcept>
#include <string>

#include "wui/accessibility.h"
#include "wui/basic_controls.h"
#include "wui/events.h"
#include "wui/paint_context.h"

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

wui::PointerEvent pointer(wui::PointerAction action, float x, float y)
{
    return {0, wui::PointerType::Mouse, action, wui::MouseButton::Left, {x, y}};
}

void testRadioGroupOwnsSelectionAndArrowPolicy()
{
    wui::Radio iconOnly;
    expect(iconOnly.measure({}).width == 32.0f &&
               iconOnly.measure({}).height == 32.0f,
           "An unlabeled Radio must retain Fluent's square 32-DIP hit slot");
    // Bound State must outlive the group because Node teardown unsubscribes
    // from it during group destruction.
    wui::State<std::string> state{"third"};
    wui::RadioGroup group;
    group.name("delivery-speed").accessibleLabel("Delivery speed");
    auto& first = group.addOption("first", "First");
    auto& unavailable = group.addOption("disabled", "Unavailable", false);
    auto& third = group.addOption("third", "Third");
    int changes = 0;
    std::string selected;
    group.onChange([&](const std::string& value) { ++changes; selected = value; });
    group.layout({0, 0, 240, 104});

    expect(first.onKeyEvent({0, wui::KeyAction::Down, 32}), "Space must select a RadioGroup option");
    expect(group.name() == "delivery-speed" && group.accessibleLabel() == "Delivery speed",
           "RadioGroup must expose a stable native name and accessible label");
    expect(group.value() == "first" && first.isSelected(), "RadioGroup must expose its selected option value");
    expect(first.onKeyEvent({0, wui::KeyAction::Down, 40}), "Down must move RadioGroup selection");
    expect(group.value() == "third" && third.isSelected() && !first.isSelected(),
           "RadioGroup arrows must wrap among enabled options and preserve mutual exclusion");
    expect(!unavailable.isSelected() && changes == 2 && selected == "third",
           "Disabled RadioGroup options must be skipped and group onChange must fire once per change");

    group.bind(state);
    group.setValue("first");
    expect(state.get() == "first" && first.isSelected() && !third.isSelected(),
           "A bound RadioGroup must retain one source of truth");
    group.setRequired(true);
    const auto groupSnapshot = wui::snapshotAccessibilityTree(group);
    expect(!groupSnapshot.empty(), "RadioGroup must contribute an accessibility tree entry");
    const auto& groupProperties = groupSnapshot.front().properties;
    expect(groupProperties.role == wui::AccessibilityRole::RadioGroup &&
               groupProperties.label == "Delivery speed" &&
               groupProperties.value == std::optional<std::string>{"first"} &&
               groupProperties.required,
           "RadioGroup accessibility must expose role, name, selected value, and required state");
    group.setGroupLayout(wui::RadioGroupLayout::HorizontalStacked);
    const auto firstStackedMeasure = group.measure({0, 400, 0, 200});
    expect(firstStackedMeasure.width > firstStackedMeasure.height &&
               firstStackedMeasure.height == 56.0f,
           "Horizontal-stacked RadioGroup must reserve a 32-DIP indicator slot, 4-DIP gap, and 20-DIP label line");

    wui::RadioGroup initiallyStacked;
    initiallyStacked.setGroupLayout(wui::RadioGroupLayout::HorizontalStacked);
    initiallyStacked.addOption("a", "Alpha");
    initiallyStacked.addOption("b", "Beta");
    expect(initiallyStacked.measure({0, 400, 0, 200}).height == 56.0f,
           "Horizontal-stacked RadioGroup must reserve its exact stacked height on the first measure before layout");
}

void testRadioGroupArrowKeysMoveRovingFocus()
{
    wui::RadioGroup group;
    auto& first = group.addOption("first", "First");
    group.addOption("disabled", "Disabled", false);
    auto& third = group.addOption("third", "Third");
    group.layout({0, 0, 240, 104});

    wui::FocusManager focus;
    wui::InputRouter router(&focus);
    router.setRoot(&group);
    focus.setFocused(&first);

    expect(router.dispatchKey({0, wui::KeyAction::Down, 40}),
           "A focused RadioGroup option must handle ArrowDown");
    expect(group.value() == "third" && focus.focused() == &third,
           "RadioGroup ArrowDown must skip disabled options and move the real roving focus");
    expect(router.dispatchKey({0, wui::KeyAction::Down, 38}),
           "A selected RadioGroup option must handle ArrowUp");
    expect(group.value() == "first" && focus.focused() == &first,
           "RadioGroup ArrowUp must move the real roving focus with selection");
}

void testSwitchOfficialSizesAndLabelPositions()
{
    wui::Switch iconOnly;
    iconOnly.setSize(wui::SwitchSize::Small);
    expect(iconOnly.measure({}).width == 48.0f,
           "Small Switch must reserve its 32-DIP track plus two 8-DIP hit margins");
    iconOnly.setSize(wui::SwitchSize::Medium);
    expect(iconOnly.measure({}).width == 56.0f,
           "Medium Switch must reserve its 40-DIP track plus two 8-DIP hit margins");

    wui::Switch toggle("Sync", false);
    toggle.setSize(wui::SwitchSize::Small);
    expect(toggle.measure({}).height == 32.0f, "Small Switch must use Fluent's 32 DIP control row");
    toggle.setSize(wui::SwitchSize::Medium);
    expect(toggle.measure({}).height == 36.0f, "Medium Switch must use Fluent's 36 DIP control row");
    const auto after = toggle.measure({});
    toggle.setLabelPosition(wui::SwitchLabelPosition::Above);
    expect(toggle.measure({}).height > after.height, "Above label must reserve its own text line");
    toggle.setRequired(true);
    expect(toggle.isRequired(), "Required Switch state must remain inspectable");
    expect(toggle.onKeyEvent({0, wui::KeyAction::Down, 32}) && toggle.isOn(),
           "Space must immediately change a Switch");
    toggle.setEnabled(false);
    expect(!toggle.onKeyEvent({0, wui::KeyAction::Down, 32}) && toggle.isOn(),
           "Disabled Switch must ignore keyboard activation");
}

void testSliderSizeOrientationAndAccessibleValue()
{
    wui::Slider slider(0, 10, 2);
    slider.accessibleLabel("Volume");
    expect(slider.accessibleLabel() == "Volume", "Slider must expose an accessible field name");
    slider.step(2);
    slider.setSize(wui::SliderSize::Small);
    expect(slider.measure({}).height == 24.0f, "Small horizontal Slider must use a 24 DIP cross-axis row");
    slider.setSize(wui::SliderSize::Medium);
    expect(slider.measure({}).height == 32.0f, "Medium horizontal Slider must use a 32 DIP cross-axis row");

    slider.setOrientation(wui::SliderOrientation::Vertical);
    const auto vertical = slider.measure({});
    expect(vertical.width == 32.0f && vertical.height == 160.0f,
           "Vertical Slider must swap its main and cross axes");
    slider.layout({0, 0, 32, 120});
    slider.onPointerEvent(pointer(wui::PointerAction::Down, 16, 10));
    slider.onPointerEvent(pointer(wui::PointerAction::Up, 16, 10));
    expect(slider.value() == 10.0f, "Top of a vertical Slider must represent maximum");
    slider.onPointerEvent(pointer(wui::PointerAction::Down, 16, 110));
    slider.onPointerEvent(pointer(wui::PointerAction::Up, 16, 110));
    expect(slider.value() == 0.0f, "Bottom of a vertical Slider must represent minimum");
    expect(slider.performAccessibilityAction(wui::AccessibilityActionKind::SetValue, "6") ==
               wui::AccessibilityActionStatus::Succeeded && slider.value() == 6.0f,
           "Slider accessibility SetValue must share clamping and snapping semantics");
    expect(slider.performAccessibilityAction(wui::AccessibilityActionKind::SetValue, "not-a-number") ==
               wui::AccessibilityActionStatus::InvalidValue,
           "Slider accessibility SetValue must reject malformed numbers");
    expect(slider.performAccessibilityAction(wui::AccessibilityActionKind::SetValue, "20") ==
               wui::AccessibilityActionStatus::InvalidValue,
           "Slider accessibility SetValue must reject out-of-range numbers instead of silently clamping");
    const auto sliderSnapshot = wui::snapshotAccessibilityTree(slider);
    expect(sliderSnapshot.size() == 1, "Slider must contribute one accessibility tree entry");
    const auto& sliderProperties = sliderSnapshot.front().properties;
    expect(sliderProperties.role == wui::AccessibilityRole::Slider &&
               sliderProperties.label == "Volume" && sliderProperties.value.has_value() &&
               std::stof(*sliderProperties.value) == 6.0f,
           "Slider accessibility must expose role, name, and current numeric value");
}

void testProgressVariantsAndIndeterminateRendering()
{
    wui::ProgressBar defaultProgress;
    defaultProgress.accessibleLabel("Uploading files");
    expect(defaultProgress.accessibleLabel() == "Uploading files",
           "ProgressBar must expose an accessible operation description");
    expect(defaultProgress.minimum() == 0.0f && defaultProgress.maximum() == 1.0f,
           "ProgressBar must use Fluent's default normalized [0, 1] range");
    expect(defaultProgress.isIndeterminate() && !defaultProgress.determinateValue().has_value(),
           "ProgressBar without a supplied value must use Fluent's indeterminate default");
    const auto defaultSnapshot = wui::snapshotAccessibilityTree(defaultProgress);
    expect(defaultSnapshot.size() == 1,
           "Default ProgressBar must contribute one accessibility tree entry");
    const auto& defaultProperties = defaultSnapshot.front().properties;
    expect(defaultProperties.role == wui::AccessibilityRole::ProgressBar &&
               defaultProperties.label == "Uploading files" &&
               !defaultProperties.value.has_value() && defaultProperties.busy &&
               defaultProperties.actions.valueReadOnly,
           "Default ProgressBar accessibility must be busy, value-less, and read-only");
    wui::ProgressBar progress(0.0f, 10.0f, 4.0f);
    expect(!progress.isIndeterminate() && progress.determinateValue() == std::optional<float>{4.0f},
           "Supplying a ProgressBar value must select determinate mode");
    progress.setValue(5.0f);
    expect(!progress.isIndeterminate() && progress.value() == 5.0f,
           "Setting a value must make an indeterminate ProgressBar determinate");
    expect(progress.measure({}).height == 2.0f, "Medium ProgressBar must use Fluent's 2 DIP track");
    progress.setThickness(wui::ProgressBarThickness::Large);
    expect(progress.measure({}).height == 4.0f, "Large ProgressBar must use a 4 DIP track");
    progress.setColor(wui::ProgressBarColor::Success);
    progress.setShape(wui::ProgressBarShape::Square);
    progress.setIndeterminate(true);
    progress.setMotionEnabled(false);
    progress.layout({0, 0, 200, 4});
    wui::PaintContext paint;
    progress.paint(paint);
    expect(paint.paintStats().clipRectCalls == 1 && progress.isIndeterminate() && !progress.determinateValue().has_value(),
           "Indeterminate ProgressBar must clip its moving segment to the track");
    expect(progress.accessibilityActions().valueReadOnly,
           "ProgressBar must expose its numeric value as read-only accessibility data");
    const auto indeterminateSnapshot = wui::snapshotAccessibilityTree(progress);
    expect(indeterminateSnapshot.size() == 1,
           "Indeterminate ProgressBar must contribute one accessibility tree entry");
    const auto& indeterminateProperties = indeterminateSnapshot.front().properties;
    expect(indeterminateProperties.role == wui::AccessibilityRole::ProgressBar &&
               indeterminateProperties.busy && !indeterminateProperties.value.has_value() &&
               indeterminateProperties.actions.valueReadOnly,
           "Indeterminate ProgressBar accessibility must expose busy=true, omit value, and stay read-only");
}

void testDividerContentAppearanceAndOrientation()
{
    wui::Divider divider;
    divider.content("OR").appearance(wui::DividerAppearance::Brand)
        .contentAlignment(wui::DividerContentAlignment::Start).inset(true);
    expect(divider.measure({}).height >= 20.0f,
           "A labelled horizontal Divider must reserve the Fluent body line height");
    divider.layout({0, 0, 240, 20});
    wui::PaintContext paint;
    divider.paint(paint);
    expect(paint.paintStats().fillRectCalls >= 1 && paint.paintStats().textDrawCalls == 1,
           "A labelled Divider must paint both separator line and content");

    divider.setOrientation(wui::DividerOrientation::Vertical);
    expect(divider.measure({}).width > divider.thickness(),
           "A labelled vertical Divider must reserve room for its content");
    divider.setContent({});
    expect(divider.measure({}).width == divider.thickness(),
           "An unlabelled vertical Divider must collapse to its stroke width");
}

} // namespace

int main()
{
    testRadioGroupOwnsSelectionAndArrowPolicy();
    testRadioGroupArrowKeysMoveRovingFocus();
    testSwitchOfficialSizesAndLabelPositions();
    testSliderSizeOrientationAndAccessibleValue();
    testProgressVariantsAndIndeterminateRendering();
    testDividerContentAppearanceAndOrientation();
    return 0;
}
