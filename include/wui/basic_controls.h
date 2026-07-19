#pragma once

// Fluent-styled foundational form controls.  They are intentionally kept in
// a separate header so applications which only need layout and text do not
// need to adopt the complete higher-level control catalogue.

#include <functional>
#include <cstddef>
#include <optional>
#include <string>

#include "wui/node.h"
#include "wui/state.h"

namespace wui {

class RadioGroup;

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

    // RadioGroup uses value as the stable selection identity.  Standalone
    // radios default it to their label for source compatibility.
    [[nodiscard]] const std::string& value() const noexcept;
    Radio& value(std::string value);
    void setValue(std::string value);

    [[nodiscard]] bool isSelected() const noexcept;
    Radio& selected(bool value);
    void setSelected(bool value);
    Radio& bind(State<bool>& state);
    Radio& onChange(ChangeHandler handler);

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    friend class RadioGroup;
    void select();
    void setSelectedFromGroup(bool value);
    void setStackedLabel(bool value) noexcept;

    std::string label_;
    std::string value_;
    bool selected_{false};
    bool stackedLabel_{false};
    bool optionEnabled_{true};
    std::optional<Binding<bool>> binding_;
    StateSubscription<bool> subscription_;
    bool hasBinding_{false};
    ChangeHandler onChange_;
};

enum class RadioGroupLayout {
    Vertical,
    Horizontal,
    HorizontalStacked,
};

// Owns the mutual-exclusion, value and arrow-key policy for a set of Radio
// children. Options are exposed as real child controls so pointer routing,
// focus and native accessibility retain one item per choice.
class RadioGroup : public ControlNode {
public:
    using ChangeHandler = std::function<void(const std::string&)>;

    RadioGroup() = default;

    Radio& addOption(std::string value, std::string label, bool enabled = true);
    [[nodiscard]] const std::string& name() const noexcept;
    RadioGroup& name(std::string value);
    void setName(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    RadioGroup& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& value() const noexcept;
    RadioGroup& value(std::string value);
    void setValue(std::string value);
    // The selected child is exposed for host input routing. In particular,
    // Fluent's arrow-key radio policy moves both selection and keyboard focus
    // to the newly selected option.
    [[nodiscard]] Radio* selectedRadio() noexcept;
    [[nodiscard]] const Radio* selectedRadio() const noexcept;
    RadioGroup& bind(State<std::string>& state);
    RadioGroup& onChange(ChangeHandler handler);

    [[nodiscard]] RadioGroupLayout groupLayout() const noexcept;
    RadioGroup& groupLayout(RadioGroupLayout value) noexcept;
    void setGroupLayout(RadioGroupLayout value) noexcept;
    [[nodiscard]] bool isRequired() const noexcept;
    RadioGroup& required(bool value) noexcept;
    void setRequired(bool value) noexcept;
    void setEnabled(bool enabled) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;

private:
    friend class Radio;
    void selectRadio(Radio& radio);
    bool moveSelection(Radio& from, int delta);
    void applyValue(const std::string& value, bool notify);
    void syncChildStates() noexcept;

    std::string value_;
    std::string name_;
    std::string accessibleLabel_;
    std::optional<Binding<std::string>> binding_;
    StateSubscription<std::string> subscription_;
    bool hasBinding_{false};
    ChangeHandler onChange_;
    RadioGroupLayout layout_{RadioGroupLayout::Vertical};
    bool required_{false};
};

enum class SwitchSize {
    Small,
    Medium,
};

enum class SwitchLabelPosition {
    Before,
    After,
    Above,
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
    [[nodiscard]] SwitchSize size() const noexcept;
    Switch& size(SwitchSize value) noexcept;
    void setSize(SwitchSize value) noexcept;
    [[nodiscard]] SwitchLabelPosition labelPosition() const noexcept;
    Switch& labelPosition(SwitchLabelPosition value) noexcept;
    void setLabelPosition(SwitchLabelPosition value) noexcept;
    [[nodiscard]] bool isRequired() const noexcept;
    Switch& required(bool value) noexcept;
    void setRequired(bool value) noexcept;

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
    StateSubscription<bool> subscription_;
    bool hasBinding_{false};
    ChangeHandler onChange_;
    SwitchSize size_{SwitchSize::Medium};
    SwitchLabelPosition labelPosition_{SwitchLabelPosition::After};
    bool required_{false};
};

enum class SliderSize {
    Small,
    Medium,
};

enum class SliderOrientation {
    Horizontal,
    Vertical,
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
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    Slider& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] SliderSize size() const noexcept;
    Slider& size(SliderSize value) noexcept;
    void setSize(SliderSize value) noexcept;
    [[nodiscard]] SliderOrientation orientation() const noexcept;
    Slider& orientation(SliderOrientation value) noexcept;
    void setOrientation(SliderOrientation value) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    [[nodiscard]] float normalizedValue() const noexcept;
    [[nodiscard]] float normalizedAndSnapped(float value) const noexcept;
    void setValueFromPointer(float x);

