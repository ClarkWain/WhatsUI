#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
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
    void clearChildren();

    // Register a callback that runs when this node is destroyed. Reactive
    // builders use it to unsubscribe from a State, so a State outliving the
    // node cannot call into freed memory.
    void addTeardown(std::function<void()> callback);

    void setInvalidationHandler(std::function<void()> handler);

    [[nodiscard]] virtual SizeF measure(const Constraints& constraints) const = 0;
    [[nodiscard]] virtual float baselineOffset() const noexcept;
    // Prepare backend resources before beginFrame(). The default implementation
    // walks children so leaf widgets only override when they own resources.
    virtual void prepare(PaintContext& context);
    virtual void layout(const RectF& bounds);
    virtual void paint(PaintContext& context) = 0;
    [[nodiscard]] virtual Node* hitTest(PointF point);
    virtual bool onPointerEvent(const PointerEvent& event);
    virtual bool onKeyEvent(const KeyEvent& event);
    virtual bool onTextInput(const TextInputEvent& event);
    virtual bool onCompositionInput(const CompositionInputEvent& event);

    // A layout change also changes the pixels occupied by this node and its
    // ancestors, so it implicitly invalidates paint up to the root boundary.
    // Paint-only changes leave layout validity intact.
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

    // A composite calls this after it has assigned bounds to its complete
    // subtree.  This is deliberately separate from clearDirty(Layout): a
    // parent must not claim a descendant is laid out until that descendant
    // has received its final bounds.
    void clearLayoutDirtyRecursively() noexcept;

    [[nodiscard]] const RectF& bounds() const noexcept
    {
        return bounds_;
    }

    // Main-axis flex weight for Row/Column layout. 0 = fixed (measured) size;
    // >0 = share of the remaining main-axis space, proportional to weight.
    [[nodiscard]] float flex() const noexcept
    {
        return flex_;
    }

    void setFlex(float flex) noexcept
    {
        flex_ = flex;
        markDirty(DirtyFlag::Layout);
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
    std::vector<std::function<void()>> teardown_;
    std::function<void()> invalidationHandler_;
    RectF bounds_{};
    float flex_{0.0f};
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
