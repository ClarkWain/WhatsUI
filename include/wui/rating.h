#pragma once

// Fluent 2 rating controls. Rating is the interactive input; RatingDisplay is
// deliberately passive and should be used when presenting an aggregate value.

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "wui/node.h"
#include "wui/state.h"

namespace wui {

enum class RatingColor { Neutral, Brand, Marigold };
enum class RatingSize { Small, Medium, Large, ExtraLarge };
enum class RatingShape { Star, Circle, Square };

class Rating : public ControlNode {
public:
    using ChangeHandler = std::function<void(float)>;
    using ItemLabelHandler = std::function<std::string(float)>;

    explicit Rating(float value = 0.0f, int maximum = 5);

    [[nodiscard]] float value() const noexcept;
    Rating& value(float value);
    void setValue(float value);
    [[nodiscard]] int maximum() const noexcept;
    void setMaximum(int maximum);
    [[nodiscard]] float step() const noexcept;
    Rating& step(float step);
    void setStep(float step);
    [[nodiscard]] RatingColor color() const noexcept;
    void setColor(RatingColor color) noexcept;
    [[nodiscard]] RatingSize size() const noexcept;
    void setSize(RatingSize size) noexcept;
    [[nodiscard]] RatingShape shape() const noexcept;
    void setShape(RatingShape shape) noexcept;
    [[nodiscard]] bool isReadOnly() const noexcept;
    void setReadOnly(bool value) noexcept;
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    void setAccessibleLabel(std::string label);
    Rating& itemLabel(ItemLabelHandler handler);
    void setItemLabel(ItemLabelHandler handler);
    [[nodiscard]] std::string labelForValue(float value) const;
    [[nodiscard]] std::string accessibleValueText() const;
    Rating& bind(State<float>& state);
    Rating& onChange(ChangeHandler handler);

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    [[nodiscard]] float normalized(float value) const noexcept;
    [[nodiscard]] float valueAt(float x) const noexcept;
    void commit(float value);

    float value_{0.0f};
    int maximum_{5};
    float step_{1.0f};
    RatingColor color_{RatingColor::Neutral};
    RatingSize size_{RatingSize::ExtraLarge};
    RatingShape shape_{RatingShape::Star};
    bool readOnly_{false};
    std::string accessibleLabel_{"Rating"};
    std::optional<float> hoveredValue_;
    std::optional<Binding<float>> binding_;
    StateSubscription<float> subscription_;
    bool hasBinding_{false};
    ChangeHandler onChange_;
    ItemLabelHandler itemLabel_;
};

class RatingDisplay : public Node {
public:
    using CountFormatter = std::function<std::string(std::uint64_t)>;
    explicit RatingDisplay(std::optional<float> value = std::optional<float>{0.0f}, int maximum = 5);

    [[nodiscard]] std::optional<float> value() const noexcept;
    RatingDisplay& value(float value);
    void setValue(std::optional<float> value) noexcept;
    [[nodiscard]] int maximum() const noexcept;
    void setMaximum(int maximum) noexcept;
    [[nodiscard]] std::optional<std::uint64_t> count() const noexcept;
    void setCount(std::optional<std::uint64_t> count) noexcept;
    RatingDisplay& countFormatter(CountFormatter formatter);
    void setCountFormatter(CountFormatter formatter);
    [[nodiscard]] bool isCompact() const noexcept;
    void setCompact(bool compact) noexcept;
    [[nodiscard]] RatingColor color() const noexcept;
    void setColor(RatingColor color) noexcept;
    [[nodiscard]] RatingSize size() const noexcept;
    void setSize(RatingSize size) noexcept;
    [[nodiscard]] RatingShape shape() const noexcept;
    void setShape(RatingShape shape) noexcept;
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    void setAccessibleLabel(std::string label);

    [[nodiscard]] std::string valueText() const;
    [[nodiscard]] std::string countText() const;
    [[nodiscard]] std::string generatedAccessibleLabel() const;
    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;

private:
    std::optional<float> value_;
    std::optional<std::uint64_t> count_;
    int maximum_{5};
    RatingColor color_{RatingColor::Neutral};
    RatingSize size_{RatingSize::Medium};
    RatingShape shape_{RatingShape::Star};
    bool compact_{false};
    std::string accessibleLabel_;
    CountFormatter countFormatter_;
};

} // namespace wui
