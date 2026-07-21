#include "wui/accordion.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

#include "wui/icons.h"
#include "wui/text_metrics.h"
#include "wui/theme.h"

namespace wui {
namespace {

constexpr int kEnter = 13;
constexpr int kSpace = 32;
constexpr int kHome = 36;
constexpr int kEnd = 35;
constexpr int kUp = 38;
constexpr int kDown = 40;
constexpr float kHeaderHeight = 44.0f;
constexpr float kHeaderPadding = 10.0f;
constexpr float kPanelPadding = 12.0f;
constexpr float kPanelBottomPadding = 12.0f;

float measureText(const std::string& value, const TextStyleToken& style) noexcept
{
    if (const auto* measurer = textMeasurer()) return measurer->measureText(value, style.size, style.weight).width;
    return static_cast<float>(value.size()) * style.size * 0.56f;
}

std::vector<std::string> wrapText(const std::string& value, float width, const TextStyleToken& style)
{
    std::vector<std::string> lines;
    if (value.empty()) return lines;
    width = std::max(1.0f, width);
    std::istringstream words(value);
    std::string word;
    std::string line;
    while (words >> word) {
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && measureText(candidate, style) > width) {
            lines.push_back(std::move(line));
            line = std::move(word);
        } else {
            line = std::move(candidate);
        }
    }
    if (!line.empty()) lines.push_back(std::move(line));
    return lines;
}

bool hasState(const ControlNode& node, ControlVisualState state) noexcept
{
    return (node.visualStates() & toMask(state)) != 0;
}

void paintFocusRing(PaintContext& context, const RectF& rect, const Theme& current, bool focused)
{
    if (!focused) return;
    const float outerWidth = context.snapStrokeWidth(current.stroke.thick);
    const float innerWidth = context.snapStrokeWidth(current.stroke.thin);
    const RectF outer = context.snapRectEdges(rect);
    context.strokeRoundRect(outer, current.radius.medium, outerWidth,
                            current.colors.strokeFocusInner);
    const RectF inner{
        outer.x + outerWidth, outer.y + outerWidth,
        std::max(0.0f, outer.width - outerWidth * 2.0f),
        std::max(0.0f, outer.height - outerWidth * 2.0f)};
    context.strokeRoundRect(
        inner, std::max(0.0f, current.radius.medium - outerWidth),
        innerWidth, current.colors.strokeFocusOuter);
}

} // namespace

AccordionItem::AccordionItem(std::string header, std::string body)
    : header_(std::move(header)), body_(std::move(body)) {}

const std::string& AccordionItem::header() const noexcept { return header_; }
AccordionItem& AccordionItem::header(std::string value) { setHeader(std::move(value)); return *this; }
void AccordionItem::setHeader(std::string value)
{
    if (header_ == value) return;
    header_ = std::move(value); markDirty(DirtyFlag::Layout);
}
const std::string& AccordionItem::body() const noexcept { return body_; }
AccordionItem& AccordionItem::body(std::string value) { setBody(std::move(value)); return *this; }
void AccordionItem::setBody(std::string value)
{
    if (body_ == value) return;
    body_ = std::move(value); markDirty(DirtyFlag::Layout);
}
bool AccordionItem::isExpanded() const noexcept { return expanded_; }
AccordionItem& AccordionItem::expanded(bool value) { setExpanded(value); return *this; }
void AccordionItem::setExpanded(bool value)
{
    if (auto* owner = dynamic_cast<Accordion*>(parent())) { (void)owner->setItemExpanded(*this, value); return; }
    setExpandedFromOwner(value, true);
}
AccordionItem& AccordionItem::onExpandedChange(ChangeHandler handler) { onExpandedChange_ = std::move(handler); return *this; }
AccordionItem& AccordionItem::content(std::unique_ptr<Node> value) { setContent(std::move(value)); return *this; }
void AccordionItem::setContent(std::unique_ptr<Node> value)
{
    clearChildren();
    if (value) appendChild(std::move(value));
    markDirty(DirtyFlag::Layout);
}
Node* AccordionItem::content() const noexcept { return children().empty() ? nullptr : children().front().get(); }

float AccordionItem::textBodyHeight(float availableWidth) const
{
    if (body_.empty()) return 0.0f;
    const auto& style = theme().typography.body1;
    return static_cast<float>(wrapText(body_, availableWidth, style).size()) * style.lineHeight;
}

