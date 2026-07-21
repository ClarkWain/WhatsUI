#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include "wui/accessibility.h"
#include "wui/rating.h"
#include "wui/widgets.h"

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool near(float left, float right) { return std::abs(left - right) < 0.001f; }

wui::PointerEvent pointer(wui::PointerAction action, float x, float y,
                          wui::MouseButton button = wui::MouseButton::None)
{
    wui::PointerEvent event;
    event.action = action;
    event.button = button;
    event.position = {x, y};
    return event;
}

wui::KeyEvent key(int code)
{
    wui::KeyEvent event;
    event.action = wui::KeyAction::Down;
    event.keyCode = code;
    return event;
}

void testRatingValueAndInteraction()
{
    wui::Rating rating(2.0f, 5);
    rating.setStep(0.5f);
    rating.layout({10.0f, 10.0f, 140.0f, 28.0f});
    int changes = 0;
    float changedValue = 0.0f;
    rating.onChange([&](float value) { ++changes; changedValue = value; });

    expect(rating.onPointerEvent(pointer(wui::PointerAction::Down, 45.0f, 20.0f,
                                         wui::MouseButton::Left)),
           "Rating must start a primary pointer gesture");
    expect(rating.onPointerEvent(pointer(wui::PointerAction::Up, 45.0f, 20.0f,
                                         wui::MouseButton::Left)) &&
               near(rating.value(), 1.5f) && changes == 1 && near(changedValue, 1.5f),
           "Half-step Rating must commit the pointer-selected half item once");
    rating.onPointerEvent(pointer(wui::PointerAction::Down, 45.0f, 20.0f,
                                  wui::MouseButton::Left));
    rating.onPointerEvent(pointer(wui::PointerAction::Up, 45.0f, 20.0f,
                                  wui::MouseButton::Left));
    expect(changes == 1, "Selecting the current Rating value must not emit a duplicate change");

    expect(rating.onKeyEvent(key(39)) && near(rating.value(), 2.0f),
           "Right arrow must increment Rating by its configured step");
    expect(rating.onKeyEvent(key(35)) && near(rating.value(), 5.0f),
           "End must select Rating maximum");
    expect(rating.onKeyEvent(key(36)) && near(rating.value(), 0.5f),
           "Home must select the first valid Rating radio value");
    rating.setValue(0.0f);
    expect(near(rating.value(), 0.0f),
           "Rating must support an explicit programmatic clear-to-zero value");
    rating.setMaximum(1);
    expect(rating.maximum() == 2,
           "Rating maximum must enforce Fluent's minimum of two items");

    wui::Rating gapOwnership(0.0f, 5);
    gapOwnership.setSize(wui::RatingSize::Small);
    gapOwnership.layout({0.0f, 0.0f, 68.0f, 16.0f});
    gapOwnership.onPointerEvent(pointer(
        wui::PointerAction::Down, 13.0f, 8.0f,
        wui::MouseButton::Left));
    gapOwnership.onPointerEvent(pointer(
        wui::PointerAction::Up, 13.0f, 8.0f,
        wui::MouseButton::Left));
    expect(near(gapOwnership.value(), 1.0f),
           "The 2 DIP gap must belong to the preceding Rating item");
    gapOwnership.onPointerEvent(pointer(
        wui::PointerAction::Down, 14.0f, 8.0f,
        wui::MouseButton::Left));
    gapOwnership.onPointerEvent(pointer(
        wui::PointerAction::Up, 14.0f, 8.0f,
        wui::MouseButton::Left));
    expect(near(gapOwnership.value(), 2.0f),
           "The next Rating item must begin immediately after its 2 DIP gap");
}

