#pragma once

// Locale-neutral civil values and Fluent compatibility date/time inputs.  The
// values deliberately have no timezone: widgets store calendar intent, while
// an application decides how (or whether) to map that intent to an instant.

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "wui/overlays.h"

namespace wui {

struct CivilDate {
    int year{1970};
    int month{1};
    int day{1};
};
bool operator==(CivilDate left, CivilDate right) noexcept;
bool operator!=(CivilDate left, CivilDate right) noexcept;
bool operator<(CivilDate left, CivilDate right) noexcept;
bool isValidDate(CivilDate value) noexcept;
CivilDate addDays(CivilDate value, int days) noexcept;
CivilDate addMonths(CivilDate value, int months) noexcept;
int weekday(CivilDate value) noexcept; // 0 = Sunday, 6 = Saturday.
std::optional<CivilDate> parseIsoDate(std::string_view value) noexcept;
std::string formatIsoDate(CivilDate value);

struct CivilTime { int hour{0}; int minute{0}; int second{0}; };
bool operator==(CivilTime left, CivilTime right) noexcept;
bool isValidTime(CivilTime value) noexcept;
std::optional<CivilTime> parseTime(std::string_view value) noexcept; // HH:mm[:ss]
std::string formatTime(CivilTime value, bool includeSeconds = false);

enum class CalendarSelectionMode { Single, Range };

class CalendarNode : public ControlNode {
public:
    using SelectHandler = std::function<void(std::optional<CivilDate>, std::optional<CivilDate>)>;
    using DisablePredicate = std::function<bool(CivilDate)>;
    CalendarNode();
    CalendarNode& displayedMonth(CivilDate value); void setDisplayedMonth(CivilDate value);
    [[nodiscard]] CivilDate displayedMonth() const noexcept;
    CalendarNode& selectedDate(std::optional<CivilDate> value); void setSelectedDate(std::optional<CivilDate> value);
    [[nodiscard]] std::optional<CivilDate> selectedDate() const noexcept;
    CalendarNode& selectedRange(std::optional<CivilDate> start, std::optional<CivilDate> end); void setSelectedRange(std::optional<CivilDate> start, std::optional<CivilDate> end);
    [[nodiscard]] std::optional<CivilDate> rangeStart() const noexcept; [[nodiscard]] std::optional<CivilDate> rangeEnd() const noexcept;
    CalendarNode& selectionMode(CalendarSelectionMode value); void setSelectionMode(CalendarSelectionMode value);
    CalendarNode& minimumDate(std::optional<CivilDate> value); CalendarNode& maximumDate(std::optional<CivilDate> value);
    CalendarNode& isDateDisabled(DisablePredicate predicate); CalendarNode& onSelect(SelectHandler handler);
    [[nodiscard]] CivilDate focusedDate() const noexcept; [[nodiscard]] bool isDateEnabled(CivilDate value) const;
    [[nodiscard]] SizeF measure(const Constraints& constraints) const override; void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override; bool onPointerEvent(const PointerEvent& event) override; bool onKeyEvent(const KeyEvent& event) override;
private:
    [[nodiscard]] RectF dayBounds(CivilDate value) const noexcept; [[nodiscard]] std::optional<CivilDate> dateAt(PointF point) const noexcept;
    void moveFocus(int days); void select(CivilDate value); void setFocused(CivilDate value);
    CivilDate displayed_{1970,1,1}; CivilDate focused_{1970,1,1}; std::optional<CivilDate> selected_; std::optional<CivilDate> rangeStart_, rangeEnd_;
    std::optional<CivilDate> hoveredDate_, pressedDate_;
    int hoveredHeader_{0}, pressedHeader_{0}; // -1 previous, +1 next.
    std::optional<CivilDate> minimum_, maximum_; CalendarSelectionMode mode_{CalendarSelectionMode::Single}; DisablePredicate disabled_; SelectHandler onSelect_;
};

class DatePickerNode : public ControlNode {
public:
    using ChangeHandler = std::function<void(std::optional<CivilDate>)>;
    explicit DatePickerNode(std::string placeholder = "Select a date"); ~DatePickerNode() override;
    DatePickerNode& value(std::optional<CivilDate> value); void setValue(std::optional<CivilDate> value); [[nodiscard]] std::optional<CivilDate> value() const noexcept;
    DatePickerNode& text(std::string value); [[nodiscard]] const std::string& text() const noexcept; [[nodiscard]] bool isValid() const noexcept;
    DatePickerNode& placeholder(std::string value); DatePickerNode& minimumDate(std::optional<CivilDate> value); DatePickerNode& maximumDate(std::optional<CivilDate> value);
    DatePickerNode& bindOverlayHost(OverlayHost& host) noexcept; DatePickerNode& onChange(ChangeHandler handler);
    [[nodiscard]] bool isOpen() const noexcept; [[nodiscard]] SizeF measure(const Constraints& constraints) const override; void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override; bool onKeyEvent(const KeyEvent& event) override; bool onTextInput(const TextInputEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(AccessibilityActionKind kind, std::string_view value) override;
private: void openPopup(); void closePopup(); void commit(std::optional<CivilDate> value); void validateText();
    std::string placeholder_, text_; std::optional<CivilDate> value_, minimum_, maximum_; bool valid_{true}, open_{false}; OverlayHost* host_{nullptr}; std::size_t overlay_{0}; ChangeHandler onChange_;
};

class TimePickerNode : public ControlNode {
public:
    using ChangeHandler = std::function<void(std::optional<CivilTime>)>;
    explicit TimePickerNode(std::string placeholder = "Select a time"); ~TimePickerNode() override;
    TimePickerNode& value(std::optional<CivilTime> value); void setValue(std::optional<CivilTime> value); [[nodiscard]] std::optional<CivilTime> value() const noexcept;
    TimePickerNode& text(std::string value); [[nodiscard]] const std::string& text() const noexcept; [[nodiscard]] bool isValid() const noexcept;
    TimePickerNode& minuteStep(int value); [[nodiscard]] int minuteStep() const noexcept;
    TimePickerNode& bindOverlayHost(OverlayHost& host) noexcept; TimePickerNode& onChange(ChangeHandler handler);
    [[nodiscard]] bool isOpen() const noexcept; [[nodiscard]] SizeF measure(const Constraints& constraints) const override; void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override; bool onKeyEvent(const KeyEvent& event) override; bool onTextInput(const TextInputEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(AccessibilityActionKind kind, std::string_view value) override;
private: void openPopup(); void closePopup(); void commit(std::optional<CivilTime> value); void validateText();
    std::string placeholder_, text_; std::optional<CivilTime> value_; bool valid_{true}, open_{false}; int minuteStep_{15}; OverlayHost* host_{nullptr}; std::size_t overlay_{0}; ChangeHandler onChange_;
};

} // namespace wui
