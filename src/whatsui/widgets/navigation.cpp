#include "wui/navigation.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "wui/text_metrics.h"
#include "wui/theme.h"

namespace wui {
namespace {

constexpr int kEnter = 13;
constexpr int kSpace = 32;
constexpr int kHome = 36;
constexpr int kEnd = 35;
constexpr int kLeft = 37;
constexpr int kRight = 39;

float textWidth(const std::string& value, const TextStyleToken& style) noexcept
{
    if (const auto* measurer = textMeasurer()) {
        return measurer->measureText(value, style.size, style.weight).width;
    }
    return static_cast<float>(value.size()) * style.size * 0.56f;
}

bool hasState(const ControlNode& node, ControlVisualState state) noexcept
{
    return (node.visualStates() & toMask(state)) != 0;
}

Color interactiveFill(const Theme& current, const ControlNode& node,
                      const ColorTokens::Interaction& ramp) noexcept
{
    if (!node.isEnabled()) return current.colors.neutralBackground1.rest;
    if (hasState(node, ControlVisualState::Pressed)) return ramp.pressed;
    if (hasState(node, ControlVisualState::Hovered)) return ramp.hover;
    return ramp.rest;
}

void focusRing(PaintContext& context, const RectF& rect, float radius,
               const Theme& current, bool focused)
{
    if (!focused) return;
    const float inset = current.controls.focusInset;
    context.strokeRoundRect({rect.x - inset, rect.y - inset, rect.width + 2 * inset,
                             rect.height + 2 * inset}, radius + inset,
                             current.controls.focusWidth, current.colors.strokeFocusInner);
}

} // namespace

ToolbarItem::ToolbarItem(std::string label) : label_(std::move(label)) {}
const std::string& ToolbarItem::label() const noexcept { return label_; }
ToolbarItem& ToolbarItem::label(std::string value) { setLabel(std::move(value)); return *this; }
void ToolbarItem::setLabel(std::string value) { if (label_ != value) { label_ = std::move(value); markDirty(DirtyFlag::Layout); } }
ToolbarItemAppearance ToolbarItem::appearance() const noexcept { return appearance_; }
ToolbarItem& ToolbarItem::appearance(ToolbarItemAppearance value) noexcept { setAppearance(value); return *this; }
void ToolbarItem::setAppearance(ToolbarItemAppearance value) noexcept { if (appearance_ != value) { appearance_ = value; markDirty(DirtyFlag::Paint); } }
ToolbarItem& ToolbarItem::onInvoke(InvokeHandler handler) { onInvoke_ = std::move(handler); return *this; }
SizeF ToolbarItem::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    return constraints.clamp({textWidth(label_, current.typography.body1) + current.spacing.horizontal.m * 2,
                              current.controls.height});
}
void ToolbarItem::paint(PaintContext& context)
{
    const auto& current = theme(); const RectF rect = bounds();
    if (rect.width <= 0.0f || rect.height <= 0.0f) { clearDirty(DirtyFlag::Paint); return; }
    const bool primary = appearance_ == ToolbarItemAppearance::Primary;
    const Color fill = primary ? interactiveFill(current, *this, current.colors.brandBackground)
                               : interactiveFill(current, *this, current.colors.neutralBackground1);
    if (primary || hasState(*this, ControlVisualState::Hovered) || hasState(*this, ControlVisualState::Pressed))
        context.fillRoundRect(rect, current.radius.medium, fill);
    focusRing(context, rect, current.radius.medium, current, hasState(*this, ControlVisualState::Focused));
    const auto& style = current.typography.body1;
    const Color foreground = !isEnabled() ? current.colors.neutralForegroundDisabled
                            : primary ? current.colors.onBrand : current.colors.neutralForeground1;
    context.drawText(label_, rect.x + (rect.width - textWidth(label_, style)) * .5f,
                     context.centeredTextBottom(label_, rect, style.size, style.weight), style.size,
                     foreground, style.weight, style.family);
    clearDirty(DirtyFlag::Paint);
}
bool ToolbarItem::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    switch (event.action) {
    case PointerAction::Enter: setVisualState(ControlVisualState::Hovered, true); return true;
    case PointerAction::Leave: setVisualState(ControlVisualState::Hovered, false); setVisualState(ControlVisualState::Pressed, false); return true;
    case PointerAction::Down: if (event.button == MouseButton::Left) { setVisualState(ControlVisualState::Pressed, true); setVisualState(ControlVisualState::Focused, true); return true; } break;
    case PointerAction::Up: if (event.button == MouseButton::Left) { const bool invokeNow = hasState(*this, ControlVisualState::Pressed) && bounds().contains(event.position); setVisualState(ControlVisualState::Pressed, false); if (invokeNow) invoke(); return true; } break;
    case PointerAction::Cancel: setVisualState(ControlVisualState::Pressed, false); return true;
    default: break;
    }
    return false;
}
bool ToolbarItem::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || event.action != KeyAction::Down) return false;
    if (event.keyCode == kEnter || event.keyCode == kSpace) { invoke(); return true; }
    if ((event.keyCode == kLeft || event.keyCode == kRight || event.keyCode == kHome || event.keyCode == kEnd) && parent()) return parent()->onKeyEvent(event);
    return false;
}
AccessibilityActionCapabilities ToolbarItem::accessibilityActions() const noexcept { AccessibilityActionCapabilities result; result.invoke = true; return result; }
AccessibilityActionStatus ToolbarItem::performAccessibilityAction(AccessibilityActionKind kind, std::string_view) { if (kind != AccessibilityActionKind::Invoke) return AccessibilityActionStatus::NotSupported; if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled; invoke(); return AccessibilityActionStatus::Succeeded; }
void ToolbarItem::invoke() { if (isEnabled() && onInvoke_) onInvoke_(); }