void testRatingBindingReadonlyDisabledAndAccessibility()
{
    wui::State<float> state{3.0f};
    wui::Rating rating;
    rating.setAccessibleLabel("Product quality");
    rating.setItemLabel([](float value) { return value == 2.5f ? "Fair" : "Other"; });
    rating.bind(state).step(0.5f);
    expect(near(rating.value(), 3.0f), "Rating binding must adopt external state");
    state.set(4.5f);
    expect(near(rating.value(), 4.5f), "Rating binding must follow external updates");
    state.set(4.7f);
    expect(near(state.get(), 4.5f) && near(rating.value(), 4.5f),
           "Rating binding must canonicalize external off-step values in the source State");
    state.set(std::numeric_limits<float>::quiet_NaN());
    expect(near(state.get(), 0.0f) && near(rating.value(), 0.0f),
           "Rating binding must canonicalize non-finite external values");
    state.set(4.5f);
    expect(rating.performAccessibilityAction(wui::AccessibilityActionKind::SetValue, "2.5") ==
               wui::AccessibilityActionStatus::Succeeded && near(state.get(), 2.5f),
           "Rating accessibility SetValue must reuse its binding path");
    expect(rating.performAccessibilityAction(wui::AccessibilityActionKind::SetValue, "0") ==
               wui::AccessibilityActionStatus::Succeeded && near(state.get(), 0.0f),
           "Rating accessibility SetValue must support an explicit cleared value");
    rating.setValue(2.5f);
    expect(rating.performAccessibilityAction(wui::AccessibilityActionKind::SetValue, "8") ==
               wui::AccessibilityActionStatus::InvalidValue,
           "Rating accessibility SetValue must reject values outside its range");

    const auto snapshot = wui::snapshotAccessibilityTree(rating, &rating);
    expect(snapshot.size() == 1 && snapshot[0].properties.role == wui::AccessibilityRole::RadioGroup &&
               snapshot[0].properties.label == "Product quality" && snapshot[0].properties.focused &&
               snapshot[0].properties.value == std::optional<std::string>{"2.5 out of 5"} &&
               snapshot[0].properties.description == "Fair",
           "Rating must expose a named, focused radio-group semantic with current value");

    rating.setReadOnly(true);
    expect(rating.accessibilityActions().valueReadOnly && !rating.accessibilityActions().setValue &&
               !rating.onKeyEvent(key(39)) && near(rating.value(), 2.5f),
           "Read-only Rating must expose immutable value semantics and reject keyboard edits");
    rating.setReadOnly(false);
    rating.setEnabled(false);
    expect(rating.performAccessibilityAction(wui::AccessibilityActionKind::SetValue, "3") ==
               wui::AccessibilityActionStatus::ElementNotEnabled,
           "Disabled Rating must reject programmatic edits");
}

void testRatingRebindAndSetterInvalidation()
{
    // State values must outlive the Rating subscription handle.
    wui::State<float> oldState{2.0f};
    wui::State<float> newState{4.0f};
    wui::Rating rating;
    rating.setStep(0.5f);
    rating.bind(oldState);
    rating.bind(newState);

    oldState.set(1.0f);
    expect(near(rating.value(), 4.0f),
           "Rebinding Rating must immediately detach the previous State observer");
    newState.set(3.7f);
    expect(near(newState.get(), 3.5f) && near(rating.value(), 3.5f),
           "The replacement Rating State must keep canonical normalization writeback");

    rating.clearDirty();
    rating.setValue(2.5f);
    expect(rating.isDirty(wui::DirtyFlag::Paint) && !rating.isDirty(wui::DirtyFlag::Layout),
           "Rating value changes must invalidate paint without forcing layout");
    rating.clearDirty();
    rating.setMaximum(3);
    expect(rating.isDirty(wui::DirtyFlag::Layout),
           "Rating maximum changes must invalidate item geometry");
    rating.clearDirty();
    rating.setSize(wui::RatingSize::Small);
    expect(rating.isDirty(wui::DirtyFlag::Layout),
           "Rating size changes must invalidate item geometry");
    rating.clearDirty();
    rating.setColor(wui::RatingColor::Brand);
    expect(rating.isDirty(wui::DirtyFlag::Paint) && !rating.isDirty(wui::DirtyFlag::Layout),
           "Rating color changes must invalidate paint only");
    rating.clearDirty();
    rating.setAccessibleLabel("Updated rating");
    expect(rating.isDirty(wui::DirtyFlag::Style),
           "Rating accessible-name changes must refresh semantic style state");

    wui::RatingDisplay display(3.0f, 5);
    display.clearDirty();
    display.setValue(4.5f);
    expect(display.isDirty(wui::DirtyFlag::Layout),
           "RatingDisplay value text changes must invalidate measurement");
    display.clearDirty();
    display.setCount(42);
    expect(display.isDirty(wui::DirtyFlag::Layout),
           "RatingDisplay count changes must invalidate measurement");
    display.clearDirty();
    display.setColor(wui::RatingColor::Marigold);
    expect(display.isDirty(wui::DirtyFlag::Paint) && !display.isDirty(wui::DirtyFlag::Layout),
           "RatingDisplay color changes must invalidate paint only");
    display.clearDirty();
    display.setAccessibleLabel("Store score");
    expect(display.isDirty(wui::DirtyFlag::Style),
           "RatingDisplay accessible-name changes must refresh semantic style state");
}

