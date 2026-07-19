#include "wui/widgets.h"

#include <algorithm>
#include <utility>

#include "wui/theme.h"

namespace wui {
namespace {

void paintElevation(PaintContext& context, const RectF& bounds, float radius, const ElevationToken& elevation)
{
    for (const ShadowLayerToken* layer : {&elevation.ambient, &elevation.key}) {
        context.drawBoxShadow(bounds, radius, layer->blur, layer->offsetX, layer->offsetY,
                              layer->spread, layer->color);
    }
}

void drawFocusRing(PaintContext& context, const RectF& bounds, float radius,
                   const Theme& current, bool focused)
{
    if (!focused) return;
    const float inset = current.controls.focusInset;
    context.strokeRoundRect(
        {bounds.x - inset, bounds.y - inset,
         bounds.width + inset * 2.0f,
         bounds.height + inset * 2.0f},
        radius + inset, current.stroke.thick,
        current.colors.strokeFocusOuter);
    const float innerInset =
        std::max(0.0f, inset - current.stroke.thin * 0.5f);
    context.strokeRoundRect(
        {bounds.x - innerInset, bounds.y - innerInset,
         bounds.width + innerInset * 2.0f,
         bounds.height + innerInset * 2.0f},
        radius + innerInset, current.stroke.thin,
        current.colors.strokeFocusInner);
}

} // namespace

Card& Card::child(std::unique_ptr<Node> child) { appendChild(std::move(child)); return *this; }
void Card::setAppearance(CardAppearance appearance) noexcept { if (appearance_ != appearance) { appearance_ = appearance; markDirty(DirtyFlag::Paint); } }
CardAppearance Card::appearance() const noexcept { return appearance_; }
void Card::setSize(CardSize size) noexcept { if (size_ != size) { size_ = size; markDirty(DirtyFlag::Layout); } }
CardSize Card::size() const noexcept { return size_; }
void Card::setOrientation(CardOrientation orientation) noexcept { if (orientation_ != orientation) { orientation_ = orientation; markDirty(DirtyFlag::Layout); } }
CardOrientation Card::orientation() const noexcept { return orientation_; }
void Card::setSelected(bool selected) noexcept { if (selected_ != selected) { selected_ = selected; markDirty(DirtyFlag::Paint); } }
bool Card::isSelected() const noexcept { return selected_; }
Card& Card::selectable(bool value) noexcept
{
    if (selectable_ != value) {
        selectable_ = value;
        setVisualState(ControlVisualState::Pressed, false);
        markDirty(DirtyFlag::Paint);
    }
    return *this;
}
bool Card::isSelectable() const noexcept { return selectable_; }
Card& Card::onSelectionChange(ChangeHandler handler) { onSelectionChange_ = std::move(handler); return *this; }

InsetsF Card::padding() const noexcept
{
    const float value = size_ == CardSize::Small ? 8.0f : size_ == CardSize::Large ? 16.0f : 12.0f;
    return {value, value, value, value};
}

SizeF Card::measure(const Constraints& constraints) const
{
    const InsetsF insets = padding();
    const Constraints inner = constraints.deflate(insets);
    SizeF content{};
    for (const auto& child : children()) {
        const SizeF size = child->measureWithConstraints(inner);
        if (orientation_ == CardOrientation::Horizontal) { content.width += size.width; content.height = std::max(content.height, size.height); }
        else { content.width = std::max(content.width, size.width); content.height += size.height; }
    }
    if (children().size() > 1) { const float gap = orientation_ == CardOrientation::Horizontal ? theme().spacing.horizontal.s : theme().spacing.vertical.s; if (orientation_ == CardOrientation::Horizontal) content.width += gap * static_cast<float>(children().size() - 1); else content.height += gap * static_cast<float>(children().size() - 1); }
    return constraints.clamp({content.width + insets.horizontal(), content.height + insets.vertical()});
}

void Card::layout(const RectF& bounds)
{
    Node::layout(bounds);
    const InsetsF insets = padding();
    const RectF content{bounds.x + insets.left, bounds.y + insets.top,
                        std::max(0.0f, bounds.width - insets.horizontal()),
                        std::max(0.0f, bounds.height - insets.vertical())};
    float cursor = orientation_ == CardOrientation::Horizontal ? content.x : content.y;
    const float gap = orientation_ == CardOrientation::Horizontal ? theme().spacing.horizontal.s : theme().spacing.vertical.s;
    for (const auto& child : children()) {
        const SizeF size = child->measureWithConstraints({0.0f, content.width, 0.0f, content.height});
        if (orientation_ == CardOrientation::Horizontal) { child->layout({cursor, content.y, size.width, content.height}); cursor += size.width + gap; }
        else { child->layout({content.x, cursor, content.width, size.height}); cursor += size.height + gap; }
    }
    clearLayoutDirtyRecursively();
}

void Card::paint(PaintContext& context)
{
    const Theme& current = theme();
    const float radius = size_ == CardSize::Small ? current.radius.small
                       : size_ == CardSize::Large ? current.radius.large
                                                  : current.radius.medium;
    const bool enabled = isEnabled();
    const bool hovered = enabled && selectable_ && (visualStates() & toMask(ControlVisualState::Hovered)) != 0;
    const bool pressed = enabled && selectable_ && (visualStates() & toMask(ControlVisualState::Pressed)) != 0;
    const bool focused = enabled && selectable_ && (visualStates() & toMask(ControlVisualState::Focused)) != 0;
    const ColorTokens::Interaction* ramp = &current.colors.neutralCardBackground;
    Color fill = ramp->rest;
    bool stroke = false;
    Color strokeColor = current.colors.neutralStroke1;
    const ElevationToken* elevation = nullptr;
    switch (appearance_) {
    case CardAppearance::Filled:
        elevation = &current.elevation.shadow2;
        break;
    case CardAppearance::FilledAlternative:
        ramp = &current.colors.neutralBackground2;
        fill = ramp->rest;
        elevation = &current.elevation.shadow2;
        break;
    case CardAppearance::Outline: stroke = true; break;
    case CardAppearance::Subtle:
        fill = {0, 0, 0, 0};
        break;
    }
    if (!enabled) {
        // Disabled cards remain readable but cannot look elevated or react to
        // pointer state. The neutral surface and stroke match other disabled
        // Fluent controls without baking an opacity into child content.
        fill = current.colors.neutralBackground1.rest;
        stroke = appearance_ != CardAppearance::Subtle;
        strokeColor = current.colors.neutralStroke1;
        elevation = nullptr;
    } else if (selected_) {
        // Fluent selection is a neutral selected surface plus a brand stroke;
        // a solid brand fill would make arbitrary Card children unreadable.
        fill = ramp->selected;
        stroke = true;
        strokeColor = current.colors.brandForeground1;
        if (appearance_ == CardAppearance::Filled || appearance_ == CardAppearance::FilledAlternative) {
            elevation = hovered ? &current.elevation.shadow4 : &current.elevation.shadow2;
        }
    } else if (pressed) {
        fill = ramp->pressed;
        if (appearance_ == CardAppearance::Filled || appearance_ == CardAppearance::FilledAlternative) {
            elevation = &current.elevation.shadow2;
        }
    } else if (hovered) {
        fill = ramp->hover;
        if (appearance_ == CardAppearance::Filled || appearance_ == CardAppearance::FilledAlternative) {
            elevation = &current.elevation.shadow4;
        } else if (appearance_ == CardAppearance::Outline || appearance_ == CardAppearance::Subtle) {
            elevation = &current.elevation.shadow2;
        }
    }
    drawFocusRing(context, bounds(), radius, current, focused);
    if (elevation != nullptr) paintElevation(context, bounds(), radius, *elevation);
    if (stroke) {
        context.fillStrokeRoundRect(bounds(), radius,
                                    current.stroke.thin, fill,
                                    strokeColor);
    } else if (fill.a != 0) {
        context.fillRoundRect(bounds(), radius, fill);
    }
    ContainerNode::paint(context);
    clearDirty(DirtyFlag::Paint);
}

bool Card::onPointerEvent(const PointerEvent& event)
{
    if (!selectable_ || !isEnabled()) return false;
    switch (event.action) {
    case PointerAction::Down:
        if (event.button != MouseButton::Left) return false;
        setVisualState(ControlVisualState::Pressed, true);
        setVisualState(ControlVisualState::Focused, true);
        return true;
    case PointerAction::Up:
        if (event.button != MouseButton::Left) return false;
        {
            const bool activate = (visualStates() & toMask(ControlVisualState::Pressed)) != 0 && bounds().contains(event.position);
            setVisualState(ControlVisualState::Pressed, false);
            if (activate) {
                setSelected(!selected_);
                if (onSelectionChange_) onSelectionChange_(selected_);
            }
        }
        return true;
    case PointerAction::Enter:
        setVisualState(ControlVisualState::Hovered, true);
        return true;
    case PointerAction::Move:
        setVisualState(ControlVisualState::Hovered, bounds().contains(event.position));
        return true;
    case PointerAction::Leave:
        setVisualState(ControlVisualState::Hovered, false);
        return true;
    case PointerAction::Cancel:
        setVisualState(ControlVisualState::Pressed, false);
        return true;
    }
    return false;
}
bool Card::onKeyEvent(const KeyEvent& event)
{ if (!selectable_ || !isEnabled() || event.action != KeyAction::Down || (event.keyCode != 13 && event.keyCode != 32)) return false; setSelected(!selected_); if (onSelectionChange_) onSelectionChange_(selected_); return true; }
AccessibilityActionCapabilities Card::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions;
    actions.toggle = selectable_;
    return actions;
}
AccessibilityActionStatus Card::performAccessibilityAction(
    AccessibilityActionKind kind, std::string_view value)
{
    (void)value;
    if (kind != AccessibilityActionKind::Toggle) return AccessibilityActionStatus::NotSupported;
    if (!selectable_) return AccessibilityActionStatus::NotSupported;
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    setSelected(!selected_);
    if (onSelectionChange_) onSelectionChange_(selected_);
    return AccessibilityActionStatus::Succeeded;
}

