#include "wui/virtual_list.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

#include "wui/internal/keyed_recycler.h"
#include "wui/internal/viewport_model.h"
#include "wui/theme.h"

namespace wui {
namespace {

constexpr float kDefaultWidth = 160.0f;
constexpr float kDefaultViewportRows = 8.0f;

[[nodiscard]] bool isPrimary(const PointerEvent& event) noexcept
{
    return event.button == MouseButton::Left;
}

} // namespace

struct VirtualListNode::State {
    internal::ViewportModel viewport;
    internal::KeyedRecycler recycler;
};

VirtualListNode::VirtualListNode()
    : state_(std::make_unique<State>())
{
    state_->viewport.setItemExtent(36.0f);
    state_->viewport.setOverscanItems(2);
}

VirtualListNode::~VirtualListNode() = default;

void VirtualListNode::setItemCount(Index count)
{
    if (state_->viewport.itemCount() == count) return;
    state_->viewport.setItemCount(count);
    selectedIndex_ = normalizedSelection(selectedIndex_);
    reconcile();
    layoutMountedChildren();
    markDirty(DirtyFlag::Layout);
}

VirtualListNode::Index VirtualListNode::itemCount() const noexcept
{
    return state_->viewport.itemCount();
}

void VirtualListNode::setKeyProvider(KeyProvider provider)
{
    state_->recycler.setKeyProvider(std::move(provider));
    refresh();
}

void VirtualListNode::setItemBuilder(ItemBuilder builder)
{
    state_->recycler.setBuilder(std::move(builder));
    // A new builder represents a new row rendering contract. Existing row
    // objects may no longer be valid for it, so release mounted/pool entries
    // before materialising the viewport again.
    state_->recycler.clear(*this);
    reconcile();
    layoutMountedChildren();
    markDirty(DirtyFlag::Layout);
}

void VirtualListNode::refresh()
{
    reconcile();
    layoutMountedChildren();
    markDirty(DirtyFlag::Layout);
}

float VirtualListNode::rowExtent() const noexcept
{
    return state_->viewport.itemExtent();
}

void VirtualListNode::setRowExtent(float extent) noexcept
{
    const float next = std::isfinite(extent) ? std::max(1.0f, extent) : 36.0f;
    if (state_->viewport.itemExtent() == next) return;
    state_->viewport.setItemExtent(next);
    reconcile();
    layoutMountedChildren();
    markDirty(DirtyFlag::Layout);
}

float VirtualListNode::scrollOffset() const noexcept
{
    return state_->viewport.scrollOffset();
}

void VirtualListNode::setScrollOffset(float offset) noexcept
{
    const float previous = state_->viewport.scrollOffset();
    state_->viewport.setScrollOffset(offset);
    if (state_->viewport.scrollOffset() == previous) return;
    reconcile();
    layoutMountedChildren();
    markDirty(DirtyFlag::Paint);
}

float VirtualListNode::maxScrollOffset() const noexcept
{
    return state_->viewport.maxScrollOffset();
}

void VirtualListNode::scrollToIndex(Index index)
{
    const float previous = state_->viewport.scrollOffset();
    state_->viewport.scrollToIndex(index);
    if (state_->viewport.scrollOffset() == previous) return;
    reconcile();
    layoutMountedChildren();
    markDirty(DirtyFlag::Paint);
}

VirtualListNode::Range VirtualListNode::visibleRange() const noexcept
{
    const auto range = state_->viewport.visibleRange();
    return {range.first, range.last};
}

VirtualListNode::Index VirtualListNode::mountedCount() const noexcept
{
    return state_->recycler.mountedCount();
}

VirtualListNode::Index VirtualListNode::pooledCount() const noexcept
{
    return state_->recycler.pooledCount();
}

std::unique_ptr<Node> VirtualListNode::removeChild(std::size_t index)
{
    auto child = ControlNode::removeChild(index);
    state_->recycler.forget(child.get());
    return child;
}

void VirtualListNode::clearChildren()
{
    state_->recycler.clear(*this);
}

int VirtualListNode::selectedIndex() const noexcept
{
    return selectedIndex_;
}

void VirtualListNode::setSelectedIndex(int index)
{
    const int next = normalizedSelection(index);
    if (selectedIndex_ == next) return;
    selectedIndex_ = next;
    if (next >= 0) scrollToIndex(static_cast<Index>(next));
    markDirty(DirtyFlag::Paint);
}

VirtualListNode& VirtualListNode::onSelectionChanged(SelectionHandler handler)
{
    onSelectionChanged_ = std::move(handler);
    return *this;
}

SizeF VirtualListNode::measure(const Constraints& constraints) const
{
    const float preferredHeight = std::min(static_cast<float>(state_->viewport.itemCount()) * state_->viewport.itemExtent(), state_->viewport.itemExtent() * kDefaultViewportRows);
    return constraints.clamp({kDefaultWidth, preferredHeight});
}

void VirtualListNode::layout(const RectF& bounds)
{
    setBounds(bounds);
    state_->viewport.setViewportExtent(bounds.height);
    reconcile();
    layoutMountedChildren();
    clearDirty(DirtyFlag::Layout);
}

void VirtualListNode::paint(PaintContext& context)
{
    const Theme& current = theme();
    const bool focused = (visualStates() & toMask(ControlVisualState::Focused)) != 0;
    const float focusInset = current.controls.focusInset;
    if (focused) {
        context.strokeRoundRect(
            {bounds().x - focusInset, bounds().y - focusInset,
             bounds().width + focusInset * 2.0f,
             bounds().height + focusInset * 2.0f},
            current.radius.medium + focusInset,
            current.stroke.thick, current.colors.strokeFocusOuter);
        const float innerInset =
            std::max(0.0f,
                     focusInset - current.stroke.thin * 0.5f);
        context.strokeRoundRect(
            {bounds().x - innerInset, bounds().y - innerInset,
             bounds().width + innerInset * 2.0f,
             bounds().height + innerInset * 2.0f},
            current.radius.medium + innerInset,
            current.stroke.thin, current.colors.strokeFocusInner);
    }
    const float stroke = current.stroke.thin;
    const RectF viewport{bounds().x + stroke, bounds().y + stroke,
                         std::max(0.0f, bounds().width - stroke * 2.0f), std::max(0.0f, bounds().height - stroke * 2.0f)};
    context.fillStrokeRoundRect(bounds(), current.radius.medium,
                                stroke,
                                current.colors.neutralBackground1.rest,
                                current.colors.neutralStroke1);
    const int checkpoint = context.save();
    context.clipRect(viewport);
    ContainerNode::paint(context);
    context.restoreTo(checkpoint);
    clearDirty(DirtyFlag::Paint);
}

Node* VirtualListNode::hitTest(PointF point)
{
    // Rows are supplied by the application and may be purely visual. Routing
    // to the list makes selection deterministic and avoids stale child input
    // targets while reconciliation replaces only off-screen rows.
    return bounds().contains(point) ? this : nullptr;
}

bool VirtualListNode::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    switch (event.action) {
    case PointerAction::Down:
        if (!isPrimary(event)) return false;
        pressedIndex_ = rowAt(event.position);
        setVisualState(ControlVisualState::Pressed, true);
        return true;
    case PointerAction::Up: {
        if (!isPrimary(event)) return false;
        const int pressed = pressedIndex_;
        pressedIndex_ = -1;
        setVisualState(ControlVisualState::Pressed, false);
        const int row = rowAt(event.position);
        if (pressed >= 0 && pressed == row) select(row);
        return true;
    }
    case PointerAction::Cancel:
        pressedIndex_ = -1;
        setVisualState(ControlVisualState::Pressed, false);
        return true;
    case PointerAction::Scroll:
        setScrollOffset(state_->viewport.scrollOffset() - event.scrollDelta.y);
        return true;
    case PointerAction::Move:
    case PointerAction::Enter:
    case PointerAction::Leave:
        return true;
    }
    return false;
}

