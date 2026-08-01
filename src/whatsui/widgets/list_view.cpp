#include "wui/list_view.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "wui/internal/viewport_model.h"
#include "wui/theme.h"

namespace wui {
namespace {

constexpr float kHorizontalPadding = 12.0f;
constexpr float kDefaultWidth = 160.0f;

[[nodiscard]] bool isPrimary(const PointerEvent& event) noexcept
{
    return event.button == MouseButton::Left;
}

} // namespace

struct ListViewNode::State {
    internal::ViewportModel viewport;
};

ListViewNode::ListViewNode(std::vector<Item> items, int selectedIndex)
    : items_(std::move(items))
    , selectedIndex_(normalizedSelection(selectedIndex))
    , state_(std::make_unique<State>())
{
    syncViewport();
}

ListViewNode::~ListViewNode() = default;

const std::vector<ListViewNode::Item>& ListViewNode::items() const noexcept
{
    return items_;
}

std::size_t ListViewNode::itemCount() const noexcept
{
    return usesItemProvider_ ? providerItemCount_ : items_.size();
}

void ListViewNode::setItems(std::vector<Item> items)
{
    usesItemProvider_ = false;
    itemProvider_ = nullptr;
    selectableProvider_ = nullptr;
    providerItemCount_ = 0;
    items_ = std::move(items);
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    syncViewport();
    setSelectedIndex(selectedIndex());
    markDirty(DirtyFlag::Layout);
}

void ListViewNode::setItemProvider(std::size_t count, ItemProvider provider, SelectableProvider selectable)
{
    usesItemProvider_ = static_cast<bool>(provider);
    itemProvider_ = std::move(provider);
    selectableProvider_ = std::move(selectable);
    providerItemCount_ = usesItemProvider_ ? count : 0;
    if (usesItemProvider_) items_.clear();
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    syncViewport();
    setSelectedIndex(selectedIndex());
    markDirty(DirtyFlag::Layout);
}

void ListViewNode::appendItem(Item item)
{
    if (usesItemProvider_) {
        setItems({std::move(item)});
        return;
    }
    items_.push_back(std::move(item));
    syncViewport();
    markDirty(DirtyFlag::Layout);
}

void ListViewNode::clearItems()
{
    if (!usesItemProvider_ && items_.empty()) return;
    usesItemProvider_ = false;
    itemProvider_ = nullptr;
    selectableProvider_ = nullptr;
    providerItemCount_ = 0;
    items_.clear();
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    syncViewport();
    state_->viewport.setScrollOffset(0.0f);
    setSelectedIndex(-1);
    markDirty(DirtyFlag::Layout);
}

int ListViewNode::selectedIndex() const noexcept
{
    const int value = hasBinding_ ? binding_->get() : selectedIndex_;
    return isSelectable(value) ? value : -1;
}

ListViewNode& ListViewNode::selectedIndex(int index)
{
    setSelectedIndex(index);
    return *this;
}

void ListViewNode::setSelectedIndex(int index)
{
    const int next = normalizedSelection(index);
    if (hasBinding_) {
        binding_->set(next);
    } else if (selectedIndex_ != next) {
        selectedIndex_ = next;
        if (next >= 0) state_->viewport.scrollToIndex(static_cast<std::size_t>(next));
        markDirty(DirtyFlag::Paint);
    }
}

ListViewNode& ListViewNode::bind(wui::State<int>& state)
{
    binding_.emplace(state);
    hasBinding_ = true;
    selectedIndex_ = normalizedSelection(state.get());
    const auto id = state.subscribe([this](int value) {
        selectedIndex_ = normalizedSelection(value);
        if (selectedIndex_ >= 0) state_->viewport.scrollToIndex(static_cast<std::size_t>(selectedIndex_));
        markDirty(DirtyFlag::Paint);
    });
    addTeardown([&state, id] { state.unsubscribe(id); });
    // Normalize an externally supplied disabled/out-of-range value so the
    // model and visible selection agree from first attachment onward.
    setSelectedIndex(selectedIndex_);
    markDirty(DirtyFlag::Paint);
    return *this;
}

ListViewNode& ListViewNode::onSelectionChanged(SelectionHandler handler)
{
    onSelectionChanged_ = std::move(handler);
    return *this;
}

float ListViewNode::rowHeight() const noexcept
{
    return rowHeight_;
}

void ListViewNode::setRowHeight(float value) noexcept
{
    const float next = std::isfinite(value) ? std::max(24.0f, value) : 36.0f;
    if (rowHeight_ != next) {
        rowHeight_ = next;
        syncViewport();
        markDirty(DirtyFlag::Layout);
    }
}

float ListViewNode::scrollOffset() const noexcept
{
    return state_->viewport.scrollOffset();
}

void ListViewNode::setScrollOffset(float value) noexcept
{
    syncViewport();
    const float previous = state_->viewport.scrollOffset();
    state_->viewport.setScrollOffset(value);
    if (state_->viewport.scrollOffset() != previous) markDirty(DirtyFlag::Paint);
}

float ListViewNode::maximumScrollOffset() const noexcept
{
    return state_->viewport.maxScrollOffset();
}

ListViewNode::Range ListViewNode::visibleRange() const noexcept
{
    const auto range = state_->viewport.visibleRange();
    return {range.first, range.last};
}

SizeF ListViewNode::measure(const Constraints& constraints) const
{
    return constraints.clamp({preferredWidth(), rowHeight_ * static_cast<float>(itemCount())});
}

void ListViewNode::layout(const RectF& bounds)
{
    Node::layout(bounds);
    syncViewport();
    clearLayoutDirtyRecursively();
}

void ListViewNode::paint(PaintContext& context)
{
    syncViewport();
    const Theme& current = theme();
    const bool enabled = isEnabled();
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
    const RectF content{bounds().x + stroke, bounds().y + stroke,
                        std::max(0.0f, bounds().width - stroke * 2.0f), std::max(0.0f, bounds().height - stroke * 2.0f)};
    context.fillStrokeRoundRect(bounds(), current.radius.medium,
                                stroke,
                                current.colors.neutralBackground1.rest,
                                current.colors.neutralStroke1);

    const int checkpoint = context.save();
    context.clipRect(content);
    const int selected = selectedIndex();
    const Range visible = visibleRange();
    for (std::size_t index = visible.first; index < visible.last; ++index) {
        const Item item = itemAt(index);
        const RectF row{content.x, content.y + static_cast<float>(index) * rowHeight_ - state_->viewport.scrollOffset(), content.width, rowHeight_};
        const bool rowSelectable =
            usesItemProvider_ && selectableProvider_
                ? providerItemSelectable(index)
                : item.enabled;
        const bool itemEnabled = enabled && rowSelectable;
        const bool rowSelected = static_cast<int>(index) == selected;
        const bool rowPressed = static_cast<int>(index) == pressedIndex_;
        const bool rowHovered = static_cast<int>(index) == hoveredIndex_;
        Color fill = current.colors.neutralBackground1.rest;
        if (rowSelected) fill = Color{230, 244, 253, 255};
        if (itemEnabled && rowHovered) fill = rowSelected ? Color{217, 237, 249, 255} : current.colors.surfaceHover;
        if (itemEnabled && rowPressed) fill = rowSelected ? Color{205, 230, 246, 255} : current.colors.surfacePressed;
        context.fillRect(row, fill);
        const Color text =
            itemEnabled ? current.colors.neutralForeground1
                        : current.colors.neutralForegroundDisabled;
        context.drawText(
            item.label, row.x + kHorizontalPadding,
            context.centeredTextBottom(
                item.label, row,
                current.typography.body1.size,
                current.typography.body1.weight,
                current.typography.body1.family),
            current.typography.body1.size, text,
            current.typography.body1.weight,
            current.typography.body1.family);
    }
    context.restoreTo(checkpoint);
    clearDirty(DirtyFlag::Paint);
}

bool ListViewNode::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    const int row = rowAt(event.position);
    switch (event.action) {
    case PointerAction::Enter:
    case PointerAction::Move:
        hoveredIndex_ = row;
        markDirty(DirtyFlag::Paint);
        return true;
    case PointerAction::Leave:
        hoveredIndex_ = -1;
        markDirty(DirtyFlag::Paint);
        return true;
    case PointerAction::Down:
        if (!isPrimary(event)) return false;
        pressedIndex_ = row;
        setVisualState(ControlVisualState::Pressed, true);
        markDirty(DirtyFlag::Paint);
        return true;
    case PointerAction::Up: {
        if (!isPrimary(event)) return false;
        const int pressed = pressedIndex_;
        pressedIndex_ = -1;
        setVisualState(ControlVisualState::Pressed, false);
        if (pressed >= 0 && pressed == row && isSelectable(row)) select(row);
        markDirty(DirtyFlag::Paint);
        return true;
    }
    case PointerAction::Cancel:
        pressedIndex_ = -1;
        setVisualState(ControlVisualState::Pressed, false);
        markDirty(DirtyFlag::Paint);
        return true;
    case PointerAction::Scroll: {
        const float previous = state_->viewport.scrollOffset();
        setScrollOffset(previous - event.scrollDelta.y);
        return state_->viewport.scrollOffset() != previous;
    }
    }
    return false;
}

