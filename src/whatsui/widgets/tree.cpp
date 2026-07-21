#include "wui/tree.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "wui/icons.h"
#include "wui/text_metrics.h"
#include "wui/theme.h"

namespace wui {
namespace {
constexpr int kEnter = 13, kSpace = 32, kHome = 36, kEnd = 35;
constexpr int kLeft = 37, kUp = 38, kRight = 39, kDown = 40;
constexpr float kIndent = 24.0f;
constexpr float kDisclosureSlot = 24.0f;

bool state(const ControlNode& node, ControlVisualState value) noexcept
{ return (node.visualStates() & toMask(value)) != 0; }

float textWidth(const std::string& value, const TextStyleToken& style) noexcept
{
    if (const auto* measurer = textMeasurer()) return measurer->measureText(value, style.size, style.weight).width;
    return static_cast<float>(value.size()) * style.size * 0.55f;
}
} // namespace

TreeItem::TreeItem(std::string id, std::string label) : id_(std::move(id)), label_(std::move(label)) {}
const std::string& TreeItem::id() const noexcept { return id_; }
TreeItem& TreeItem::id(std::string value) { setId(std::move(value)); return *this; }
void TreeItem::setId(std::string value) { if (id_ != value) { id_ = std::move(value); markDirty(DirtyFlag::Paint); } }
const std::string& TreeItem::label() const noexcept { return label_; }
TreeItem& TreeItem::label(std::string value) { setLabel(std::move(value)); return *this; }
void TreeItem::setLabel(std::string value) { if (label_ != value) { label_ = std::move(value); markDirty(DirtyFlag::Layout); } }
TreeItem& TreeItem::addItem(std::string id, std::string label)
{
    auto child = std::make_unique<TreeItem>(std::move(id), std::move(label));
    auto* raw = child.get(); appendChild(std::move(child)); markDirty(DirtyFlag::Layout); return *raw;
}
bool TreeItem::hasChildren() const noexcept { return !children().empty(); }
bool TreeItem::isExpanded() const noexcept { return expanded_; }
TreeItem& TreeItem::expanded(bool value) { setExpanded(value); return *this; }
void TreeItem::setExpanded(bool value) { if (auto* tree = ownerTree()) (void)tree->setExpanded(*this, value); else if (expanded_ != value) { expanded_ = value; markDirty(DirtyFlag::Layout); } }
bool TreeItem::isSelected() const noexcept { return selected_; }
std::size_t TreeItem::level() const noexcept { return depth() + 1; }
void TreeItem::setSelectedFromOwner(bool value) noexcept { if (selected_ != value) { selected_ = value; markDirty(DirtyFlag::Paint); } }
Tree* TreeItem::ownerTree() const noexcept
{
    for (Node* current = parent(); current; current = current->parent()) if (auto* tree = dynamic_cast<Tree*>(current)) return tree;
    return nullptr;
}
std::size_t TreeItem::depth() const noexcept
{
    std::size_t result = 0;
    for (Node* current = parent(); current && dynamic_cast<TreeItem*>(current); current = current->parent()) ++result;
    return result;
}
RectF TreeItem::disclosureBounds() const noexcept
{
    const float x = bounds().x + static_cast<float>(depth()) * kIndent;
    return {x, bounds().y + (bounds().height - kDisclosureSlot) * .5f,
            kDisclosureSlot, kDisclosureSlot};
}
SizeF TreeItem::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    const float natural = static_cast<float>(depth()) * kIndent + kDisclosureSlot + current.spacing.horizontal.xs +
        textWidth(label_, current.typography.body1) + current.spacing.horizontal.m;
    return constraints.clamp({std::max(80.0f, natural), 32.0f});
}
void TreeItem::paint(PaintContext& context)
{
    const auto& current = theme();
    const RectF rect = context.snapRectEdges(bounds());
    if (rect.width <= 0 || rect.height <= 0) return;
    const bool disabled = !isEnabled();
    Color background{0, 0, 0, 0};
    if (selected_) background = current.colors.neutralBackground1.selected;
    else if (!disabled && state(*this, ControlVisualState::Pressed)) background = current.colors.neutralBackground1.pressed;
    else if (!disabled && state(*this, ControlVisualState::Hovered)) background = current.colors.neutralBackground1.hover;
    if (background.a)
        context.fillRoundRect(rect, current.radius.medium, background);
    if (state(*this, ControlVisualState::Focused))
        context.strokeRoundRect(
            {rect.x + 1.0f, rect.y + 1.0f,
             std::max(0.0f, rect.width - 2.0f),
             std::max(0.0f, rect.height - 2.0f)},
            current.radius.medium,
            context.snapStrokeWidth(current.stroke.thick),
            current.colors.strokeFocusInner);
    const RectF glyph = disclosureBounds();
    if (hasChildren()) {
        const Color glyphColor = disabled ? current.colors.neutralForegroundDisabled : current.colors.neutralForeground2;
        drawIcon(context,
                 expanded_ ? IconName::ChevronDown
                           : IconName::ChevronRight,
                 glyph, glyphColor, IconSize::Size16);
    }
    // Fluent TreeItemLayout uses a 24-DIP expand-icon slot and 2-DIP main
    // content inset. Leaf rows retain the same slot so labels remain aligned
    // with sibling branches.
    const float x = glyph.x + glyph.width + current.spacing.horizontal.xs;
    const auto& style = current.typography.body1;
    context.drawText(label_, x, context.centeredTextBottom(label_, rect, style.size, style.weight), style.size,
        disabled ? current.colors.neutralForegroundDisabled : current.colors.neutralForeground1, style.weight, style.family);
    clearDirty(DirtyFlag::Paint);
}
Node* TreeItem::hitTest(PointF point) { return bounds().contains(point) ? this : nullptr; }
bool TreeItem::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled() || !bounds().contains(event.position)) return false;
    switch (event.action) {
    case PointerAction::Enter: setVisualState(ControlVisualState::Hovered, true); return true;
    case PointerAction::Leave: setVisualState(ControlVisualState::Hovered, false); setVisualState(ControlVisualState::Pressed, false); return true;
    case PointerAction::Down: if (event.button == MouseButton::Left) { setVisualState(ControlVisualState::Pressed, true); return true; } break;
    case PointerAction::Up: if (event.button == MouseButton::Left) {
        const bool activate = state(*this, ControlVisualState::Pressed); setVisualState(ControlVisualState::Pressed, false);
        if (activate) { if (hasChildren() && disclosureBounds().contains(event.position)) setExpanded(!expanded_); else if (auto* tree = ownerTree()) tree->selectItem(*this); }
        return true;
    } break;
    case PointerAction::Cancel: setVisualState(ControlVisualState::Pressed, false); return true;
    default: break;
    }
    return false;
}
bool TreeItem::onKeyEvent(const KeyEvent& event) { return ownerTree() ? ownerTree()->onKeyEvent(event) : false; }
AccessibilityActionCapabilities TreeItem::accessibilityActions() const noexcept
{ AccessibilityActionCapabilities actions; actions.invoke = true; actions.focus = true; actions.expandCollapse = hasChildren(); return actions; }
AccessibilityActionStatus TreeItem::performAccessibilityAction(AccessibilityActionKind kind, std::string_view)
{
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    if (kind == AccessibilityActionKind::Invoke || kind == AccessibilityActionKind::SetFocus) { if (auto* tree = ownerTree()) tree->selectItem(*this); return AccessibilityActionStatus::Succeeded; }
    if (kind == AccessibilityActionKind::Expand && hasChildren()) { setExpanded(true); return AccessibilityActionStatus::Succeeded; }
    if (kind == AccessibilityActionKind::Collapse && hasChildren()) { setExpanded(false); return AccessibilityActionStatus::Succeeded; }
    return AccessibilityActionStatus::NotSupported;
}