ToolbarItem& Toolbar::addItem(std::string label, ToolbarItemAppearance appearance)
{
    auto item = std::make_unique<ToolbarItem>(std::move(label)); item->setAppearance(appearance); auto* raw = item.get(); appendChild(std::move(item)); return *raw;
}
Toolbar& Toolbar::orientation(ToolbarOrientation value) noexcept { setOrientation(value); return *this; }
void Toolbar::setOrientation(ToolbarOrientation value) noexcept { if (orientation_ != value) { orientation_ = value; markDirty(DirtyFlag::Layout); } }
ToolbarOrientation Toolbar::orientation() const noexcept { return orientation_; }
const std::vector<std::string>& Toolbar::overflowedItems() const noexcept { return overflowedItems_; }
Toolbar& Toolbar::onOverflow(OverflowHandler handler) { onOverflow_ = std::move(handler); return *this; }
Toolbar& Toolbar::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void Toolbar::setAccessibleLabel(std::string value) { accessibleLabel_ = std::move(value); markDirty(DirtyFlag::Style); }
const std::string& Toolbar::accessibleLabel() const noexcept { return accessibleLabel_; }
std::size_t Toolbar::focusedIndex() const noexcept { return focusedIndex_; }
SizeF Toolbar::measure(const Constraints& constraints) const
{
    float width = 0, height = 0; const float gap = theme().spacing.horizontal.xs;
    for (const auto& child : children()) {
        const auto size = child->measureWithConstraints(constraints);
        if (orientation_ == ToolbarOrientation::Horizontal) { width += size.width; height = std::max(height, size.height); }
        else { width = std::max(width, size.width); height += size.height; }
    }
    if (children().size() > 1) {
        if (orientation_ == ToolbarOrientation::Horizontal) width += gap * float(children().size() - 1);
        else height += gap * float(children().size() - 1);
    }
    return constraints.clamp({width, height});
}
void Toolbar::layout(const RectF& rect)
{
    Node::layout(rect);
    const float gap = theme().spacing.horizontal.xs;
    const bool horizontal = orientation_ == ToolbarOrientation::Horizontal;
    const float limit = horizontal ? rect.width : rect.height;
    const float overflowExtent = theme().controls.height;
    std::vector<SizeF> sizes; sizes.reserve(children().size());
    float natural = 0.0f;
    for (const auto& child : children()) {
        const SizeF size = child->measureWithConstraints({0, rect.width, 0, rect.height});
        sizes.push_back(size); natural += horizontal ? size.width : size.height;
    }
    if (children().size() > 1) natural += gap * static_cast<float>(children().size() - 1);
    const bool needsOverflow = natural > limit + 0.01f;
    const float itemLimit = needsOverflow ? std::max(0.0f, limit - overflowExtent - (children().empty() ? 0.0f : gap)) : limit;
    overflowedItems_.clear(); overflowBounds_ = {};
    float cursor = horizontal ? rect.x : rect.y;
    float used = 0.0f;
    bool hidden = false;
    for (std::size_t i = 0; i < children().size(); ++i) {
        const float extent = horizontal ? sizes[i].width : sizes[i].height;
        const float next = used + (i == 0 ? 0.0f : gap) + extent;
        if (needsOverflow && (hidden || next > itemLimit + 0.01f)) {
            hidden = true;
            children()[i]->layout({0, 0, 0, 0});
            if (const auto* item = dynamic_cast<const ToolbarItem*>(children()[i].get())) overflowedItems_.push_back(item->label());
            continue;
        }
        if (i != 0) cursor += gap;
        if (horizontal) children()[i]->layout({cursor, rect.y + (rect.height - sizes[i].height) * .5f, sizes[i].width, sizes[i].height});
        else children()[i]->layout({rect.x + (rect.width - sizes[i].width) * .5f, cursor, sizes[i].width, sizes[i].height});
        cursor += extent; used = next;
    }
    if (!overflowedItems_.empty()) {
        const float edge = horizontal ? rect.x + rect.width - overflowExtent : rect.y + rect.height - overflowExtent;
        overflowBounds_ = horizontal ? RectF{edge, rect.y + (rect.height - overflowExtent) * .5f, overflowExtent, overflowExtent}
                                     : RectF{rect.x + (rect.width - overflowExtent) * .5f, edge, overflowExtent, overflowExtent};
    }
    clearLayoutDirtyRecursively();
}
RectF Toolbar::overflowBounds() const noexcept { return overflowBounds_; }
void Toolbar::paint(PaintContext& context)
{
    ContainerNode::paint(context);
    if (!overflowedItems_.empty()) {
        const auto& current = theme(); const auto& style = current.typography.body1Strong;
        context.fillRoundRect(overflowBounds_, current.radius.medium, current.colors.neutralBackground1.rest);
        context.drawText("…", overflowBounds_.x + (overflowBounds_.width - textWidth("…", style)) * .5f,
                         context.centeredTextBottom("…", overflowBounds_, style.size, style.weight), style.size,
                         current.colors.neutralForeground1, style.weight, style.family);
    }
    clearDirty(DirtyFlag::Paint);
}
bool Toolbar::onPointerEvent(const PointerEvent& event)
{
    if (!overflowedItems_.empty() && event.action == PointerAction::Up && event.button == MouseButton::Left && overflowBounds_.contains(event.position)) {
        if (onOverflow_) onOverflow_(overflowedItems_);
        return true;
    }
    return false;
}
bool Toolbar::moveFocus(int delta)
{
    if (children().empty()) return false;
    std::size_t next = focusedIndex_;
    for (std::size_t attempts = 0; attempts < children().size(); ++attempts) {
        next = static_cast<std::size_t>((static_cast<int>(next) + delta + static_cast<int>(children().size())) % static_cast<int>(children().size()));
        auto* item = dynamic_cast<ToolbarItem*>(children()[next].get()); if (item && item->isEnabled() && item->bounds().width > 0.0f && item->bounds().height > 0.0f) { focusedIndex_ = next; for (std::size_t i = 0; i < children().size(); ++i) if (auto* candidate = dynamic_cast<ToolbarItem*>(children()[i].get())) candidate->setVisualState(ControlVisualState::Focused, i == next); return true; }
    }
    return false;
}
bool Toolbar::onKeyEvent(const KeyEvent& event)
{
    if (event.action != KeyAction::Down) return false;
    if (event.keyCode == kHome) { focusedIndex_ = 0; return moveFocus(0) || moveFocus(1); }
    if (event.keyCode == kEnd) { focusedIndex_ = children().empty() ? 0 : children().size() - 1; return moveFocus(0) || moveFocus(-1); }
    if (event.keyCode == (orientation_ == ToolbarOrientation::Horizontal ? kLeft : 38)) return moveFocus(-1);
    if (event.keyCode == (orientation_ == ToolbarOrientation::Horizontal ? kRight : 40)) return moveFocus(1);
    return false;
}