EventResult ListViewNode::onPointerEvent(const PointerEvent& event, EventContext& context)
{
    if (context.phase() == EventPhase::Capture || event.action != PointerAction::Scroll) {
        return ControlNode::onPointerEvent(event, context);
    }
    if (!isEnabled()) return EventResult::Ignored;

    const float previous = state_->viewport.scrollOffset();
    setScrollOffset(previous - event.scrollDelta.y);
    const float applied = state_->viewport.scrollOffset() - previous;
    context.setRemainingScrollDelta(
        {event.scrollDelta.x, event.scrollDelta.y + applied});
    return applied != 0.0f ? EventResult::Handled : EventResult::Ignored;
}

bool ListViewNode::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || event.action != KeyAction::Down) return false;
    const int current = selectedIndex();
    int next = -1;
    switch (event.keyCode) {
    case 38: // Up
        next = nextEnabled(current < 0 ? static_cast<int>(itemCount()) : current - 1, -1);
        break;
    case 40: // Down
        next = nextEnabled(current < 0 ? -1 : current + 1, 1);
        break;
    case 36: // Home
        next = nextEnabled(-1, 1);
        break;
    case 35: // End
        next = nextEnabled(static_cast<int>(itemCount()), -1);
        break;
    default:
        return false;
    }
    if (next >= 0) select(next);
    return true;
}

