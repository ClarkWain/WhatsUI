#include "wui/virtual_list.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

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

VirtualList::VirtualList()
    : keyProvider_([](Index index) { return std::to_string(index); })
{
}

void VirtualList::setItemCount(Index count)
{
    if (itemCount_ == count) return;
    itemCount_ = count;
    selectedIndex_ = normalizedSelection(selectedIndex_);
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maxScrollOffset());
    reconcile();
    layoutMountedChildren();
    markDirty(DirtyFlag::Layout);
}

VirtualList::Index VirtualList::itemCount() const noexcept
{
    return itemCount_;
}

void VirtualList::setKeyProvider(KeyProvider provider)
{
    keyProvider_ = provider ? std::move(provider) : KeyProvider([](Index index) { return std::to_string(index); });
    refresh();
}

void VirtualList::setItemBuilder(ItemBuilder builder)
{
    itemBuilder_ = std::move(builder);
    // A new builder represents a new row rendering contract. Existing row
    // objects may no longer be valid for it, so release mounted/pool entries
    // before materialising the viewport again.
    mounted_.clear();
    clearChildren();
    pool_.clear();
    reconcile();
    layoutMountedChildren();
    markDirty(DirtyFlag::Layout);
}

void VirtualList::refresh()
{
    reconcile();
    layoutMountedChildren();
    markDirty(DirtyFlag::Layout);
}

float VirtualList::rowExtent() const noexcept
{
    return rowExtent_;
}

void VirtualList::setRowExtent(float extent) noexcept
{
    const float next = std::isfinite(extent) ? std::max(1.0f, extent) : 36.0f;
    if (rowExtent_ == next) return;
    rowExtent_ = next;
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maxScrollOffset());
    reconcile();
    layoutMountedChildren();
    markDirty(DirtyFlag::Layout);
}

float VirtualList::scrollOffset() const noexcept
{
    return scrollOffset_;
}

void VirtualList::setScrollOffset(float offset) noexcept
{
    if (!std::isfinite(offset)) offset = 0.0f;
    const float next = std::clamp(offset, 0.0f, maxScrollOffset());
    if (scrollOffset_ == next) return;
    scrollOffset_ = next;
    reconcile();
    layoutMountedChildren();
    markDirty(DirtyFlag::Paint);
}

float VirtualList::maxScrollOffset() const noexcept
{
    return std::max(0.0f, static_cast<float>(itemCount_) * rowExtent_ - bounds().height);
}

void VirtualList::scrollToIndex(Index index)
{
    if (index >= itemCount_) return;
    const float top = static_cast<float>(index) * rowExtent_;
    const float bottom = top + rowExtent_;
    if (top < scrollOffset_) setScrollOffset(top);
    else if (bottom > scrollOffset_ + bounds().height) setScrollOffset(bottom - bounds().height);
}

VirtualList::Range VirtualList::visibleRange() const noexcept
{
    if (itemCount_ == 0 || bounds().height <= 0.0f || rowExtent_ <= 0.0f) return {};
    const auto first = static_cast<Index>(std::min<float>(itemCount_, std::floor(scrollOffset_ / rowExtent_)));
    const auto last = static_cast<Index>(std::min<float>(itemCount_, std::ceil((scrollOffset_ + bounds().height) / rowExtent_)));
    return {first, std::max(first, last)};
}

VirtualList::Index VirtualList::mountedCount() const noexcept
{
    return mounted_.size();
}

VirtualList::Index VirtualList::pooledCount() const noexcept
{
    return pool_.size();
}

int VirtualList::selectedIndex() const noexcept
{
    return selectedIndex_;
}

void VirtualList::setSelectedIndex(int index)
{
    const int next = normalizedSelection(index);
    if (selectedIndex_ == next) return;
    selectedIndex_ = next;
    if (next >= 0) scrollToIndex(static_cast<Index>(next));
    markDirty(DirtyFlag::Paint);
}