CardHeader::CardHeader(std::string title, std::string description)
    : title_(std::move(title)), description_(std::move(description)) {}
void CardHeader::setTitle(std::string title) { title_ = std::move(title); markDirty(DirtyFlag::Layout); }
void CardHeader::setDescription(std::string description) { description_ = std::move(description); markDirty(DirtyFlag::Layout); }
const std::string& CardHeader::title() const noexcept { return title_; }
const std::string& CardHeader::description() const noexcept { return description_; }
CardHeader& CardHeader::media(std::unique_ptr<Node> media) { setMedia(std::move(media)); return *this; }
CardHeader& CardHeader::action(std::unique_ptr<Node> action) { setAction(std::move(action)); return *this; }
std::size_t CardHeader::actionIndex() const noexcept { return hasMedia_ ? 1u : 0u; }
void CardHeader::setMedia(std::unique_ptr<Node> media)
{
    if (hasMedia_) {
        static_cast<void>(removeChild(0));
        hasMedia_ = false;
    }
    if (media) {
        // The media slot always precedes the title block and action, even
        // when it is configured after action().
        insertChild(0, std::move(media));
        hasMedia_ = true;
    }
}
void CardHeader::setAction(std::unique_ptr<Node> action)
{
    if (hasAction_) {
        static_cast<void>(removeChild(actionIndex()));
        hasAction_ = false;
    }
    if (action) {
        // There are only two slots and their relative order is stable.
        appendChild(std::move(action));
        hasAction_ = true;
    }
}
Node* CardHeader::media() const noexcept
{
    return hasMedia_ && !children().empty() ? children().front().get() : nullptr;
}
Node* CardHeader::action() const noexcept
{
    const std::size_t index = actionIndex();
    return hasAction_ && index < children().size() ? children()[index].get() : nullptr;
}