Tab::Tab(std::string value, std::string label) : value_(std::move(value)), label_(std::move(label)) { if (value_.empty()) value_ = label_; }
const std::string& Tab::value() const noexcept { return value_; }
Tab& Tab::value(std::string value) { setValue(std::move(value)); return *this; }
void Tab::setValue(std::string value) { if (value_ != value) { value_ = std::move(value); markDirty(DirtyFlag::Layout); } }
const std::string& Tab::label() const noexcept { return label_; }
Tab& Tab::label(std::string value) { setLabel(std::move(value)); return *this; }
void Tab::setLabel(std::string value) { if (label_ != value) { label_ = std::move(value); markDirty(DirtyFlag::Layout); } }
bool Tab::isSelected() const noexcept { return selected_; }
SizeF Tab::measure(const Constraints& constraints) const { const auto& current = theme(); return constraints.clamp({textWidth(label_, current.typography.body1Strong) + current.spacing.horizontal.l * 2, current.controls.height + 8}); }
void Tab::paint(PaintContext& context)
{
    const auto& current = theme(); const RectF rect = bounds(); const bool selected = selected_;
    if (hasState(*this, ControlVisualState::Hovered) && !selected) context.fillRoundRect({rect.x + 2, rect.y + 2, std::max(0.f, rect.width - 4), std::max(0.f, rect.height - 6)}, current.radius.medium, current.colors.neutralBackground1.hover);
    if (hasState(*this, ControlVisualState::Pressed) && !selected) context.fillRoundRect({rect.x + 2, rect.y + 2, std::max(0.f, rect.width - 4), std::max(0.f, rect.height - 6)}, current.radius.medium, current.colors.neutralBackground1.pressed);
    focusRing(context, {rect.x + 2, rect.y + 2, std::max(0.f, rect.width - 4), std::max(0.f, rect.height - 6)}, current.radius.medium, current, hasState(*this, ControlVisualState::Focused));
    const auto& style = selected ? current.typography.body1Strong : current.typography.body1;
    const Color foreground = !isEnabled() ? current.colors.neutralForegroundDisabled : selected ? current.colors.brandForeground1 : current.colors.neutralForeground1;
    context.drawText(label_, rect.x + (rect.width - textWidth(label_, style)) * .5f, context.centeredTextBottom(label_, {rect.x,rect.y,rect.width,rect.height-4}, style.size, style.weight), style.size, foreground, style.weight, style.family);
    if (selected) context.fillRoundRect({rect.x + 4, rect.y + rect.height - 3, std::max(0.f, rect.width - 8), 3}, current.radius.circular, current.colors.brandForeground1);
    clearDirty(DirtyFlag::Paint);
}
bool Tab::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    if (event.action == PointerAction::Enter) { setVisualState(ControlVisualState::Hovered,true); return true; }
    if (event.action == PointerAction::Leave) { setVisualState(ControlVisualState::Hovered,false); setVisualState(ControlVisualState::Pressed,false); return true; }
    if (event.action == PointerAction::Down && event.button == MouseButton::Left) { setVisualState(ControlVisualState::Pressed,true); setVisualState(ControlVisualState::Focused,true); return true; }
    if (event.action == PointerAction::Up && event.button == MouseButton::Left) { const bool activate = hasState(*this, ControlVisualState::Pressed) && bounds().contains(event.position); setVisualState(ControlVisualState::Pressed,false); if (activate) select(); return true; }
    if (event.action == PointerAction::Cancel) { setVisualState(ControlVisualState::Pressed,false); return true; }
    return false;
}
bool Tab::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || event.action != KeyAction::Down) return false;
    if (event.keyCode == kEnter || event.keyCode == kSpace) { select(); return true; }
    if (parent()) return parent()->onKeyEvent(event);
    return false;
}
AccessibilityActionCapabilities Tab::accessibilityActions() const noexcept { AccessibilityActionCapabilities result; result.invoke = true; result.toggle = true; return result; }
AccessibilityActionStatus Tab::performAccessibilityAction(AccessibilityActionKind kind, std::string_view) { if (kind != AccessibilityActionKind::Invoke && kind != AccessibilityActionKind::Toggle) return AccessibilityActionStatus::NotSupported; if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled; select(); return AccessibilityActionStatus::Succeeded; }
void Tab::setSelectedFromList(bool value) noexcept { if (selected_ != value) { selected_ = value; markDirty(DirtyFlag::Paint); } }
void Tab::select() { if (auto* list = dynamic_cast<TabList*>(parent())) list->selectTab(*this); }