SizeF AccordionItem::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    const float naturalWidth = std::max(kHeaderHeight,
        measureText(header_, current.typography.body1) +
            kHeaderPadding * 2.0f + 16.0f);
    const float proposedWidth = std::isfinite(constraints.maxWidth)
        ? std::max(constraints.minWidth, constraints.maxWidth)
        : naturalWidth;
    float bodyHeight = 0.0f;
    const float available =
        std::max(1.0f, proposedWidth - kPanelPadding * 2.0f);
    if (expanded_) {
        bodyHeight = textBodyHeight(available);
        if (const auto* child = content()) {
            bodyHeight += child->measureWithConstraints({0.0f, available, 0.0f, constraints.maxHeight}).height;
        }
        if (bodyHeight > 0.0f) bodyHeight += kPanelBottomPadding;
    }
    return constraints.clamp({proposedWidth, kHeaderHeight + bodyHeight});
}

RectF AccordionItem::headerBounds() const noexcept
{
    return {bounds().x, bounds().y, bounds().width, std::min(kHeaderHeight, bounds().height)};
}

void AccordionItem::layout(const RectF& rect)
{
    Node::layout(rect);
    if (auto* child = content()) {
        if (!expanded_) { child->layout({0, 0, 0, 0}); }
        else {
            const auto& current = theme();
            const float inset = kPanelPadding;
            const float textHeight = textBodyHeight(std::max(1.0f, rect.width - inset * 2.0f));
            const float contentY =
                rect.y + kHeaderHeight + textHeight;
            const float height = std::max(
                0.0f,
                rect.y + rect.height - kPanelBottomPadding - contentY);
            child->layout({rect.x + inset, contentY, std::max(0.0f, rect.width - inset * 2.0f), height});
        }
    }
    clearLayoutDirtyRecursively();
}

void AccordionItem::paint(PaintContext& context)
{
    const auto& current = theme();
    const RectF headerRect = context.snapRectEdges(headerBounds());
    if (headerRect.width <= 0.0f || headerRect.height <= 0.0f) { clearDirty(DirtyFlag::Paint); return; }
    const bool disabled = !isEnabled();
    Color fill{0, 0, 0, 0};
    if (!disabled && hasState(*this, ControlVisualState::Pressed)) fill = current.colors.neutralBackground1.pressed;
    else if (!disabled && hasState(*this, ControlVisualState::Hovered)) fill = current.colors.neutralBackground1.hover;
    if (fill.a != 0) context.fillRoundRect(headerRect, current.radius.medium, fill);
    paintFocusRing(context, headerRect, current, hasState(*this, ControlVisualState::Focused));

    const auto& titleStyle = current.typography.body1;
    const float textX = headerRect.x + kHeaderPadding;
    const Color foreground = disabled ? current.colors.neutralForegroundDisabled : current.colors.neutralForeground1;
    context.drawText(header_, textX, context.centeredTextBottom(header_, headerRect, titleStyle.size, titleStyle.weight),
                     titleStyle.size, foreground, titleStyle.weight, titleStyle.family);

    const float glyphSize = 20.0f;
    const float glyphX =
        headerRect.x + headerRect.width - kHeaderPadding - glyphSize;
    const float glyphY = headerRect.y + (headerRect.height - glyphSize) * 0.5f;
    drawIcon(context,
             expanded_ ? IconName::ChevronUp : IconName::ChevronDown,
             {glyphX, glyphY, glyphSize, glyphSize},
             disabled ? current.colors.neutralForegroundDisabled
                      : current.colors.neutralForeground2,
             IconSize::Size16);

    if (expanded_) {
        const float inset = kPanelPadding;
        float y = headerRect.y + headerRect.height;
        if (!body_.empty()) {
            const auto& bodyStyle = current.typography.body1;
            const float lineWidth = std::max(1.0f, bounds().width - inset * 2.0f);
            for (const std::string& line : wrapText(body_, lineWidth, bodyStyle)) {
                context.drawText(line, bounds().x + inset, y + bodyStyle.lineHeight,
                                 bodyStyle.size, disabled ? current.colors.neutralForegroundDisabled : current.colors.neutralForeground2,
                                 bodyStyle.weight, bodyStyle.family);
                y += bodyStyle.lineHeight;
            }
        }
        ContainerNode::paint(context);
    }
    clearDirty(DirtyFlag::Paint);
}

