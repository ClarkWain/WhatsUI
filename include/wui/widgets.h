#pragma once

#include <functional>
#include <string>

#include "wui/node.h"

namespace wui {

class Text : public Node {
public:
    explicit Text(std::string value = {});

    [[nodiscard]] const std::string& value() const noexcept;
    Text& value(std::string value);
    void setValue(std::string value);

    [[nodiscard]] float fontSize() const noexcept;
    void setFontSize(float size) noexcept;

    void setColor(Color color) noexcept;
    void clearColor() noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;

private:
    std::string value_;
    float fontSize_{16.0f};
    Color color_{};
    bool hasColor_{false};
};

class Spacer : public Node {
public:
    explicit Spacer(SizeF size = {}) noexcept;

    [[nodiscard]] SizeF size() const noexcept;
    void setSize(SizeF size) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;

private:
    SizeF size_{};
};

class Container : public ContainerNode {
public:
    Container& child(std::unique_ptr<Node> child);

    void setBackground(Color color) noexcept;
    void setRadius(float radius) noexcept;
    void setPadding(InsetsF padding) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;

private:
    Color background_{0, 0, 0, 0};
    float radius_{0.0f};
    InsetsF padding_{};
};

class Row : public ContainerNode {
public:
    Row& child(std::unique_ptr<Node> child);
    Row& gap(float gap) noexcept;
    void setGap(float gap) noexcept;
    [[nodiscard]] float gap() const noexcept;

    Row& padding(InsetsF padding) noexcept;
    void setPadding(InsetsF padding) noexcept;
    [[nodiscard]] InsetsF padding() const noexcept;

    Row& align(Alignment align) noexcept;
    void setAlign(Alignment align) noexcept;
    [[nodiscard]] Alignment align() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;

private:
    float gap_{0.0f};
    InsetsF padding_{};
    Alignment align_{Alignment::Start};
};

class Column : public ContainerNode {
public:
    Column& child(std::unique_ptr<Node> child);
    Column& gap(float gap) noexcept;
    void setGap(float gap) noexcept;
    [[nodiscard]] float gap() const noexcept;

    Column& padding(InsetsF padding) noexcept;
    void setPadding(InsetsF padding) noexcept;
    [[nodiscard]] InsetsF padding() const noexcept;

    Column& align(Alignment align) noexcept;
    void setAlign(Alignment align) noexcept;
    [[nodiscard]] Alignment align() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;

private:
    float gap_{0.0f};
    InsetsF padding_{};
    Alignment align_{Alignment::Start};
};

enum class ButtonVariant {
    Primary,
    Ghost,
    Danger,
};

class Button : public ControlNode {
public:
    using ClickHandler = std::function<void()>;

    explicit Button(std::string label = {});

    [[nodiscard]] const std::string& label() const noexcept;
    Button& label(std::string label);
    void setLabel(std::string label);

    Button& onClick(ClickHandler handler);

    void setVariant(ButtonVariant variant) noexcept;
    [[nodiscard]] ButtonVariant variant() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;

private:
    std::string label_;
    ClickHandler onClick_;
    ButtonVariant variant_{ButtonVariant::Primary};
};

} // namespace wui