Tab& TabList::addTab(std::string value, std::string label, bool enabled)
{
    auto tab = std::make_unique<Tab>(std::move(value), std::move(label));
    tab->setEnabled(enabled);
    auto* raw = tab.get(); appendChild(std::move(tab));
    if (value_.empty() && enabled) selectTab(*raw, false);
    return *raw;
}
const std::string& TabList::value() const noexcept { return value_; }
TabList& TabList::value(std::string value) { setValue(std::move(value)); return *this; }
void TabList::setValue(std::string value)
{
    // A TabList never exposes a selected value that has no matching Tab.
    // This is important to UIA clients, which read value and selected Tab from
    // the same snapshot and expect them to agree.
    for (const auto& child : children()) {
        if (auto* tab = dynamic_cast<Tab*>(child.get()); tab && tab->value() == value) {
            selectTab(*tab);
            return;
        }
    }
}
TabList& TabList::onChange(ChangeHandler handler) { onChange_ = std::move(handler); return *this; }
TabList::ActivationMode TabList::activationMode() const noexcept { return activationMode_; }
TabList& TabList::activationMode(ActivationMode value) noexcept { setActivationMode(value); return *this; }
void TabList::setActivationMode(ActivationMode value) noexcept { activationMode_ = value; }
std::size_t TabList::focusedIndex() const noexcept { return focusedIndex_; }
TabList& TabList::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void TabList::setAccessibleLabel(std::string value) { accessibleLabel_ = std::move(value); markDirty(DirtyFlag::Style); }
const std::string& TabList::accessibleLabel() const noexcept { return accessibleLabel_; }
SizeF TabList::measure(const Constraints& constraints) const { float width = 0, height = 0; for (const auto& child : children()) { const auto size = child->measureWithConstraints(constraints); width += size.width; height = std::max(height,size.height); } return constraints.clamp({width,height}); }
void TabList::layout(const RectF& rect) { Node::layout(rect); float cursor = rect.x; for (const auto& child : children()) { const auto size=child->measureWithConstraints({0,rect.width,0,rect.height}); child->layout({cursor,rect.y,size.width,rect.height}); cursor += size.width; } clearLayoutDirtyRecursively(); }
void TabList::paint(PaintContext& context) { const auto& current = theme(); context.fillRect({bounds().x,bounds().y+bounds().height-1,bounds().width,1},current.colors.neutralStroke1); ContainerNode::paint(context); clearDirty(DirtyFlag::Paint); }
void TabList::selectTab(Tab& tab, bool notify)
{
    if (!tab.isEnabled()) return;
    const bool changed = value_ != tab.value();
    value_ = tab.value();
    for (const auto& child : children()) {
        if (auto* candidate = dynamic_cast<Tab*>(child.get())) candidate->setSelectedFromList(candidate == &tab);
    }
    markDirty(DirtyFlag::Paint);
    if (changed && notify && onChange_) onChange_(value_);
}
bool TabList::moveSelection(int delta)
{
    if (children().empty()) return false;
    std::size_t start = 0;
    for (std::size_t i = 0; i < children().size(); ++i)
        if (auto* tab = dynamic_cast<Tab*>(children()[i].get()); tab && tab->isSelected()) { start = i; break; }
    for (std::size_t i = 0; i < children().size(); ++i) {
        start = static_cast<std::size_t>((static_cast<int>(start) + delta + static_cast<int>(children().size())) % static_cast<int>(children().size()));
        if (auto* tab = dynamic_cast<Tab*>(children()[start].get()); tab && tab->isEnabled()) {
            selectTab(*tab);
            focusedIndex_ = start;
            for (const auto& child : children()) if (auto* candidate = dynamic_cast<Tab*>(child.get())) candidate->setVisualState(ControlVisualState::Focused, candidate == tab);
            return true;
        }
    }
    return false;
}
bool TabList::moveFocus(int delta)
{
    if (children().empty()) return false;
    std::size_t next = focusedIndex_;
    for (std::size_t attempt = 0; attempt < children().size(); ++attempt) {
        next = static_cast<std::size_t>((static_cast<int>(next) + delta + static_cast<int>(children().size())) % static_cast<int>(children().size()));
        if (auto* tab = dynamic_cast<Tab*>(children()[next].get()); tab && tab->isEnabled()) {
            focusedIndex_ = next;
            for (const auto& child : children()) if (auto* candidate = dynamic_cast<Tab*>(child.get())) candidate->setVisualState(ControlVisualState::Focused, candidate == tab);
            return true;
        }
    }
    return false;
}
bool TabList::selectFocused()
{
    if (focusedIndex_ >= children().size()) return false;
    if (auto* tab = dynamic_cast<Tab*>(children()[focusedIndex_].get()); tab && tab->isEnabled()) { selectTab(*tab); return true; }
    return false;
}
bool TabList::onKeyEvent(const KeyEvent& event)
{
    if (event.action != KeyAction::Down) return false;
    if (event.keyCode == kEnter || event.keyCode == kSpace) return selectFocused();
    if (event.keyCode == kLeft) return activationMode_ == ActivationMode::Automatic ? moveSelection(-1) : moveFocus(-1);
    if (event.keyCode == kRight) return activationMode_ == ActivationMode::Automatic ? moveSelection(1) : moveFocus(1);
    const bool first = event.keyCode == kHome;
    if (first || event.keyCode == kEnd) {
        if (first) {
            for (std::size_t i = 0; i < children().size(); ++i) if (auto* tab = dynamic_cast<Tab*>(children()[i].get()); tab && tab->isEnabled()) { focusedIndex_ = i; if (activationMode_ == ActivationMode::Automatic) selectTab(*tab); for (const auto& item : children()) if (auto* candidate = dynamic_cast<Tab*>(item.get())) candidate->setVisualState(ControlVisualState::Focused, candidate == tab); return true; }
        } else {
            for (std::size_t i = children().size(); i-- > 0;) if (auto* tab = dynamic_cast<Tab*>(children()[i].get()); tab && tab->isEnabled()) { focusedIndex_ = i; if (activationMode_ == ActivationMode::Automatic) selectTab(*tab); for (const auto& item : children()) if (auto* candidate = dynamic_cast<Tab*>(item.get())) candidate->setVisualState(ControlVisualState::Focused, candidate == tab); return true; }
        }
    }
    return false;
}