VirtualList& VirtualList::onSelectionChanged(SelectionHandler handler)
{
    onSelectionChanged_ = std::move(handler);
    return *this;
}

SizeF VirtualList::measure(const Constraints& constraints) const
{
    const float preferredHeight = std::min(static_cast<float>(itemCount_) * rowExtent_, rowExtent_ * kDefaultViewportRows);
    return constraints.clamp({kDefaultWidth, preferredHeight});
}

void VirtualList::layout(const RectF& bounds)
{
    setBounds(bounds);
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maxScrollOffset());
    reconcile();
    layoutMountedChildren();
    clearDirty(DirtyFlag::Layout);
}

void VirtualList::paint(PaintContext& context)
{
    const Theme& current = theme();
    const bool focused = (visualStates() & toMask(ControlVisualState::Focused)) != 0;
    const float focusInset = current.controls.focusInset;
    if (focused) {
        context.fillRoundRect({bounds().x - focusInset, bounds().y - focusInset,
                               bounds().width + focusInset * 2.0f, bounds().height + focusInset * 2.0f},
                              current.radius.md + focusInset, current.colors.focus);
    }
    context.fillRoundRect(bounds(), current.radius.md, current.colors.border);
    const RectF viewport{bounds().x + 1.0f, bounds().y + 1.0f,
                         std::max(0.0f, bounds().width - 2.0f), std::max(0.0f, bounds().height - 2.0f)};
    context.fillRoundRect(viewport, std::max(0.0f, current.radius.md - 1.0f), current.colors.surface);
    const int checkpoint = context.save();
    context.clipRect(viewport);
    ContainerNode::paint(context);
    context.restoreTo(checkpoint);
    clearDirty(DirtyFlag::Paint);
}

Node* VirtualList::hitTest(PointF point)
{
    // Rows are supplied by the application and may be purely visual. Routing
    // to the list makes selection deterministic and avoids stale child input
    // targets while reconciliation replaces only off-screen rows.
    return bounds().contains(point) ? this : nullptr;
}

bool VirtualList::onPointerEvent(const PointerEvent& event)
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
        setScrollOffset(scrollOffset_ - event.scrollDelta.y);
        return true;
    case PointerAction::Move:
    case PointerAction::Enter:
    case PointerAction::Leave:
        return true;
    }
    return false;
}

bool VirtualList::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || event.action != KeyAction::Down || itemCount_ == 0) return false;
    int next = selectedIndex_;
    switch (event.keyCode) {
    case 38: // Up
        next = selectedIndex_ < 0 ? static_cast<int>(itemCount_ - 1) : std::max(0, selectedIndex_ - 1);
        break;
    case 40: // Down
        next = selectedIndex_ < 0 ? 0 : std::min(static_cast<int>(itemCount_ - 1), selectedIndex_ + 1);
        break;
    case 36: // Home
        next = 0;
        break;
    case 35: // End
        next = static_cast<int>(itemCount_ - 1);
        break;
    default:
        return false;
    }
    select(next);
    return true;
}

VirtualList::Range VirtualList::mountedRange() const noexcept
{
    const Range visible = visibleRange();
    if (visible.empty()) return visible;
    const Index first = visible.first > overscanRows_ ? visible.first - overscanRows_ : 0;
    const Index last = std::min(itemCount_, visible.last + overscanRows_);
    return {first, last};
}

VirtualList::Key VirtualList::keyFor(Index index) const
{
    return keyProvider_ ? keyProvider_(index) : std::to_string(index);
}

int VirtualList::rowAt(PointF point) const noexcept
{
    if (!bounds().contains(point) || rowExtent_ <= 0.0f) return -1;
    const float contentY = point.y - bounds().y + scrollOffset_;
    if (contentY < 0.0f) return -1;
    const Index index = static_cast<Index>(contentY / rowExtent_);
    return index < itemCount_ && index <= static_cast<Index>(std::numeric_limits<int>::max()) ? static_cast<int>(index) : -1;
}