SizeF CardHeader::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    const float titleHeight = title_.empty() ? 0.0f : current.typography.body1Strong.lineHeight;
    const float descriptionHeight = description_.empty() ? 0.0f : current.typography.caption1.lineHeight;
    float actionWidth = 0.0f, actionHeight = 0.0f, mediaWidth = 0.0f, mediaHeight = 0.0f;
    if (const Node* mediaNode = media()) { const SizeF media = mediaNode->measureWithConstraints(constraints); mediaWidth = media.width + current.spacing.horizontal.s; mediaHeight = media.height; }
    if (const Node* actionNode = action()) {
        const SizeF action = actionNode->measureWithConstraints(constraints);
        actionWidth = action.width + current.spacing.horizontal.m;
        actionHeight = action.height;
    }
    const float textWidth = std::max(static_cast<float>(title_.size()) * current.typography.body1Strong.size * 0.56f,
                                     static_cast<float>(description_.size()) * current.typography.caption1.size * 0.56f);
    return constraints.clamp({textWidth + mediaWidth + actionWidth, std::max({titleHeight + descriptionHeight, actionHeight, mediaHeight})});
}

void CardHeader::layout(const RectF& bounds)
{
    Node::layout(bounds);
    if (Node* mediaNode = media()) { const SizeF mediaSize = mediaNode->measureWithConstraints({0,bounds.width,0,bounds.height}); mediaNode->layout({bounds.x,bounds.y + std::max(0.0f,(bounds.height-mediaSize.height)*0.5f),mediaSize.width,mediaSize.height}); }
    if (Node* actionNode = action()) { const SizeF actionSize = actionNode->measureWithConstraints({0,bounds.width,0,bounds.height}); actionNode->layout({bounds.x+std::max(0.0f,bounds.width-actionSize.width),bounds.y,actionSize.width,actionSize.height}); }
    clearLayoutDirtyRecursively();
}