TabPanel::TabPanel(std::string value) : value_(std::move(value)) {}
const std::string& TabPanel::value() const noexcept { return value_; }
TabPanel& TabPanel::value(std::string value) { setValue(std::move(value)); return *this; }
void TabPanel::setValue(std::string value) { if(value_!=value){value_=std::move(value);markDirty(DirtyFlag::Style);} }
TabPanel& TabPanel::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void TabPanel::setAccessibleLabel(std::string value) { accessibleLabel_=std::move(value);markDirty(DirtyFlag::Style); }
const std::string& TabPanel::accessibleLabel() const noexcept { return accessibleLabel_; }
TabPanel& TabPanel::tabList(TabList& value) noexcept { setTabList(&value); return *this; }
void TabPanel::setTabList(TabList* value) noexcept { if (tabList_ != value) { tabList_ = value; markDirty(DirtyFlag::Layout); } }
const TabList* TabPanel::tabList() const noexcept { return tabList_; }
bool TabPanel::isActive() const noexcept { return active_ && (!tabList_ || tabList_->value() == value_); }
TabPanel& TabPanel::active(bool value) noexcept { setActive(value); return *this; }
void TabPanel::setActive(bool value) noexcept { if (active_ != value) { active_ = value; markDirty(DirtyFlag::Layout); } }
SizeF TabPanel::measure(const Constraints& constraints) const
{
    if (!isActive()) return constraints.clamp({0, 0});
    float w=0,h=0;for(const auto&child:children()){auto s=child->measureWithConstraints(constraints);w=std::max(w,s.width);h+=s.height;}return constraints.clamp({w,h});
}
void TabPanel::layout(const RectF& rect)
{
    if (!isActive()) { Node::layout({rect.x, rect.y, 0, 0}); for (const auto& child : children()) child->layout({0, 0, 0, 0}); clearLayoutDirtyRecursively(); return; }
    Node::layout(rect);float y=rect.y;for(const auto&child:children()){auto s=child->measureWithConstraints({0,rect.width,0,std::max(0.f,rect.y+rect.height-y)});child->layout({rect.x,y,rect.width,s.height});y+=s.height;}clearLayoutDirtyRecursively();
}
void TabPanel::paint(PaintContext& context) { if (!isActive()) { clearDirty(DirtyFlag::Paint); return; } ContainerNode::paint(context); clearDirty(DirtyFlag::Paint); }
Node* TabPanel::hitTest(PointF point) { return isActive() ? ContainerNode::hitTest(point) : nullptr; }

