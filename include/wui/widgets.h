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

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;

private:
    std::string value_;
    float fontSize_{16.0f};
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

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
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

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;

private:
    float gap_{0.0f};
    InsetsF padding_{};
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

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;

private:
    float gap_{0.0f};
    InsetsF padding_{};
};

class Button : public ControlNode {
public:
    using ClickHandler = std::function<void()>;

    explicit Button(std::string label = {});

    [[nodiscard]] const std::string& label() const noexcept;
    Button& label(std::string label);
    void setLabel(std::string label);

    Button& onClick(ClickHandler handler);

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;

private:
    std::string label_;
    ClickHandler onClick_;
};

} // namespace wui