Node* AccordionItem::hitTest(PointF point)
{
    if (!bounds().contains(point)) return nullptr;
    if (expanded_) {
        for (auto it = children().rbegin(); it != children().rend(); ++it) {
            if (Node* hit = (*it)->hitTest(point)) return hit;
        }
    }
    return this;
}

bool AccordionItem::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled() || !headerBounds().contains(event.position)) return false;
    switch (event.action) {
    case PointerAction::Enter: setVisualState(ControlVisualState::Hovered, true); return true;
    case PointerAction::Leave: setVisualState(ControlVisualState::Hovered, false); setVisualState(ControlVisualState::Pressed, false); return true;
    case PointerAction::Down:
        if (event.button == MouseButton::Left) { setVisualState(ControlVisualState::Pressed, true); setVisualState(ControlVisualState::Focused, true); return true; }
        break;
    case PointerAction::Up:
        if (event.button == MouseButton::Left) {
            const bool activate = hasState(*this, ControlVisualState::Pressed);
            setVisualState(ControlVisualState::Pressed, false);
            if (activate) {
                if (auto* owner = dynamic_cast<Accordion*>(parent())) (void)owner->toggleItem(*this);
                else setExpanded(!expanded_);
            }
            return true;
        }
        break;
    case PointerAction::Cancel: setVisualState(ControlVisualState::Pressed, false); return true;
    default: break;
    }
    return false;
}

bool AccordionItem::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || event.action != KeyAction::Down) return false;
    if (event.keyCode == kEnter || event.keyCode == kSpace) {
        if (auto* owner = dynamic_cast<Accordion*>(parent())) (void)owner->toggleItem(*this);
        else setExpanded(!expanded_);
        return true;
    }
    if ((event.keyCode == kUp || event.keyCode == kDown || event.keyCode == kHome || event.keyCode == kEnd) && parent()) return parent()->onKeyEvent(event);
    return false;
}
AccessibilityActionCapabilities AccordionItem::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions; actions.invoke = true; actions.expandCollapse = true; return actions;
}
AccessibilityActionStatus AccordionItem::performAccessibilityAction(AccessibilityActionKind kind, std::string_view)
{
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    if (kind == AccessibilityActionKind::Invoke) { setExpanded(!expanded_); return AccessibilityActionStatus::Succeeded; }
    if (kind == AccessibilityActionKind::Expand && !expanded_) { setExpanded(true); return AccessibilityActionStatus::Succeeded; }
    if (kind == AccessibilityActionKind::Collapse && expanded_) { setExpanded(false); return AccessibilityActionStatus::Succeeded; }
    return kind == AccessibilityActionKind::Expand || kind == AccessibilityActionKind::Collapse
        ? AccessibilityActionStatus::Succeeded : AccessibilityActionStatus::NotSupported;
}
void AccordionItem::setExpandedFromOwner(bool value, bool notify)
{
    if (expanded_ == value) return;
    expanded_ = value;
    markDirty(DirtyFlag::Layout);
    if (notify && onExpandedChange_) onExpandedChange_(value);
}