int VirtualList::normalizedSelection(int index) const noexcept
{
    return index >= 0 && static_cast<Index>(index) < itemCount_ ? index : -1;
}

void VirtualList::reconcile()
{
    const Range desiredRange = mountedRange();
    std::vector<std::pair<Index, Key>> desired;
    desired.reserve(desiredRange.size());
    std::unordered_set<Key> desiredKeys;
    for (Index index = desiredRange.first; index < desiredRange.last; ++index) {
        Key key = keyFor(index);
        // A duplicate model key would make reuse ambiguous. Keep both rows
        // deterministic by making the second key index-qualified instead of
        // accidentally moving an unrelated mounted node.
        if (!desiredKeys.insert(key).second) key += "#" + std::to_string(index);
        desired.emplace_back(index, std::move(key));
    }

    for (std::size_t index = mounted_.size(); index > 0; --index) {
        if (desiredKeys.find(mounted_[index - 1].key) == desiredKeys.end()) unmount(index - 1);
    }

    for (const auto& [index, key] : desired) {
        const auto existing = std::find_if(mounted_.begin(), mounted_.end(), [&key](const Mounted& mounted) {
            return mounted.key == key;
        });
        if (existing != mounted_.end()) {
            existing->index = index;
            continue;
        }
        std::unique_ptr<Node> node = takePooled(key);
        if (!node && itemBuilder_) node = itemBuilder_(index, key);
        if (!node) continue;
        Node* raw = node.get();
        appendChild(std::move(node));
        mounted_.push_back({index, key, raw});
    }
    trimPool();
}

void VirtualList::layoutMountedChildren()
{
    for (Mounted& mounted : mounted_) {
        mounted.node->layout({bounds().x + 1.0f,
                              bounds().y + static_cast<float>(mounted.index) * rowExtent_ - scrollOffset_,
                              std::max(0.0f, bounds().width - 2.0f), rowExtent_});
    }
}

void VirtualList::unmount(std::size_t mountedIndex)
{
    Mounted mounted = std::move(mounted_[mountedIndex]);
    const auto& nodes = children();
    const auto child = std::find_if(nodes.begin(), nodes.end(), [&mounted](const std::unique_ptr<Node>& node) {
        return node.get() == mounted.node;
    });
    if (child != nodes.end()) {
        const auto position = static_cast<std::size_t>(std::distance(nodes.begin(), child));
        addToPool(std::move(mounted.key), removeChild(position));
    }
    mounted_.erase(mounted_.begin() + static_cast<std::ptrdiff_t>(mountedIndex));
}

std::unique_ptr<Node> VirtualList::takePooled(const Key& key)
{
    const auto item = std::find_if(pool_.begin(), pool_.end(), [&key](const Pooled& pooled) { return pooled.key == key; });
    if (item == pool_.end()) return nullptr;
    std::unique_ptr<Node> node = std::move(item->node);
    pool_.erase(item);
    return node;
}

void VirtualList::addToPool(Key key, std::unique_ptr<Node> node)
{
    if (node) pool_.push_back({std::move(key), std::move(node)});
}

void VirtualList::trimPool()
{
    const Index viewportRows = visibleRange().size();
    const Index limit = std::max<Index>(8, (viewportRows + overscanRows_ * 2) * 2);
    if (pool_.size() > limit) pool_.erase(pool_.begin(), pool_.begin() + static_cast<std::ptrdiff_t>(pool_.size() - limit));
}

void VirtualList::select(int index)
{
    if (index < 0 || static_cast<Index>(index) >= itemCount_ || index == selectedIndex_) return;
    selectedIndex_ = index;
    scrollToIndex(static_cast<Index>(index));
    if (onSelectionChanged_) onSelectionChanged_(static_cast<Index>(index));
    markDirty(DirtyFlag::Paint);
}

} // namespace wui