    float minimum_{0.0f};
    float maximum_{100.0f};
    float value_{0.0f};
    float step_{1.0f};
    std::optional<Binding<float>> binding_;
    StateSubscription<float> subscription_;
    bool hasBinding_{false};
    ChangeHandler onChange_;
    std::string accessibleLabel_;
    SliderSize size_{SliderSize::Medium};
    SliderOrientation orientation_{SliderOrientation::Horizontal};
};

enum class ProgressBarColor {
    Brand,
    Error,
    Warning,
    Success,
};

enum class ProgressBarShape {
    Rounded,
    Square,
};

enum class ProgressBarThickness {
    Medium,
    Large,
};

// A passive progress indicator.  With no value it represents an indeterminate
// operation (the Fluent default); supplying a value makes it determinate.
// Its determinate value uses the same clamped range semantics as Slider,
// while a zero-height range is displayed as empty.
class ProgressBar : public Node {
public:
    ProgressBar(float minimum = 0.0f, float maximum = 1.0f,
                std::optional<float> value = std::nullopt);
    ~ProgressBar() override;

    [[nodiscard]] float minimum() const noexcept;
    [[nodiscard]] float maximum() const noexcept;
    void setRange(float minimum, float maximum);
    [[nodiscard]] float value() const noexcept;
    ProgressBar& value(float value);
    void setValue(float value);
    ProgressBar& bind(State<float>& state);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    ProgressBar& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] bool isIndeterminate() const noexcept;
    [[nodiscard]] std::optional<float> determinateValue() const noexcept;
    ProgressBar& indeterminate(bool value) noexcept;
    void setIndeterminate(bool value) noexcept;
    [[nodiscard]] ProgressBarColor color() const noexcept;
    ProgressBar& color(ProgressBarColor value) noexcept;
    void setColor(ProgressBarColor value) noexcept;
    [[nodiscard]] ProgressBarShape shape() const noexcept;
    ProgressBar& shape(ProgressBarShape value) noexcept;
    void setShape(ProgressBarShape value) noexcept;
    [[nodiscard]] ProgressBarThickness thickness() const noexcept;
    ProgressBar& thickness(ProgressBarThickness value) noexcept;
    void setThickness(ProgressBarThickness value) noexcept;
    [[nodiscard]] bool isMotionEnabled() const noexcept;
    ProgressBar& motionEnabled(bool value) noexcept;
    void setMotionEnabled(bool value) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;

protected:
    void onAttach() noexcept override;
    void onDetach() noexcept override;

private:
    [[nodiscard]] float normalizedValue() const noexcept;
    [[nodiscard]] float clampedValue(float value) const noexcept;
    void startIndeterminateAnimation() noexcept;
    void stopIndeterminateAnimation() noexcept;

    float minimum_{0.0f};
    float maximum_{100.0f};
    float value_{0.0f};
    std::optional<Binding<float>> binding_;
    StateSubscription<float> subscription_;
    bool hasBinding_{false};
    std::string accessibleLabel_;
    bool indeterminate_{false};
    bool motionEnabled_{true};
    float indeterminatePhase_{0.0f};
    std::optional<std::size_t> animationId_;
    ProgressBarColor color_{ProgressBarColor::Brand};
    ProgressBarShape shape_{ProgressBarShape::Rounded};
    ProgressBarThickness thickness_{ProgressBarThickness::Medium};
};

enum class DividerOrientation {
    Horizontal,
    Vertical,
};

enum class DividerAppearance {
    Default,
    Subtle,
    Brand,
    Strong,
};

enum class DividerContentAlignment {
    Start,
    Center,
    End,
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
    [[nodiscard]] const std::string& content() const noexcept;
    Divider& content(std::string value);
    void setContent(std::string value);
    [[nodiscard]] DividerAppearance appearance() const noexcept;
    Divider& appearance(DividerAppearance value) noexcept;
    void setAppearance(DividerAppearance value) noexcept;
    [[nodiscard]] DividerContentAlignment contentAlignment() const noexcept;
    Divider& contentAlignment(DividerContentAlignment value) noexcept;
    void setContentAlignment(DividerContentAlignment value) noexcept;
    [[nodiscard]] bool isInset() const noexcept;
    Divider& inset(bool value) noexcept;
    void setInset(bool value) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;

private:
    DividerOrientation orientation_{DividerOrientation::Horizontal};
    float thickness_{1.0f};
    std::string content_;
    DividerAppearance appearance_{DividerAppearance::Default};
    DividerContentAlignment contentAlignment_{DividerContentAlignment::Center};
    bool inset_{false};
};

} // namespace wui