Link::Link(std::string label) : label_(std::move(label)) {}
const std::string& Link::label() const noexcept { return label_; }
Link& Link::label(std::string value) { setLabel(std::move(value)); return *this; }
void Link::setLabel(std::string value) { if(label_!=value){label_=std::move(value);markDirty(DirtyFlag::Layout);} }
const std::string& Link::href() const noexcept { return href_; }
Link& Link::href(std::string value) { setHref(std::move(value)); return *this; }
void Link::setHref(std::string value) { href_=std::move(value);markDirty(DirtyFlag::Style); }
Link& Link::onInvoke(InvokeHandler handler) { onInvoke_=std::move(handler);return *this; }
SizeF Link::measure(const Constraints& constraints) const { const auto& style=theme().typography.body1;return constraints.clamp({textWidth(label_,style),style.lineHeight}); }
void Link::paint(PaintContext& context) { const auto&current=theme();const auto&style=current.typography.body1;const RectF rect=bounds();const bool disabled=!isEnabled();const bool pressed=hasState(*this,ControlVisualState::Pressed);const Color fg=disabled?current.colors.neutralForegroundDisabled:pressed?current.colors.brandBackground.pressed:current.colors.brandForeground1;focusRing(context,rect,current.radius.small,current,hasState(*this,ControlVisualState::Focused));context.drawText(label_,rect.x,context.centeredTextBottom(label_,rect,style.size,style.weight),style.size,fg,style.weight,style.family);if(!disabled){const float underline=pressed?2:1;context.fillRect({rect.x,rect.y+rect.height-underline,textWidth(label_,style),underline},fg);}clearDirty(DirtyFlag::Paint); }
bool Link::onPointerEvent(const PointerEvent&e){if(!isEnabled())return false;if(e.action==PointerAction::Enter){setVisualState(ControlVisualState::Hovered,true);return true;}if(e.action==PointerAction::Leave){setVisualState(ControlVisualState::Hovered,false);setVisualState(ControlVisualState::Pressed,false);return true;}if(e.action==PointerAction::Down&&e.button==MouseButton::Left){setVisualState(ControlVisualState::Pressed,true);setVisualState(ControlVisualState::Focused,true);return true;}if(e.action==PointerAction::Up&&e.button==MouseButton::Left){const bool activate=hasState(*this,ControlVisualState::Pressed)&&bounds().contains(e.position);setVisualState(ControlVisualState::Pressed,false);if(activate)invoke();return true;}if(e.action==PointerAction::Cancel){setVisualState(ControlVisualState::Pressed,false);return true;}return false;}
bool Link::onKeyEvent(const KeyEvent&e){if(!isEnabled()||e.action!=KeyAction::Down)return false;if(e.keyCode==kEnter||e.keyCode==kSpace){invoke();return true;}return false;}
AccessibilityActionCapabilities Link::accessibilityActions()const noexcept{AccessibilityActionCapabilities a;a.invoke=true;return a;} AccessibilityActionStatus Link::performAccessibilityAction(AccessibilityActionKind k,std::string_view){if(k!=AccessibilityActionKind::Invoke)return AccessibilityActionStatus::NotSupported;if(!isEnabled())return AccessibilityActionStatus::ElementNotEnabled;invoke();return AccessibilityActionStatus::Succeeded;}void Link::invoke(){if(isEnabled()&&onInvoke_)onInvoke_();}