void testRatingDisplayContract()
{
    wui::Rating interactive;
    interactive.setSize(wui::RatingSize::Small);
    expect(near(interactive.measure({}).width, 68.0f) &&
               near(interactive.measure({}).height, 16.0f),
           "Small Rating must use five 12 DIP items, four 2 DIP gaps and a 16 DIP row");
    interactive.setSize(wui::RatingSize::Medium);
    expect(near(interactive.measure({}).width, 88.0f) &&
               near(interactive.measure({}).height, 16.0f),
           "Medium Rating must use five 16 DIP items separated by 2 DIP");
    interactive.setSize(wui::RatingSize::Large);
    expect(near(interactive.measure({}).width, 108.0f) &&
               near(interactive.measure({}).height, 20.0f),
           "Large Rating must use five 20 DIP items separated by 2 DIP");
    interactive.setSize(wui::RatingSize::ExtraLarge);
    expect(near(interactive.measure({}).width, 148.0f) &&
               near(interactive.measure({}).height, 28.0f),
           "Extra-large Rating must use five 28 DIP items separated by 2 DIP");

    wui::RatingDisplay display(4.5f, 5);
    display.setCount(12345);
    expect(display.valueText() == "4.5" && display.countText() == "12,345",
           "RatingDisplay must format visible value and count deterministically");
    expect(display.generatedAccessibleLabel() == "4.5 out of 5, 12,345 ratings",
           "RatingDisplay must generate a complete accessible result");
    display.setCount(1);
    expect(display.generatedAccessibleLabel() == "4.5 out of 5, 1 rating",
           "RatingDisplay must use singular accessible count grammar for exactly one rating");
    display.countFormatter([](std::uint64_t value) { return "count:" + std::to_string(value); });
    display.setCount(12);
    expect(display.countText() == "count:12",
           "RatingDisplay must allow applications to replace locale-specific count formatting");
    display.setCountFormatter({});
    display.setCount(12345);
    display.setCompact(true);
    display.setSize(wui::RatingSize::ExtraLarge);
    expect(display.measure({}).width > 28.0f,
           "Compact RatingDisplay must include its required visible value text");
    const auto snapshot = wui::snapshotAccessibilityTree(display);
    expect(snapshot.size() == 1 && snapshot[0].properties.role == wui::AccessibilityRole::Image &&
               snapshot[0].properties.label == display.generatedAccessibleLabel(),
           "RatingDisplay must expose one passive image-style accessible result");

    display.setValue(std::nullopt);
    expect(display.valueText() == "0" && !display.generatedAccessibleLabel().empty() &&
               display.measure({}).width > 0.0f,
           "A rendered RatingDisplay must always retain a visible and accessible value");
}

void testImageFluentPropertiesAndAccessibility()
{
    const std::vector<unsigned char> pixels{
        255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 255, 255, 255, 255,
    };
    wui::Image image(pixels, 2, 2);
    image.setFit(wui::ImageFit::Center);
    image.setShape(wui::ImageShape::Rounded);
    image.setBordered(true);
    image.setShadow(true);
    image.setBlock(true);
    image.setAlt("Four color sample");
    const auto measured = image.measure({0.0f, 120.0f, 0.0f, 200.0f});
    expect(near(measured.width, 120.0f) && near(measured.height, 120.0f),
           "Block Image must fill finite width while preserving intrinsic aspect ratio");
    const auto heightLimited = image.measure({0.0f, 120.0f, 0.0f, 40.0f});
    expect(near(heightLimited.width, 40.0f) && near(heightLimited.height, 40.0f),
           "Block Image must honor both max axes without distorting its intrinsic aspect ratio");
    image.clearDirty();
    image.setAlt("Updated description");
    expect(image.isDirty(wui::DirtyFlag::Style),
           "Changing Image alt text must invalidate live accessibility semantics");
    image.layout({0.0f, 0.0f, 120.0f, 120.0f});
    wui::PaintContext paint;
    image.paint(paint);
    expect(paint.paintStats().boxShadowCalls == 2,
           "Shadow Image must use Fluent's two-layer shadow4 elevation");
    const auto snapshot = wui::snapshotAccessibilityTree(image);
    expect(snapshot.size() == 1 && snapshot[0].properties.role == wui::AccessibilityRole::Image &&
               snapshot[0].properties.label == "Updated description",
           "Informative Image must expose its concise alt description");
    image.setDecorative(true);
    expect(wui::snapshotAccessibilityTree(image).empty(),
           "Decorative Image must be excluded from assistive semantics");

    wui::Image fallbackOnly;
    fallbackOnly.fallback(pixels, 2, 2);
    expect(fallbackOnly.hasSource() && near(fallbackOnly.intrinsicSize().width, 2.0f),
           "Image fallback must become the effective source when no primary source is available");
    fallbackOnly.source(pixels, 2, 2);
    fallbackOnly.clearSource();
    expect(fallbackOnly.hasSource(),
           "Clearing a failed primary Image source must reveal its configured fallback");
}

} // namespace

int main()
{
    try {
        testRatingValueAndInteraction();
        testRatingBindingReadonlyDisabledAndAccessibility();
        testRatingRebindAndSetterInvalidation();
        testRatingDisplayContract();
        testImageFluentPropertiesAndAccessibility();
        std::cout << "WhatsUI Rating/Image tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WhatsUI Rating/Image tests failed: " << error.what() << '\n';
        return 1;
    }
}
