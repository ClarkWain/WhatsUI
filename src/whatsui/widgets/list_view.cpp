#include "wui/list_view.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "wui/theme.h"

namespace wui {
namespace {

constexpr float kHorizontalPadding = 12.0f;
constexpr float kDefaultWidth = 160.0f;

[[nodiscard]] bool isPrimary(const PointerEvent& event) noexcept
{
    return event.button == MouseButton::Left;
}

[[nodiscard]] bool intersects(const RectF& left, const RectF& right) noexcept
{
    return left.x < right.x + right.width && left.x + left.width > right.x &&
        left.y < right.y + right.height && left.y + left.height > right.y;
}

} // namespace

ListView::ListView(std::vector<Item> items, int selectedIndex)
    : items_(std::move(items))
    , selectedIndex_(normalizedSelection(selectedIndex))
{
}

const std::vector<ListView::Item>& ListView::items() const noexcept
{
    return items_;
}

void ListView::setItems(std::vector<Item> items)
{
    items_ = std::move(items);
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    setSelectedIndex(selectedIndex());
    markDirty(DirtyFlag::Layout);
}

void ListView::appendItem(Item item)
{
    items_.push_back(std::move(item));
    markDirty(DirtyFlag::Layout);
}

void ListView::clearItems()
{
    if (items_.empty()) return;
    items_.clear();
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    setSelectedIndex(-1);
    markDirty(DirtyFlag::Layout);
}

int ListView::selectedIndex() const noexcept
{
    const int value = hasBinding_ ? binding_->get() : selectedIndex_;
    return isSelectable(value) ? value : -1;
}

ListView& ListView::selectedIndex(int index)
{
    setSelectedIndex(index);
    return *this;
}

void ListView::setSelectedIndex(int index)
{
    const int next = normalizedSelection(index);
    if (hasBinding_) {
        binding_->set(next);
    } else if (selectedIndex_ != next) {
        selectedIndex_ = next;
        markDirty(DirtyFlag::Paint);
    }
}

ListView& ListView::bind(State<int>& state)
{
    binding_.emplace(state);
    hasBinding_ = true;
    selectedIndex_ = normalizedSelection(state.get());
    const auto id = state.subscribe([this](int value) {
        selectedIndex_ = normalizedSelection(value);
        markDirty(DirtyFlag::Paint);
    });
    addTeardown([&state, id] { state.unsubscribe(id); });
    // Normalize an externally supplied disabled/out-of-range value so the
    // model and visible selection agree from first attachment onward.
    setSelectedIndex(selectedIndex_);
    markDirty(DirtyFlag::Paint);
    return *this;
}

ListView& ListView::onSelectionChanged(SelectionHandler handler)
{
    onSelectionChanged_ = std::move(handler);
    return *this;
}

float ListView::rowHeight() const noexcept
{
    return rowHeight_;
}

void ListView::setRowHeight(float value) noexcept
{
    const float next = std::isfinite(value) ? std::max(24.0f, value) : 36.0f;
    if (rowHeight_ != next) {
        rowHeight_ = next;
        markDirty(DirtyFlag::Layout);
    }
}

SizeF ListView::measure(const Constraints& constraints) const
{
    return constraints.clamp({preferredWidth(), rowHeight_ * static_cast<float>(items_.size())});
}

void ListView::paint(PaintContext& context)
{
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
    for (std::size_t index = 0; index < items_.size(); ++index) {
        const RectF row{content.x, content.y + static_cast<float>(index) * rowHeight_, content.width, rowHeight_};
        if (!intersects(row, content)) continue;
        const bool itemEnabled = enabled && items_[index].enabled;
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
            items_[index].label, row.x + kHorizontalPadding,
            context.centeredTextBottom(
                items_[index].label, row,
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

bool ListView::onPointerEvent(const PointerEvent& event)
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
    case PointerAction::Scroll:
        return false;
    }
    return false;
}

bool ListView::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || event.action != KeyAction::Down) return false;
    const int current = selectedIndex();
    int next = -1;
    switch (event.keyCode) {
    case 38: // Up
        next = nextEnabled(current < 0 ? static_cast<int>(items_.size()) : current - 1, -1);
        break;
    case 40: // Down
        next = nextEnabled(current < 0 ? -1 : current + 1, 1);
        break;
    case 36: // Home
        next = nextEnabled(-1, 1);
        break;
    case 35: // End
        next = nextEnabled(static_cast<int>(items_.size()), -1);
        break;
    default:
        return false;
    }
    if (next >= 0) select(next);
    return true;
}

bool ListView::isSelectable(int index) const noexcept
{
    return index >= 0 && static_cast<std::size_t>(index) < items_.size() && items_[static_cast<std::size_t>(index)].enabled;
}

int ListView::normalizedSelection(int index) const noexcept
{
    return isSelectable(index) ? index : -1;
}

int ListView::rowAt(PointF point) const noexcept
{
    if (!bounds().contains(point) || rowHeight_ <= 0.0f) return -1;
    const float contentY = point.y - bounds().y - 1.0f;
    if (contentY < 0.0f) return -1;
    const int index = static_cast<int>(contentY / rowHeight_);
    return index >= 0 && static_cast<std::size_t>(index) < items_.size() ? index : -1;
}

int ListView::nextEnabled(int from, int direction) const noexcept
{
    if (direction == 0) return -1;
    for (int index = from + direction; index >= 0 && static_cast<std::size_t>(index) < items_.size(); index += direction) {
        if (isSelectable(index)) return index;
    }
    return -1;
}

float ListView::preferredWidth() const noexcept
{
    const float characterWidth = theme().typography.body1.size * 0.56f;
    float width = kDefaultWidth;
    for (const Item& item : items_) {
        width = std::max(width, static_cast<float>(item.label.size()) * characterWidth + kHorizontalPadding * 2.0f);
    }
    return width;
}

void ListView::select(int index)
{
    if (!isSelectable(index) || index == selectedIndex()) return;
    setSelectedIndex(index);
    if (onSelectionChanged_) onSelectionChanged_(index);
}

} // namespace wui