bool ListViewNode::isSelectable(int index) const noexcept
{
    if (index < 0 || static_cast<std::size_t>(index) >= itemCount()) return false;
    const auto itemIndex = static_cast<std::size_t>(index);
    return usesItemProvider_ ? providerItemSelectable(itemIndex) : items_[itemIndex].enabled;
}

int ListViewNode::normalizedSelection(int index) const noexcept
{
    return isSelectable(index) ? index : -1;
}

int ListViewNode::rowAt(PointF point) const noexcept
{
    if (!bounds().contains(point) || rowHeight_ <= 0.0f) return -1;
    const float contentY = point.y - bounds().y - 1.0f + state_->viewport.scrollOffset();
    if (contentY < 0.0f) return -1;
    const int index = static_cast<int>(contentY / rowHeight_);
    return index >= 0 && static_cast<std::size_t>(index) < itemCount() ? index : -1;
}

int ListViewNode::nextEnabled(int from, int direction) const noexcept
{
    if (direction == 0) return -1;
    for (int index = from + direction; index >= 0 && static_cast<std::size_t>(index) < itemCount(); index += direction) {
        if (isSelectable(index)) return index;
    }
    return -1;
}

float ListViewNode::preferredWidth() const noexcept
{
    if (usesItemProvider_) return kDefaultWidth;
    const float characterWidth = theme().typography.body1.size * 0.56f;
    float width = kDefaultWidth;
    for (const Item& item : items_) {
        width = std::max(width, static_cast<float>(item.label.size()) * characterWidth + kHorizontalPadding * 2.0f);
    }
    return width;
}

ListViewNode::Item ListViewNode::itemAt(std::size_t index) const
{
    if (usesItemProvider_ && itemProvider_ && index < providerItemCount_) return itemProvider_(index);
    return index < items_.size() ? items_[index] : Item{};
}

bool ListViewNode::providerItemSelectable(std::size_t index) const noexcept
{
    try {
        return selectableProvider_ ? selectableProvider_(index) : itemAt(index).enabled;
    } catch (...) {
        // Selection queries are used by noexcept public diagnostics and input
        // routing. A faulty provider must not terminate the UI process or
        // accidentally make a row interactive.
        return false;
    }
}

void ListViewNode::syncViewport() noexcept
{
    state_->viewport.setItemCount(itemCount());
    state_->viewport.setItemExtent(rowHeight_);
    state_->viewport.setViewportExtent(std::max(0.0f, bounds().height - theme().stroke.thin * 2.0f));
}

void ListViewNode::select(int index)
{
    if (!isSelectable(index) || index == selectedIndex()) return;
    setSelectedIndex(index);
    state_->viewport.scrollToIndex(static_cast<std::size_t>(index));
    if (onSelectionChanged_) onSelectionChanged_(index);
}

} // namespace wui