TreeItem& Tree::addItem(std::string id, std::string label)
{
    auto item = std::make_unique<TreeItem>(std::move(id), std::move(label)); auto* raw = item.get(); appendChild(std::move(item));
    // A newly constructed Tree is not automatically keyboard-focused. The
    // host focus manager (or the first Tree key command) establishes roving
    // focus; otherwise passive tree presentations would show a stray ring.
    return *raw;
}
Tree& Tree::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void Tree::setAccessibleLabel(std::string value) { accessibleLabel_ = std::move(value); markDirty(DirtyFlag::Paint); }
const std::string& Tree::accessibleLabel() const noexcept { return accessibleLabel_; }
Tree& Tree::rowHeight(float value) noexcept { setRowHeight(value); return *this; }
void Tree::setRowHeight(float value) noexcept { const float next = std::max(24.0f, value); if (rowHeight_ != next) { rowHeight_ = next; markDirty(DirtyFlag::Layout); } }
float Tree::rowHeight() const noexcept { return rowHeight_; }
Tree& Tree::maxVisibleItems(std::size_t value) noexcept { setMaxVisibleItems(value); return *this; }
void Tree::setMaxVisibleItems(std::size_t value) noexcept { maxVisibleItems_ = std::max<std::size_t>(1, value); markDirty(DirtyFlag::Layout); }
std::size_t Tree::maxVisibleItems() const noexcept { return maxVisibleItems_; }
float Tree::scrollOffset() const noexcept { return scrollOffset_; }
void Tree::setScrollOffset(float value) noexcept { const float next = std::clamp(std::isfinite(value) ? value : 0.0f, 0.0f, maximumScrollOffset()); if (scrollOffset_ != next) { scrollOffset_ = next; markDirty(DirtyFlag::Paint); } }
float Tree::maximumScrollOffset() const noexcept { const float viewport = std::max(0.0f, bounds().height); return std::max(0.0f, static_cast<float>(visibleItems().size()) * rowHeight_ - viewport); }
TreeItem* Tree::selectedItem() const noexcept { return findItem(selectedId_); }
const std::string& Tree::selectedId() const noexcept { return selectedId_; }
bool Tree::select(std::string_view id) { if (auto* item = findItem(id)) return selectItem(*item, false); return false; }
Tree& Tree::onSelectionChanged(SelectionHandler handler) { onSelectionChanged_ = std::move(handler); return *this; }
Tree& Tree::onExpandedChange(ExpandHandler handler) { onExpandedChange_ = std::move(handler); return *this; }
void Tree::appendVisible(TreeItem& item, std::vector<TreeItem*>& items) const
{
    items.push_back(&item);
    if (item.isExpanded()) for (const auto& child : item.children()) if (auto* treeItem = dynamic_cast<TreeItem*>(child.get())) appendVisible(*treeItem, items);
}
std::vector<TreeItem*> Tree::visibleItems() const
{
    std::vector<TreeItem*> result;
    for (const auto& child : children()) if (auto* item = dynamic_cast<TreeItem*>(child.get())) appendVisible(*item, result);
    return result;
}
SizeF Tree::measure(const Constraints& constraints) const
{
    float width = 0; for (TreeItem* item : visibleItems()) width = std::max(width, item->measureWithConstraints(constraints).width);
    const float height = std::min(static_cast<float>(maxVisibleItems_) * rowHeight_, static_cast<float>(visibleItems().size()) * rowHeight_);
    return constraints.clamp({width, height});
}
void Tree::layout(const RectF& rect)
{
    Node::layout(rect); const auto items = visibleItems();
    std::function<void(TreeItem&)> clearRows = [&](TreeItem& item) {
        item.layout({0, 0, 0, 0});
        for (const auto& child : item.children()) if (auto* nested = dynamic_cast<TreeItem*>(child.get())) clearRows(*nested);
    };
    for (const auto& root : children()) if (auto* item = dynamic_cast<TreeItem*>(root.get())) clearRows(*item);
    for (std::size_t i = 0; i < items.size(); ++i) items[i]->layout({rect.x, rect.y + static_cast<float>(i) * rowHeight_ - scrollOffset_, rect.width, rowHeight_});
    setScrollOffset(scrollOffset_); clearLayoutDirtyRecursively();
}
void Tree::paint(PaintContext& context)
{
    // Rows are painted only when they intersect the viewport. Individual rows
    // never paint outside their own bounds, so this is a safe windowing path
    // even on backends that do not expose a save/restore clip stack.
    const RectF viewport = bounds();
    for (TreeItem* item : visibleItems()) {
        const RectF row = item->bounds();
        if (row.y + row.height > viewport.y && row.y < viewport.y + viewport.height) item->paint(context);
    }
    clearDirty(DirtyFlag::Paint);
}
Node* Tree::hitTest(PointF point)
{
    if (!bounds().contains(point)) return nullptr;
    for (TreeItem* item : visibleItems()) if (item->bounds().contains(point)) return item;
    return this;
}
bool Tree::onPointerEvent(const PointerEvent& event)
{
    if (event.action == PointerAction::Scroll && bounds().contains(event.position)) { setScrollOffset(scrollOffset_ - event.scrollDelta.y); return true; }
    if (auto* hit = dynamic_cast<TreeItem*>(hitTest(event.position))) return hit->onPointerEvent(event);
    return false;
}
TreeItem* Tree::findItem(std::string_view id) const noexcept
{
    std::function<TreeItem*(const Node&)> find = [&](const Node& node) -> TreeItem* {
        for (const auto& child : node.children()) if (auto* item = dynamic_cast<TreeItem*>(child.get())) { if (item->id() == id) return item; if (auto* nested = find(*item)) return nested; }
        return nullptr;
    }; return find(*this);
}
TreeItem* Tree::nextEnabled(TreeItem* from, int delta) const noexcept
{
    const auto items = visibleItems(); if (items.empty()) return nullptr;
    auto position = std::find(items.begin(), items.end(), from); std::size_t start = position == items.end() ? 0 : static_cast<std::size_t>(position - items.begin());
    for (std::size_t step = 1; step <= items.size(); ++step) { const auto i = (start + items.size() + (delta < 0 ? items.size() - step % items.size() : step % items.size())) % items.size(); if (items[i]->isEnabled()) return items[i]; }
    return from;
}
void Tree::focus(TreeItem* item) noexcept
{
    for (TreeItem* current : visibleItems()) current->setVisualState(ControlVisualState::Focused, current == item);
    focused_ = item;
}
bool Tree::setExpanded(TreeItem& item, bool value)
{
    if (!item.isEnabled() || !item.hasChildren() || item.expanded_ == value) return false;
    item.expanded_ = value; item.markDirty(DirtyFlag::Layout); if (onExpandedChange_) onExpandedChange_(item, value); markDirty(DirtyFlag::Layout); return true;
}
bool Tree::selectItem(TreeItem& item, bool requestFocus)
{
    if (!item.isEnabled()) return false;
    if (selectedId_ == item.id()) { if (requestFocus) focus(&item); return true; }
    if (auto* prior = selectedItem()) prior->setSelectedFromOwner(false);
    selectedId_ = item.id(); item.setSelectedFromOwner(true); if (requestFocus) focus(&item); scrollIntoView(item); if (onSelectionChanged_) onSelectionChanged_(item); return true;
}
void Tree::scrollIntoView(TreeItem& item) noexcept
{
    const auto items = visibleItems(); const auto position = std::find(items.begin(), items.end(), &item); if (position == items.end()) return;
    const float top = static_cast<float>(position - items.begin()) * rowHeight_, bottom = top + rowHeight_, viewport = bounds().height;
    if (top < scrollOffset_) setScrollOffset(top); else if (bottom > scrollOffset_ + viewport) setScrollOffset(bottom - viewport);
}
bool Tree::onKeyEvent(const KeyEvent& event)
{
    if (event.action != KeyAction::Down) return false;
    if (!focused_) focus(nextEnabled(nullptr, 1)); if (!focused_) return false;
    if (event.keyCode == kUp || event.keyCode == kDown) { if (auto* item = nextEnabled(focused_, event.keyCode == kUp ? -1 : 1)) { focus(item); scrollIntoView(*item); return true; } }
    if (event.keyCode == kHome || event.keyCode == kEnd) {
        const auto items = visibleItems();
        if (event.keyCode == kHome) {
            for (TreeItem* item : items) if (item->isEnabled()) { focus(item); scrollIntoView(*item); return true; }
        } else {
            for (auto it = items.rbegin(); it != items.rend(); ++it) if ((*it)->isEnabled()) { focus(*it); scrollIntoView(**it); return true; }
        }
    }
    if (event.keyCode == kRight) { if (focused_->hasChildren() && !focused_->isExpanded()) return setExpanded(*focused_, true); if (focused_->hasChildren()) for (const auto& child : focused_->children()) if (auto* item = dynamic_cast<TreeItem*>(child.get()); item && item->isEnabled()) { focus(item); return true; } }
    if (event.keyCode == kLeft) { if (focused_->hasChildren() && focused_->isExpanded()) return setExpanded(*focused_, false); if (auto* parent = dynamic_cast<TreeItem*>(focused_->parent())) { focus(parent); return true; } }
    if (event.keyCode == kEnter || event.keyCode == kSpace) return selectItem(*focused_);
    return false;
}

} // namespace wui