BreadcrumbItem::BreadcrumbItem(std::string label,bool current):label_(std::move(label)),current_(current){}
const std::string& BreadcrumbItem::label()const noexcept{return label_;}BreadcrumbItem& BreadcrumbItem::label(std::string value){setLabel(std::move(value));return *this;}void BreadcrumbItem::setLabel(std::string value){if(label_!=value){label_=std::move(value);markDirty(DirtyFlag::Layout);}}bool BreadcrumbItem::isCurrent()const noexcept{return current_;}BreadcrumbItem& BreadcrumbItem::current(bool value)noexcept{setCurrent(value);return *this;}void BreadcrumbItem::setCurrent(bool value)noexcept{if(current_!=value){current_=value;markDirty(DirtyFlag::Paint);}}BreadcrumbItem& BreadcrumbItem::onInvoke(InvokeHandler handler){onInvoke_=std::move(handler);return *this;}
SizeF BreadcrumbItem::measure(const Constraints& constraints)const{const auto&style=current_?theme().typography.body1Strong:theme().typography.body1;return constraints.clamp({textWidth(label_,style),style.lineHeight});}
void BreadcrumbItem::paint(PaintContext& context)
{
    // Collapsed middle items retain their Node identity for a future overflow
    // menu, but must not paint at the default origin while hidden.
    if (bounds().width <= 0.0f || bounds().height <= 0.0f) {
        clearDirty(DirtyFlag::Paint);
        return;
    }
    const auto& current = theme(); const auto& style = current_ ? current.typography.body1Strong : current.typography.body1;
    const RectF rect = bounds();
    const Color fg = !isEnabled() ? current.colors.neutralForegroundDisabled
                   : current_ ? current.colors.neutralForeground1 : current.colors.brandForeground1;
    focusRing(context, rect, current.radius.small, current, hasState(*this, ControlVisualState::Focused));
    context.drawText(label_, rect.x, context.centeredTextBottom(label_, rect, style.size, style.weight),
                     style.size, fg, style.weight, style.family);
    if (!current_ && isEnabled()) context.fillRect({rect.x, rect.y + rect.height - 1, textWidth(label_, style), 1}, fg);
    clearDirty(DirtyFlag::Paint);
}
bool BreadcrumbItem::onPointerEvent(const PointerEvent&e){if(current_||!isEnabled())return false;if(e.action==PointerAction::Enter){setVisualState(ControlVisualState::Hovered,true);return true;}if(e.action==PointerAction::Leave){setVisualState(ControlVisualState::Hovered,false);setVisualState(ControlVisualState::Pressed,false);return true;}if(e.action==PointerAction::Down&&e.button==MouseButton::Left){setVisualState(ControlVisualState::Pressed,true);setVisualState(ControlVisualState::Focused,true);return true;}if(e.action==PointerAction::Up&&e.button==MouseButton::Left){const bool activate=hasState(*this,ControlVisualState::Pressed)&&bounds().contains(e.position);setVisualState(ControlVisualState::Pressed,false);if(activate)invoke();return true;}if(e.action==PointerAction::Cancel){setVisualState(ControlVisualState::Pressed,false);return true;}return false;}
bool BreadcrumbItem::onKeyEvent(const KeyEvent&e){if(current_||!isEnabled()||e.action!=KeyAction::Down)return false;if(e.keyCode==kEnter||e.keyCode==kSpace){invoke();return true;}return false;}AccessibilityActionCapabilities BreadcrumbItem::accessibilityActions()const noexcept{AccessibilityActionCapabilities a;a.invoke=!current_;return a;}AccessibilityActionStatus BreadcrumbItem::performAccessibilityAction(AccessibilityActionKind k,std::string_view){if(k!=AccessibilityActionKind::Invoke||current_)return AccessibilityActionStatus::NotSupported;if(!isEnabled())return AccessibilityActionStatus::ElementNotEnabled;invoke();return AccessibilityActionStatus::Succeeded;}void BreadcrumbItem::invoke(){if(!current_&&isEnabled()&&onInvoke_)onInvoke_();}

