#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
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

    // A node becomes attached when a UiRoot or OverlayHost adopts it.  Nodes
    // are detached before they are removed or destroyed by those owners.  The
    // callbacks are deliberately separate from destruction: a detached node
    // may be retained and attached to a different tree later.
    //
    // Lifecycle callbacks must not throw.  They are intended for resources
    // whose lifetime follows tree membership, such as State subscriptions.
    void addAttachCallback(std::function<void()> callback);
    void addDetachCallback(std::function<void()> callback);

    [[nodiscard]] bool isAttached() const noexcept
    {
        return attached_;
    }

    // Register a callback that runs when this node is destroyed. Reactive
    // builders use it to unsubscribe from a State, so a State outliving the
    // node cannot call into freed memory.
    void addTeardown(std::function<void()> callback);

    void setInvalidationHandler(std::function<void()> handler);

    [[nodiscard]] virtual SizeF measure(const Constraints& constraints) const = 0;
    // Layout containers call this instead of measure() when they size a
    // child. It preserves the exact most-recent constraint envelope for
    // read-only diagnostics without changing the virtual measurement API.
    [[nodiscard]] SizeF measureWithConstraints(const Constraints& constraints) const;
    [[nodiscard]] const std::optional<Constraints>& lastMeasuredConstraints() const noexcept
    {
        return lastMeasuredConstraints_;
    }
    [[nodiscard]] virtual float baselineOffset() const noexcept;
    // Prepare backend resources before beginFrame(). The default implementation
    // walks children so leaf widgets only override when they own resources.
    virtual void prepare(PaintContext& context);
    virtual void layout(const RectF& bounds);
    virtual void paint(PaintContext& context) = 0;
    [[nodiscard]] virtual Node* hitTest(PointF point);
    // Compatibility adapter: established widgets may continue overriding the
    // bool overload while new nodes can inspect the routing phase and make
    // explicit focus/capture/propagation requests.
    virtual EventResult onPointerEvent(const PointerEvent& event, EventContext& context);
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

    // Override these only for node-local resources.  Use the callback API for
    // bindings created by builders, where the subscription lifetime is owned
    // by the builder rather than the widget subclass.
    virtual void onAttach() noexcept {}
    virtual void onDetach() noexcept {}

    void setBounds(const RectF& bounds) noexcept
    {
        bounds_ = bounds;
    }

private:
    friend class UiRoot;
    friend class OverlayHost;

    void attachRecursively();
    void detachRecursively() noexcept;

    Node* parent_{nullptr};
    std::vector<std::unique_ptr<Node>> children_;
    std::vector<std::function<void()>> attachCallbacks_;
    std::vector<std::function<void()>> detachCallbacks_;
    std::vector<std::function<void()>> teardown_;
    std::function<void()> invalidationHandler_;
    RectF bounds_{};
    float flex_{0.0f};
    bool attached_{false};
    DirtyFlags dirtyFlags_{toMask(DirtyFlag::Layout) | toMask(DirtyFlag::Paint)};
    mutable std::optional<Constraints> lastMeasuredConstraints_;
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
