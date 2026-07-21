#include <iostream>
#include <stdexcept>

#include "wui/date_time.h"
#include "wui/runtime.h"

namespace {
void expect(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void testCivilValues()
{
    expect(wui::parseIsoDate("2024-02-29").has_value(), "Leap day must parse deterministically");
    expect(!wui::parseIsoDate("2023-02-29").has_value(), "Invalid civil dates must not parse");
    expect(wui::formatIsoDate(wui::addDays({2024, 2, 28}, 2)) == "2024-03-01", "Civil arithmetic must not use timezone/DST");
    expect(wui::parseTime("09:30").has_value() && !wui::parseTime("9:3").has_value(), "Time parser must use deterministic fixed-width input");
}
void testCalendarKeyboardAndPolicies()
{
    wui::Calendar calendar;
    const auto natural = calendar.measureWithConstraints({});
    expect(natural.width == 248.0f && natural.height == 272.0f,
           "Calendar must use seven 32-DIP rows inside its 12-DIP Fluent panel inset");
    calendar.setDisplayedMonth({2024, 2, 1});
    calendar.setSelectedDate(wui::CivilDate{2024, 2, 28});
    calendar.minimumDate(wui::CivilDate{2024, 2, 2});
    calendar.isDateDisabled([](wui::CivilDate d) { return d.day == 29; });
    calendar.layout({0, 0, 276, 300});
    calendar.onKeyEvent({0, wui::KeyAction::Down, 39});
    expect(calendar.focusedDate() == wui::CivilDate{2024, 2, 29}, "Arrow keys must move by one civil day");
    calendar.onKeyEvent({0, wui::KeyAction::Down, 13});
    expect(calendar.selectedDate() == wui::CivilDate{2024, 2, 28}, "Disabled day must not commit selection");
    calendar.setSelectionMode(wui::CalendarSelectionMode::Range);
    calendar.setSelectedRange({}, {});
    calendar.onKeyEvent({0, wui::KeyAction::Down, 37}); calendar.onKeyEvent({0, wui::KeyAction::Down, 13});
    calendar.onKeyEvent({0, wui::KeyAction::Down, 39}); calendar.onKeyEvent({0, wui::KeyAction::Down, 39}); calendar.onKeyEvent({0, wui::KeyAction::Down, 13});
    expect(calendar.rangeStart().has_value() && calendar.rangeEnd().has_value(), "Range calendar must collect start and end dates");

    calendar.setDisplayedMonth({2024, 2, 1});
    calendar.setSelectionMode(wui::CalendarSelectionMode::Single);
    calendar.layout({0, 0, 248, 272});
    const wui::PointF day15{12.0f + 4.0f * 32.0f + 16.0f,
                            12.0f + 32.0f + 24.0f + 2.0f * 32.0f + 16.0f};
    expect(calendar.onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Move,
                                    wui::MouseButton::None, day15}),
           "Calendar day hover must be retained as a visual state");
    expect(calendar.onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Down,
                                    wui::MouseButton::Left, day15}) &&
               calendar.onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Up,
                                        wui::MouseButton::Left, day15}) &&
               calendar.selectedDate() == wui::CivilDate{2024, 2, 15},
           "Calendar must commit the same enabled 32-DIP hit row on pointer release");
    const wui::PointF previousMonth{194.0f, 16.0f};
    expect(calendar.onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Down,
                                    wui::MouseButton::Left, previousMonth}) &&
               calendar.onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Up,
                                        wui::MouseButton::Left, previousMonth}) &&
               calendar.displayedMonth() == wui::CivilDate{2024, 1, 1},
           "Calendar header chevron must own its complete 28-DIP button slot");
}
void testPickersValidateAndStep()
{
    wui::DatePicker date;
    expect(date.measureWithConstraints({}).height == 32.0f,
           "DatePicker must retain the Fluent medium 32-DIP input height");
    date.text("2024-02-31"); expect(!date.isValid(), "DatePicker invalid text must be observable without locale parsing");
    date.text("2024-02-29"); expect(date.isValid() && date.value().has_value(), "DatePicker canonical ISO input must commit");
    wui::TimePicker time;
    expect(time.measureWithConstraints({}).height == 32.0f,
           "TimePicker must retain the Fluent medium 32-DIP input height");
    time.minuteStep(15); time.text("09:07"); expect(!time.isValid(), "TimePicker must reject values off its minute step");
    time.text("09:30"); expect(time.isValid() && time.value()->minute == 30, "TimePicker must retain canonical time value");
    time.onKeyEvent({0, wui::KeyAction::Down, 38}); expect(time.value()->minute == 45, "TimePicker arrow key must advance by configured minute step");
}
void testPickerAccessibilityAndPopupContracts()
{
    wui::OverlayHost overlays;
    wui::DatePicker date; date.bindOverlayHost(overlays);
    expect(date.accessibilityActions().expandCollapse && date.accessibilityActions().setValue,
           "DatePicker must expose its popup and canonical value to accessibility");
    expect(date.performAccessibilityAction(wui::AccessibilityActionKind::SetValue, "2024-02-29") ==
               wui::AccessibilityActionStatus::Succeeded && date.value() == wui::CivilDate{2024, 2, 29},
           "DatePicker must accept a canonical UIA value");
    expect(date.performAccessibilityAction(wui::AccessibilityActionKind::SetValue, "2024-02-30") ==
               wui::AccessibilityActionStatus::InvalidValue,
           "DatePicker must reject an invalid UIA value without locale parsing");
    expect(date.performAccessibilityAction(wui::AccessibilityActionKind::Expand, {}) ==
               wui::AccessibilityActionStatus::Succeeded && date.isOpen() && overlays.size() == 1,
           "DatePicker expand must materialize a focused calendar popup");
    expect(date.performAccessibilityAction(wui::AccessibilityActionKind::Collapse, {}) ==
               wui::AccessibilityActionStatus::Succeeded && !date.isOpen() && overlays.empty(),
           "DatePicker collapse must remove the popup deterministically");

    wui::TimePicker time; time.minuteStep(15).bindOverlayHost(overlays);
    expect(time.performAccessibilityAction(wui::AccessibilityActionKind::SetValue, "09:30") ==
               wui::AccessibilityActionStatus::Succeeded && time.value() == wui::CivilTime{9, 30, 0},
           "TimePicker must accept a canonical stepped UIA value");
    expect(time.performAccessibilityAction(wui::AccessibilityActionKind::SetValue, "09:07") ==
               wui::AccessibilityActionStatus::InvalidValue,
           "TimePicker must reject a value outside its minute step");
    expect(time.performAccessibilityAction(wui::AccessibilityActionKind::Expand, {}) ==
               wui::AccessibilityActionStatus::Succeeded && time.isOpen() && overlays.size() == 1,
           "TimePicker expand must open a selectable time list");
    expect(time.performAccessibilityAction(wui::AccessibilityActionKind::Collapse, {}) ==
               wui::AccessibilityActionStatus::Succeeded && !time.isOpen() && overlays.empty(),
           "TimePicker collapse must close its list and restore field ownership");
}
}
int main() { try { testCivilValues(); testCalendarKeyboardAndPolicies(); testPickersValidateAndStep(); testPickerAccessibilityAndPopupContracts(); std::cout << "Fluent date/time tests passed\n"; return 0; } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