BreadcrumbItem& Breadcrumb::addItem(std::string label,bool current){auto item=std::make_unique<BreadcrumbItem>(std::move(label),current);auto*raw=item.get();appendChild(std::move(item));return *raw;}Breadcrumb& Breadcrumb::maxVisible(std::size_t value)noexcept{setMaxVisible(value);return *this;}void Breadcrumb::setMaxVisible(std::size_t value)noexcept{value=std::max<std::size_t>(2,value);if(maxVisible_!=value){maxVisible_=value;markDirty(DirtyFlag::Layout);}}std::size_t Breadcrumb::maxVisible()const noexcept{return maxVisible_;}Breadcrumb& Breadcrumb::accessibleLabel(std::string value){setAccessibleLabel(std::move(value));return *this;}void Breadcrumb::setAccessibleLabel(std::string value){accessibleLabel_=std::move(value);markDirty(DirtyFlag::Style);}const std::string&Breadcrumb::accessibleLabel()const noexcept{return accessibleLabel_;}
std::vector<std::size_t> Breadcrumb::visibleIndices()const{std::vector<std::size_t> result;const auto count=children().size();if(count<=maxVisible_){for(std::size_t i=0;i<count;++i)result.push_back(i);return result;}result.push_back(0);const std::size_t tail=std::max<std::size_t>(1,maxVisible_-1);for(std::size_t i=count-tail;i<count;++i)result.push_back(i);return result;}
std::vector<std::string> Breadcrumb::hiddenItems()const{std::vector<std::string> hidden;const auto visible=visibleIndices();for(std::size_t i=0;i<children().size();++i)if(std::find(visible.begin(),visible.end(),i)==visible.end())if(auto*item=dynamic_cast<BreadcrumbItem*>(children()[i].get()))hidden.push_back(item->label());return hidden;}
SizeF Breadcrumb::measure(const Constraints& constraints)const{const auto visible=visibleIndices();if(visible.empty())return constraints.clamp({0,0});const auto&style=theme().typography.body1;float w=0;for(auto index:visible)w+=children()[index]->measureWithConstraints(constraints).width;const float separator=textWidth("/",style)+theme().spacing.horizontal.s*2;w+=separator*float(visible.size()-1);if(visible.size()<children().size())w+=textWidth("…",style)+separator;return constraints.clamp({w,style.lineHeight});}
void Breadcrumb::layout(const RectF& rect){Node::layout(rect);const auto visible=visibleIndices();float x=rect.x;const float sep=textWidth("/",theme().typography.body1)+theme().spacing.horizontal.s*2;for(std::size_t i=0;i<children().size();++i)children()[i]->layout({0,0,0,0});for(std::size_t pos=0;pos<visible.size();++pos){if(pos==1&&visible.front()+1!=visible[pos])x+=textWidth("…",theme().typography.body1)+sep;const auto index=visible[pos];const auto size=children()[index]->measureWithConstraints({0,rect.width,0,rect.height});children()[index]->layout({x,rect.y+(rect.height-size.height)*.5f,size.width,size.height});x+=size.width;if(pos+1<visible.size())x+=sep;}clearLayoutDirtyRecursively();}
void Breadcrumb::paint(PaintContext& context){const auto visible=visibleIndices();const auto&current=theme();const auto&style=current.typography.body1;for(std::size_t pos=0;pos+1<visible.size();++pos){const auto*item=children()[visible[pos]].get();float x=item->bounds().x+item->bounds().width+current.spacing.horizontal.s;context.drawText("/",x,context.centeredTextBottom("/",{x,bounds().y,textWidth("/",style),bounds().height},style.size,style.weight),style.size,current.colors.neutralForeground3,style.weight,style.family);if(pos==0&&visible[pos]+1!=visible[pos+1]){const float ellipsisX=x+textWidth("/",style)+current.spacing.horizontal.s*2;context.drawText("…",ellipsisX,context.centeredTextBottom("…",{ellipsisX,bounds().y,textWidth("…",style),bounds().height},style.size,style.weight),style.size,current.colors.neutralForeground3,style.weight,style.family);}}ContainerNode::paint(context);clearDirty(DirtyFlag::Paint);}

} // namespace wui