AccordionItem& Accordion::addItem(std::string header, std::string body)
{
    auto item = std::make_unique<AccordionItem>(std::move(header), std::move(body));
    auto* raw = item.get(); appendChild(std::move(item));
    if (children().size() == 1) focusedIndex_ = 0;
    return *raw;
}
Accordion& Accordion::expandMode(AccordionExpandMode value) noexcept { setExpandMode(value); return *this; }
void Accordion::setExpandMode(AccordionExpandMode value) noexcept
{
    if (expandMode_ == value) return;
    expandMode_ = value;
    if (expandMode_ == AccordionExpandMode::Single) {
        bool seen = false;
        for (const auto& child : children()) if (auto* item = dynamic_cast<AccordionItem*>(child.get()); item && item->isExpanded()) {
            if (seen) item->setExpandedFromOwner(false, true); else seen = true;
        }
    }
    markDirty(DirtyFlag::Layout);
}
AccordionExpandMode Accordion::expandMode() const noexcept { return expandMode_; }
Accordion& Accordion::onExpandedChange(ChangeHandler handler) { onExpandedChange_ = std::move(handler); return *this; }
Accordion& Accordion::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void Accordion::setAccessibleLabel(std::string value) { accessibleLabel_ = std::move(value); markDirty(DirtyFlag::Style); }
const std::string& Accordion::accessibleLabel() const noexcept { return accessibleLabel_; }
std::size_t Accordion::focusedIndex() const noexcept { return focusedIndex_; }
std::vector<std::size_t> Accordion::expandedIndices() const
{
    std::vector<std::size_t> result;
    for (std::size_t index = 0; index < children().size(); ++index)
        if (const auto* item = dynamic_cast<const AccordionItem*>(children()[index].get()); item && item->isExpanded()) result.push_back(index);
    return result;
}
SizeF Accordion::measure(const Constraints& constraints) const
{
    float height = 0.0f, width = 0.0f;
    for (const auto& child : children()) {
        const auto size = child->measureWithConstraints(constraints);
        height += size.height;
        width = std::max(width, size.width);
    }
    return constraints.clamp({width, height});
}
void Accordion::layout(const RectF& rect)
{
    Node::layout(rect);
    float y = rect.y;
    for (const auto& child : children()) {
        const auto size = child->measureWithConstraints({0.0f, rect.width, 0.0f, std::max(0.0f, rect.y + rect.height - y)});
        child->layout({rect.x, y, rect.width, size.height}); y += size.height;
    }
    clearLayoutDirtyRecursively();
}
bool Accordion::onKeyEvent(const KeyEvent& event)
{
    if (event.action != KeyAction::Down || children().empty()) return false;
    if (event.keyCode == kUp) return moveFocus(-1);
    if (event.keyCode == kDown) return moveFocus(1);
    if (event.keyCode == kHome) return focusBoundary(false);
    if (event.keyCode == kEnd) return focusBoundary(true);
    return false;
}
std::size_t Accordion::indexOf(const AccordionItem& item) const noexcept
{
    for (std::size_t index = 0; index < children().size(); ++index) if (children()[index].get() == &item) return index;
    return children().size();
}
void Accordion::focusItem(std::size_t index)
{
    if (index >= children().size()) return;
    for (std::size_t position = 0; position < children().size(); ++position)
        if (auto* item = dynamic_cast<AccordionItem*>(children()[position].get())) item->setVisualState(ControlVisualState::Focused, position == index);
    focusedIndex_ = index;
}
bool Accordion::moveFocus(int delta)
{
    if (children().empty()) return false;
    const std::size_t count = children().size();
    for (std::size_t step = 1; step <= count; ++step) {
        const std::size_t candidate = (focusedIndex_ + count + (delta < 0 ? count - step % count : step % count)) % count;
        if (const auto* item = dynamic_cast<const AccordionItem*>(children()[candidate].get()); item && item->isEnabled()) { focusItem(candidate); return true; }
    }
    return false;
}
bool Accordion::focusBoundary(bool final)
{
    if (children().empty()) return false;
    if (final) {
        for (std::size_t index = children().size(); index-- > 0;) if (const auto* item = dynamic_cast<const AccordionItem*>(children()[index].get()); item && item->isEnabled()) { focusItem(index); return true; }
    } else {
        for (std::size_t index = 0; index < children().size(); ++index) if (const auto* item = dynamic_cast<const AccordionItem*>(children()[index].get()); item && item->isEnabled()) { focusItem(index); return true; }
    }
    return false;
}
bool Accordion::setItemExpanded(AccordionItem& item, bool value, bool notify)
{
    const std::size_t index = indexOf(item);
    if (index == children().size() || !item.isEnabled()) return false;
    if (value && expandMode_ == AccordionExpandMode::Single) {
        for (const auto& child : children()) if (auto* other = dynamic_cast<AccordionItem*>(child.get()); other && other != &item && other->isExpanded()) {
            other->setExpandedFromOwner(false, notify);
            if (notify && onExpandedChange_) onExpandedChange_(indexOf(*other), false);
        }
    }
    if (item.isExpanded() == value) return false;
    item.setExpandedFromOwner(value, notify);
    if (notify && onExpandedChange_) onExpandedChange_(index, value);
    markDirty(DirtyFlag::Layout);
    return true;
}
bool Accordion::toggleItem(AccordionItem& item) { return setItemExpanded(item, !item.isExpanded()); }

} // namespace wui