void CardHeader::paint(PaintContext& context)
{
    const auto& current = theme();
    const Node* mediaNode = media();
    const Node* actionNode = action();
    const float mediaWidth = mediaNode ? mediaNode->bounds().width + current.spacing.horizontal.s : 0.0f;
    const float actionWidth = actionNode ? actionNode->bounds().width + current.spacing.horizontal.m : 0.0f;
    const RectF textBounds{bounds().x + mediaWidth, bounds().y, std::max(0.0f, bounds().width - mediaWidth - actionWidth), bounds().height};
    if (!title_.empty()) {
        const RectF titleBox{textBounds.x, textBounds.y, textBounds.width, current.typography.body1Strong.lineHeight};
        context.drawText(title_, titleBox.x, context.centeredTextBottom(title_, titleBox, current.typography.body1Strong.size,
                         current.typography.body1Strong.weight), current.typography.body1Strong.size,
                         current.colors.neutralForeground1, current.typography.body1Strong.weight);
    }
    if (!description_.empty()) {
        const RectF descriptionBox{textBounds.x, textBounds.y + current.typography.body1Strong.lineHeight, textBounds.width,
                                   current.typography.caption1.lineHeight};
        context.drawText(description_, descriptionBox.x, context.centeredTextBottom(description_, descriptionBox,
                         current.typography.caption1.size, current.typography.caption1.weight), current.typography.caption1.size,
                         current.colors.neutralForeground3, current.typography.caption1.weight);
    }
    ContainerNode::paint(context);
    clearDirty(DirtyFlag::Paint);
}

CardPreview& CardPreview::child(std::unique_ptr<Node> child) { appendChild(std::move(child)); return *this; }
void CardPreview::setHeight(float value) noexcept { const float normalized = std::max(0.0f, value); if (height_ != normalized) { height_ = normalized; markDirty(DirtyFlag::Layout); } }
float CardPreview::height() const noexcept { return height_; }
SizeF CardPreview::measure(const Constraints& constraints) const
{
    SizeF result{};
    for (const auto& child : children()) result = child->measureWithConstraints(constraints);
    if (height_ > 0.0f) result.height = height_;
    return constraints.clamp(result);
}
void CardPreview::layout(const RectF& bounds)
{
    Node::layout(bounds);
    for (const auto& child : children()) child->layout(bounds);
    clearLayoutDirtyRecursively();
}
void CardPreview::paint(PaintContext& context)
{
    const int checkpoint = context.save();
    context.clipRoundRect(bounds(), theme().radius.medium);
    ContainerNode::paint(context);
    context.restoreTo(checkpoint);
    clearDirty(DirtyFlag::Paint);
}

CardFooter& CardFooter::child(std::unique_ptr<Node> child) { appendChild(std::move(child)); return *this; }
SizeF CardFooter::measure(const Constraints& constraints) const
{
    float width = 0.0f, height = 0.0f;
    for (const auto& child : children()) {
        const SizeF size = child->measureWithConstraints(constraints);
        width += size.width;
        height = std::max(height, size.height);
    }
    if (children().size() > 1) width += theme().spacing.horizontal.s * static_cast<float>(children().size() - 1);
    return constraints.clamp({width, height});
}
void CardFooter::layout(const RectF& bounds)
{
    Node::layout(bounds);
    if (children().empty()) {
        clearLayoutDirtyRecursively();
        return;
    }
    const float gap = theme().spacing.horizontal.s;
    const SizeF content = children().front()->measureWithConstraints({0.0f, bounds.width, 0.0f, bounds.height});
    children().front()->layout({bounds.x, bounds.y + std::max(0.0f, (bounds.height - content.height) * 0.5f),
                                content.width, content.height});
    float right = bounds.x + bounds.width;
    for (std::size_t index = children().size(); index-- > 1;) {
        const SizeF action = children()[index]->measureWithConstraints({0.0f, bounds.width, 0.0f, bounds.height});
        right -= action.width;
        children()[index]->layout({right, bounds.y + std::max(0.0f, (bounds.height - action.height) * 0.5f),
                                   action.width, action.height});
        right -= gap;
    }
    clearLayoutDirtyRecursively();
}

} // namespace wui