bool VirtualListNode::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || event.action != KeyAction::Down || state_->viewport.itemCount() == 0) return false;
    int next = selectedIndex_;
    switch (event.keyCode) {
    case 38: // Up
        next = selectedIndex_ < 0 ? static_cast<int>(state_->viewport.itemCount() - 1) : std::max(0, selectedIndex_ - 1);
        break;
    case 40: // Down
        next = selectedIndex_ < 0 ? 0 : std::min(static_cast<int>(state_->viewport.itemCount() - 1), selectedIndex_ + 1);
        break;
    case 36: // Home
        next = 0;
        break;
    case 35: // End
        next = static_cast<int>(state_->viewport.itemCount() - 1);
        break;
    default:
        return false;
    }
    select(next);
    return true;
}

VirtualListNode::Range VirtualListNode::mountedRange() const noexcept
{
    const auto range = state_->viewport.overscanRange();
    return {range.first, range.last};
}

int VirtualListNode::rowAt(PointF point) const noexcept
{
    if (!bounds().contains(point) || state_->viewport.itemExtent() <= 0.0f) return -1;
    const float contentY = point.y - bounds().y + state_->viewport.scrollOffset();
    if (contentY < 0.0f) return -1;
    const Index index = static_cast<Index>(contentY / state_->viewport.itemExtent());
    return index < state_->viewport.itemCount() && index <= static_cast<Index>(std::numeric_limits<int>::max()) ? static_cast<int>(index) : -1;
}

int VirtualListNode::normalizedSelection(int index) const noexcept
{
    return index >= 0 && static_cast<Index>(index) < state_->viewport.itemCount() ? index : -1;
}

void VirtualListNode::reconcile()
{
    const Range range = mountedRange();
    state_->recycler.reconcile(*this, {range.first, range.last});
}

void VirtualListNode::layoutMountedChildren()
{
    for (const auto& mounted : state_->recycler.mounted()) {
        mounted.node->layout({bounds().x + 1.0f,
                              bounds().y + static_cast<float>(mounted.index) * state_->viewport.itemExtent() - state_->viewport.scrollOffset(),
                              std::max(0.0f, bounds().width - 2.0f), state_->viewport.itemExtent()});
    }
}

void VirtualListNode::select(int index)
{
    if (index < 0 || static_cast<Index>(index) >= state_->viewport.itemCount() || index == selectedIndex_) return;
    selectedIndex_ = index;
    scrollToIndex(static_cast<Index>(index));
    if (onSelectionChanged_) onSelectionChanged_(static_cast<Index>(index));
    markDirty(DirtyFlag::Paint);
}

} // namespace wui
