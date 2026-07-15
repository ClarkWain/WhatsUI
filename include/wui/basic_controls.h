#pragma once

// Fluent-styled foundational form controls.  They are intentionally kept in
// a separate header so applications which only need layout and text do not
// need to adopt the complete higher-level control catalogue.

#include <functional>
#include <optional>
#include <string>

#include "wui/node.h"
#include "wui/state.h"

namespace wui {

// A radio option represents a selected boolean.  A containing application
// owns the group policy: radio options that are mutually exclusive should be
// bound to a single selection State or updated together from onChange().
class Radio : public ControlNode {
public:
    using ChangeHandler = std::function<void(bool)>;

    explicit Radio(std::string label = {}, bool selected = false);

    [[nodiscard]] const std::string& label() const noexcept;
    Radio& label(std::string value);
    void setLabel(std::string value);

    [[nodiscard]] bool isSelected() const noexcept;
    Radio& selected(bool value);
    void setSelected(bool value);
    Radio& bind(State<bool>& state);
    Radio& onChange(ChangeHandler handler);

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;

private:
    void select();

    std::string label_;
    bool selected_{false};
    std::optional<Binding<bool>> binding_;
    bool hasBinding_{false};
    ChangeHandler onChange_;
};

// A two-state Fluent toggle.  It supports both direct ownership and a State
// binding; bound state is always the source of truth for later UI updates.
class Switch : public ControlNode {
public:
    using ChangeHandler = std::function<void(bool)>;

    explicit Switch(std::string label = {}, bool on = false);

    [[nodiscard]] const std::string& label() const noexcept;
    Switch& label(std::string value);
    void setLabel(std::string value);

    [[nodiscard]] bool isOn() const noexcept;
    Switch& on(bool value);
    void setOn(bool value);
    Switch& bind(State<bool>& state);
    Switch& onChange(ChangeHandler handler);

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    void toggle();

    std::string label_;
    bool on_{false};
    std::optional<Binding<bool>> binding_;
    bool hasBinding_{false};
    ChangeHandler onChange_;
};

// A horizontal, keyboard-accessible numeric range.  Values are clamped to
// [minimum, maximum] and optionally snapped to step increments from minimum.
class Slider : public ControlNode {
public:
    using ChangeHandler = std::function<void(float)>;

    Slider(float minimum = 0.0f, float maximum = 100.0f, float value = 0.0f);

    [[nodiscard]] float minimum() const noexcept;
    [[nodiscard]] float maximum() const noexcept;
    void setRange(float minimum, float maximum);
    [[nodiscard]] float value() const noexcept;
    Slider& value(float value);
    void setValue(float value);
    [[nodiscard]] float step() const noexcept;
    Slider& step(float value);
    void setStep(float value);
    Slider& bind(State<float>& state);
    Slider& onChange(ChangeHandler handler);

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;

private:
    [[nodiscard]] float normalizedValue() const noexcept;
    [[nodiscard]] float normalizedAndSnapped(float value) const noexcept;
    void setValueFromPointer(float x);

    float minimum_{0.0f};
    float maximum_{100.0f};
    float value_{0.0f};
    float step_{1.0f};
    std::optional<Binding<float>> binding_;
    bool hasBinding_{false};
    ChangeHandler onChange_;
};

// A passive progress indicator.  Its value uses the same clamped range
// semantics as Slider, while a zero-height range is displayed as empty.
class ProgressBar : public Node {
public:
    ProgressBar(float minimum = 0.0f, float maximum = 100.0f, float value = 0.0f);

    [[nodiscard]] float minimum() const noexcept;
    [[nodiscard]] float maximum() const noexcept;
    void setRange(float minimum, float maximum);
    [[nodiscard]] float value() const noexcept;
    ProgressBar& value(float value);
    void setValue(float value);
    ProgressBar& bind(State<float>& state);

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;

private:
    [[nodiscard]] float normalizedValue() const noexcept;
    [[nodiscard]] float clampedValue(float value) const noexcept;

    float minimum_{0.0f};
    float maximum_{100.0f};
    float value_{0.0f};
    std::optional<Binding<float>> binding_;
    bool hasBinding_{false};
};

enum class DividerOrientation {
    Horizontal,
    Vertical,
};

// A low-emphasis separator.  Its cross-axis thickness is one logical pixel
// by default, with an explicit setter for dense or high-contrast layouts.
class Divider : public Node {
public:
    explicit Divider(DividerOrientation orientation = DividerOrientation::Horizontal);

    [[nodiscard]] DividerOrientation orientation() const noexcept;
    void setOrientation(DividerOrientation orientation) noexcept;
    [[nodiscard]] float thickness() const noexcept;
    void setThickness(float thickness) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;

private:
    DividerOrientation orientation_{DividerOrientation::Horizontal};
    float thickness_{1.0f};
};

} // namespace wui
