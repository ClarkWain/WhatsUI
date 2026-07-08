#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "wui/events.h"
#include "wui/paint_context.h"
#include "wui/types.h"

namespace wui {

enum class ControlVisualState : std::uint32_t {
    None = 0,
    Hovered = 1u << 0,
    Pressed = 1u << 1,
    Focused = 1u << 2,
    Disabled = 1u << 3,
};

using ControlVisualStates = std::uint32_t;

constexpr ControlVisualStates toMask(ControlVisualState state) noexcept
{
    return static_cast<ControlVisualStates>(state);
}

class Node {
public:
    virtual ~Node();

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    [[nodiscard]] Node* parent() const noexcept
    {
        return parent_;
    }

    [[nodiscard]] const std::vector<std::unique_ptr<Node>>& children() const noexcept
    {
        return children_;
    }

    void appendChild(std::unique_ptr<Node> child);
    [[nodiscard]] std::unique_ptr<Node> removeChild(std::size_t index);

    [[nodiscard]] virtual SizeF measure(const Constraints& constraints) const = 0;
    virtual void layout(const RectF& bounds);
    virtual void paint(PaintContext& context) = 0;
    [[nodiscard]] virtual Node* hitTest(PointF point);
    virtual bool onPointerEvent(const PointerEvent& event);
    virtual bool onKeyEvent(const KeyEvent& event);
    virtual bool onTextInput(const TextInputEvent& event);
    virtual bool onCompositionInput(const CompositionInputEvent& event);

    void markDirty(DirtyFlag flag) noexcept;

    [[nodiscard]] bool isDirty(DirtyFlag flag) const noexcept
    {
        return (dirtyFlags_ & toMask(flag)) != 0;
    }

    void clearDirty(DirtyFlag flag) noexcept
    {
        dirtyFlags_ &= ~toMask(flag);
    }

    void clearDirty() noexcept
    {
        dirtyFlags_ = toMask(DirtyFlag::None);
    }

    [[nodiscard]] const RectF& bounds() const noexcept
    {
        return bounds_;
    }

protected:
    Node() = default;

    void setBounds(const RectF& bounds) noexcept
    {
        bounds_ = bounds;
    }

private:
    Node* parent_{nullptr};
    std::vector<std::unique_ptr<Node>> children_;
    RectF bounds_{};
    DirtyFlags dirtyFlags_{toMask(DirtyFlag::Layout) | toMask(DirtyFlag::Paint)};
};

class ContainerNode : public Node {
public:
    void paint(PaintContext& context) override;
    [[nodiscard]] Node* hitTest(PointF point) override;
};

class ControlNode : public ContainerNode {
public:
    [[nodiscard]] bool isEnabled() const noexcept
    {
        return (visualStates_ & toMask(ControlVisualState::Disabled)) == 0;
    }

    void setEnabled(bool enabled) noexcept;

    [[nodiscard]] ControlVisualStates visualStates() const noexcept
    {
        return visualStates_;
    }

    void setVisualState(ControlVisualState state, bool value) noexcept;

private:
    ControlVisualStates visualStates_{0};
};

} // namespace wui
