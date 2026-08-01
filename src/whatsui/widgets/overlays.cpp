#include "wui/overlays.h"
#include "wui/runtime.h"

#include "wui/text_metrics.h"
#include "wui/theme.h"

#include "button_visuals.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace wui {
namespace {
float finiteOr(float value, float fallback) noexcept { return std::isfinite(value) ? value : fallback; }
float clampPanel(float value, float limit) noexcept { return std::max(0.0f, std::min(value, std::max(0.0f, limit))); }
float measuredTextWidth(const std::string& value, float textSize) noexcept
{
    if (const auto* measurer = textMeasurer()) return measurer->measureText(value, textSize).width;
    std::size_t codepoints = 0;
    for (const unsigned char character : value) if ((character & 0xC0u) != 0x80u) ++codepoints;
    return static_cast<float>(codepoints) * textSize * 0.5f;
}
} // namespace

PopupNode& PopupNode::content(std::unique_ptr<Node> content) { clearChildren(); if (content) appendChild(std::move(content)); return *this; }
PopupNode& PopupNode::anchor(RectF value) noexcept { anchor_ = value; markDirty(DirtyFlag::Layout); return *this; }
PopupNode& PopupNode::placement(PopupPlacement value) noexcept { placement_ = value; markDirty(DirtyFlag::Layout); return *this; }
PopupNode& PopupNode::preferredSize(SizeF value) noexcept { preferredSize_ = {std::max(0.0f, value.width), std::max(0.0f, value.height)}; markDirty(DirtyFlag::Layout); return *this; }
PopupNode& PopupNode::dismissOnOutsidePress(bool value) noexcept { dismissOnOutsidePress_ = value; return *this; }
PopupNode& PopupNode::onDismiss(DismissHandler handler) { onDismiss_ = std::move(handler); return *this; }
const RectF& PopupNode::anchor() const noexcept { return anchor_; }
const RectF& PopupNode::panelBounds() const noexcept { return panelBounds_; }
PopupPlacement PopupNode::placement() const noexcept { return placement_; }
bool PopupNode::dismissOnOutsidePress() const noexcept { return dismissOnOutsidePress_; }

SizeF PopupNode::measure(const Constraints& constraints) const
{
    SizeF desired = preferredSize_;
    if (!children().empty()) {
        const auto child = children().front()->measureWithConstraints({0.0f, constraints.maxWidth, 0.0f, constraints.maxHeight});
        if (desired.width <= 0.0f) desired.width = child.width;
        if (desired.height <= 0.0f) desired.height = child.height;
    }
    return constraints.clamp(desired);
}

RectF PopupNode::resolvePanelBounds(const RectF& host, SizeF desired) const noexcept
{
    constexpr float gap = 4.0f;
    const float right = host.x + host.width;
    const float bottom = host.y + host.height;
    const float width = clampPanel(finiteOr(desired.width, 0.0f), host.width);
    const float height = clampPanel(finiteOr(desired.height, 0.0f), host.height);
    const bool above = placement_ == PopupPlacement::AboveStart || placement_ == PopupPlacement::AboveEnd;
    const bool end = placement_ == PopupPlacement::BelowEnd || placement_ == PopupPlacement::AboveEnd;
    float x = end ? anchor_.x + anchor_.width - width : anchor_.x;
    float y = above ? anchor_.y - gap - height : anchor_.y + anchor_.height + gap;
    if (!above && y + height > bottom && anchor_.y - gap - height >= host.y) y = anchor_.y - gap - height;
    if (above && y < host.y && anchor_.y + anchor_.height + gap + height <= bottom) y = anchor_.y + anchor_.height + gap;
    x = std::clamp(x, host.x, std::max(host.x, right - width));
    y = std::clamp(y, host.y, std::max(host.y, bottom - height));
    return {x, y, width, height};
}

void PopupNode::layout(const RectF& bounds)
{
    Node::layout(bounds);
    panelBounds_ = resolvePanelBounds(bounds, measure({0.0f, bounds.width, 0.0f, bounds.height}));
    if (!children().empty()) children().front()->layout(panelBounds_);
    clearLayoutDirtyRecursively();
}

void PopupNode::paintSurface(PaintContext& context, const RectF& panel) const
{
    const auto& current = theme();
    const RectF alignedPanel = context.snapRectEdges(panel);
    const ElevationToken& elevation = current.elevation.shadow16;
    for (const ShadowLayerToken* layer : {&elevation.ambient, &elevation.key}) {
        context.drawBoxShadow(alignedPanel, current.radius.medium, layer->blur, layer->offsetX,
                              layer->offsetY, layer->spread, layer->color);
    }
    // Fluent's popup and menu surfaces use borderRadiusMedium (4 DIP) and a
    // transparent one-pixel border.  Painting only the raised fill avoids a
    // second grey contour while the shadow still defines the edge.
    context.fillRoundRect(alignedPanel, current.radius.medium,
                          current.colors.surfaceRaised);
}

void PopupNode::paint(PaintContext& context) { paintSurface(context, panelBounds_); ContainerNode::paint(context); clearDirty(DirtyFlag::Paint); }
Node* PopupNode::hitTest(PointF point)
{
    if (!bounds().contains(point)) return nullptr;
    if (panelBounds_.contains(point)) for (auto it = children().rbegin(); it != children().rend(); ++it) if (auto* hit = (*it)->hitTest(point)) return hit;
    return this;
}
bool PopupNode::onPointerEvent(const PointerEvent& event)
{
    if (event.action == PointerAction::Down && event.button == MouseButton::Left && !panelBounds_.contains(event.position)) { if (dismissOnOutsidePress_) dismiss(); return true; }
    return bounds().contains(event.position);
}
bool PopupNode::onKeyEvent(const KeyEvent& event) { if (event.action == KeyAction::Down && (event.keyCode == 27 || event.keyCode == 256)) { dismiss(); return true; } return false; }
void PopupNode::dismiss() { if (onDismiss_) onDismiss_(); }
const RectF& PopupNode::hostBounds() const noexcept { return bounds(); }

MenuNode& MenuNode::addItem(MenuItem item) { items_.push_back(std::move(item)); if (selectedIndex_ < 0) moveSelection(1); markDirty(DirtyFlag::Layout); return *this; }
MenuNode& MenuNode::clearItems() { items_.clear(); selectedIndex_ = hoveredIndex_ = pressedIndex_ = -1; markDirty(DirtyFlag::Layout); return *this; }
MenuNode& MenuNode::onDismiss(DismissHandler handler) { onDismiss_ = std::move(handler); return *this; }
const std::vector<MenuItem>& MenuNode::items() const noexcept { return items_; }
int MenuNode::selectedIndex() const noexcept { return selectedIndex_; }
void MenuNode::setSelectedIndex(int index) noexcept { if (index >= 0 && index < static_cast<int>(items_.size()) && items_[static_cast<std::size_t>(index)].enabled) { selectedIndex_ = index; markDirty(DirtyFlag::Paint); } }
float MenuNode::rowHeight() const noexcept { return std::max(28.0f, theme().controls.height); }
SizeF MenuNode::measure(const Constraints& constraints) const
{
    constexpr float kItemHorizontalPadding = 8.0f;
    constexpr float kContentGap = 8.0f;
    constexpr float kMenuPadding = 4.0f;
    constexpr float kRowGap = 2.0f;
    float width = 138.0f;
    for (const auto& item : items_) {
        const float shortcutWidth = item.shortcut.empty()
            ? 0.0f
            : measuredTextWidth(item.shortcut,
                                theme().typography.body1.size);
        width = std::max(width, kMenuPadding * 2.0f +
            kItemHorizontalPadding * 2.0f +
            measuredTextWidth(item.label, theme().typography.body1.size) +
            (item.shortcut.empty() ? 0.0f : kContentGap + shortcutWidth));
    }
    width = std::min(300.0f, width);
    const float gaps = items_.empty()
        ? 0.0f
        : kRowGap * static_cast<float>(items_.size() - 1);
    return constraints.clamp(
        {width, kMenuPadding * 2.0f +
                    rowHeight() * static_cast<float>(items_.size()) + gaps});
}
void MenuNode::layout(const RectF& bounds) { PopupNode::layout(bounds); }
void MenuNode::paint(PaintContext& context)
{
    const auto panel = panelBounds(); const auto& current = theme(); paintSurface(context, panel);
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        constexpr float kRowGap = 2.0f;
        const RectF row = context.snapRectEdges({
            panel.x + current.spacing.horizontal.xs,
            panel.y + current.spacing.vertical.xs +
                (rowHeight() + kRowGap) * static_cast<float>(i),
            std::max(0.0f,
                     panel.width -
                         current.spacing.horizontal.xs * 2.0f),
            rowHeight()});
        const int index = static_cast<int>(i);
        if (index == pressedIndex_) {
            context.fillRoundRect(
                row, current.radius.medium,
                current.colors.neutralBackground1.pressed);
        } else if (index == hoveredIndex_) {
            context.fillRoundRect(
                row, current.radius.medium,
                current.colors.neutralBackground1.hover);
        }
        const Color fg =
            item.enabled ? current.colors.neutralForeground2
                         : current.colors.neutralForegroundDisabled;
        context.drawText(
            item.label, row.x + current.spacing.horizontal.s,
            context.centeredTextBottom(
                item.label, row, current.typography.body1.size,
                current.typography.body1.weight,
                current.typography.body1.family),
            current.typography.body1.size, fg,
            current.typography.body1.weight,
            current.typography.body1.family);
        if (!item.shortcut.empty()) {
            const float sw = measuredTextWidth(
                item.shortcut, current.typography.body1.size);
            context.drawText(
                item.shortcut,
                row.x + row.width - current.spacing.horizontal.s -
                    sw,
                context.centeredTextBottom(
                    item.shortcut, row,
                    current.typography.body1.size,
                    current.typography.body1.weight,
                    current.typography.body1.family),
                current.typography.body1.size,
                current.colors.neutralForeground3,
                current.typography.body1.weight,
                current.typography.body1.family);
        }
        if (index == selectedIndex_) {
            const float outerWidth =
                context.snapStrokeWidth(current.stroke.thick);
            const float innerWidth =
                context.snapStrokeWidth(current.stroke.thin);
            const RectF focus = row;
            context.strokeRoundRect(
                focus, current.radius.medium, outerWidth,
                current.colors.strokeFocusInner);
            const RectF inner = {
                focus.x + outerWidth, focus.y + outerWidth,
                std::max(0.0f, focus.width - outerWidth * 2.0f),
                std::max(0.0f, focus.height - outerWidth * 2.0f)};
            context.strokeRoundRect(
                inner, std::max(0.0f, current.radius.medium - outerWidth),
                innerWidth, current.colors.strokeFocusOuter);
        }
    }
    clearDirty(DirtyFlag::Paint);
}
Node* MenuNode::hitTest(PointF point) { return PopupNode::hitTest(point); }
int MenuNode::itemAt(PointF point) const noexcept
{
    constexpr float kRowGap = 2.0f;
    const auto panel = panelBounds();
    if (!panel.contains(point)) return -1;
    const float local = point.y - panel.y - theme().spacing.vertical.xs;
    if (local < 0.0f) return -1;
    const float stride = rowHeight() + kRowGap;
    const int index = static_cast<int>(local / stride);
    if (index < 0 || index >= static_cast<int>(items_.size()) ||
        local - static_cast<float>(index) * stride >= rowHeight()) {
        return -1;
    }
    return index;
}
void MenuNode::moveSelection(int delta) noexcept
{
    if (items_.empty() || delta == 0) return; hoveredIndex_ = -1; pressedIndex_ = -1; const int count = static_cast<int>(items_.size()); int candidate = selectedIndex_;
    for (int attempt = 0; attempt < count; ++attempt) { candidate = (candidate + delta + count) % count; if (items_[static_cast<std::size_t>(candidate)].enabled) { selectedIndex_ = candidate; markDirty(DirtyFlag::Paint); return; } }
}
void MenuNode::invokeSelection() { if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(items_.size())) return; auto& item = items_[static_cast<std::size_t>(selectedIndex_)]; if (!item.enabled) return; if (item.onInvoke) item.onInvoke(); dismiss(); }
bool MenuNode::onPointerEvent(const PointerEvent& event)
{
    const int index = itemAt(event.position);
    if (event.action == PointerAction::Enter ||
        event.action == PointerAction::Move) {
        const int next = index >= 0 &&
            items_[static_cast<std::size_t>(index)].enabled ? index : -1;
        if (hoveredIndex_ != next) {
            hoveredIndex_ = next;
            markDirty(DirtyFlag::Paint);
        }
        return panelBounds().contains(event.position);
    }
    if (event.action == PointerAction::Leave) {
        hoveredIndex_ = -1;
        pressedIndex_ = -1;
        markDirty(DirtyFlag::Paint);
        return true;
    }
    if (event.action == PointerAction::Cancel) {
        pressedIndex_ = -1;
        markDirty(DirtyFlag::Paint);
        return true;
    }
    if (event.action == PointerAction::Down &&
        event.button == MouseButton::Left) {
        if (index >= 0 && items_[static_cast<std::size_t>(index)].enabled) {
            pressedIndex_ = index;
            markDirty(DirtyFlag::Paint);
            return true;
        }
    }
    if (event.action == PointerAction::Up &&
        event.button == MouseButton::Left) {
        const bool invoke = index >= 0 && index == pressedIndex_;
        pressedIndex_ = -1;
        markDirty(DirtyFlag::Paint);
        if (invoke) {
            selectedIndex_ = index;
            invokeSelection();
            return true;
        }
    }
    return PopupNode::onPointerEvent(event);
}
bool MenuNode::onKeyEvent(const KeyEvent& event)
{
    if (event.action != KeyAction::Down) return false;
    switch (event.keyCode) { case 38: case 265: moveSelection(-1); return true; case 40: case 264: moveSelection(1); return true; case 36: case 268: selectedIndex_ = -1; moveSelection(1); return true; case 35: case 269: selectedIndex_ = 0; moveSelection(-1); return true; case 13: case 32: case 257: invokeSelection(); return true; default: return PopupNode::onKeyEvent(event); }
}
void MenuNode::dismiss() { if (onDismiss_) onDismiss_(); }

MenuButtonNode::MenuButtonNode(std::string label)
    : ButtonNode(std::move(label))
{
    // A MenuButtonNode is a regular Fluent ButtonNode whose trailing content is the
    // standard disclosure glyph. Keeping it in ButtonNode's content layout gives
    // the pair the canonical 6-DIP gap and symmetric 12-DIP padding.
    setIcon(IconName::ChevronDown);
    setIconPosition(ButtonIconPosition::After);
    ButtonNode::onClick([this] { openMenu(); });
}
MenuButtonNode& MenuButtonNode::addItem(MenuItem item) { items_.push_back(std::move(item)); return *this; }
MenuButtonNode& MenuButtonNode::clearItems() { items_.clear(); return *this; }
MenuButtonNode& MenuButtonNode::bindOverlayHost(OverlayHost& host) noexcept { overlayHost_ = &host; return *this; }
const std::vector<MenuItem>& MenuButtonNode::items() const noexcept { return items_; }
bool MenuButtonNode::isOpen() const noexcept { return open_; }
SizeF MenuButtonNode::measure(const Constraints& constraints) const
{
    return ButtonNode::measure(constraints);
}
void MenuButtonNode::paint(PaintContext& context)
{
    ButtonNode::paint(context);
    clearDirty(DirtyFlag::Paint);
}
AccessibilityActionCapabilities MenuButtonNode::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions;
    actions.expandCollapse = overlayHost_ != nullptr && !items_.empty();
    return actions;
}
AccessibilityActionStatus MenuButtonNode::performAccessibilityAction(
    AccessibilityActionKind kind, std::string_view value)
{
    (void)value;
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    if (overlayHost_ == nullptr || items_.empty()) return AccessibilityActionStatus::NotSupported;
    if (kind == AccessibilityActionKind::Expand) {
        openMenu();
        return open_ ? AccessibilityActionStatus::Succeeded : AccessibilityActionStatus::Failed;
    }
    if (kind == AccessibilityActionKind::Collapse) {
        if (open_) closeMenu();
        return AccessibilityActionStatus::Succeeded;
    }
    return AccessibilityActionStatus::NotSupported;
}
void MenuButtonNode::openMenu()
{
    if (open_ || overlayHost_ == nullptr || items_.empty()) return;
    auto menu = std::make_unique<MenuNode>();
    MenuNode* const menuRaw = menu.get();
    menu->anchor(bounds()).placement(PopupPlacement::BelowStart);
    for (const auto& item : items_) menu->addItem(item);
    menu->onDismiss([this] { closeMenu(); });
    open_ = true;
    setVisualState(ControlVisualState::Pressed, true);
    overlayId_ = overlayHost_->show(std::move(menu));
    // OverlayHost's window change hook has finished by this point, so this
    // focus cannot be cleared by the show notification. Arrow/Enter/Escape
    // now route to the menu until it dismisses.
    overlayHost_->focus(menuRaw);
    markDirty(DirtyFlag::Paint);
}
void MenuButtonNode::closeMenu()
{
    if (!open_) return;
    OverlayHost* const host = overlayHost_;
    const auto id = overlayId_;
    open_ = false;
    overlayId_ = 0;
    setVisualState(ControlVisualState::Pressed, false);
    if (host != nullptr && id != 0) {
        [[maybe_unused]] auto dismissed = host->dismiss(id);
        host->focus(this);
    }
    markDirty(DirtyFlag::Paint);
}

SplitButtonNode::SplitButtonNode(std::string label) : label_(std::move(label)) {}
SplitButtonNode& SplitButtonNode::label(std::string value) { setLabel(std::move(value)); return *this; }
void SplitButtonNode::setLabel(std::string value) { label_ = std::move(value); markDirty(DirtyFlag::Layout); }
const std::string& SplitButtonNode::label() const noexcept { return label_; }
SplitButtonNode& SplitButtonNode::onClick(ClickHandler handler) { onClick_ = std::move(handler); return *this; }
SplitButtonNode& SplitButtonNode::addItem(MenuItem item) { items_.push_back(std::move(item)); return *this; }
SplitButtonNode& SplitButtonNode::bindOverlayHost(OverlayHost& host) noexcept { overlayHost_ = &host; return *this; }
bool SplitButtonNode::isOpen() const noexcept { return open_; }
SizeF SplitButtonNode::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    const auto textStyle =
        button_visuals::buttonTextStyle(current, ButtonSize::Medium);
    constexpr float disclosureWidth = 32.0f;
    return constraints.clamp(
        {button_visuals::measuredButtonTextWidth(label_, textStyle) +
             button_visuals::buttonHorizontalPadding(
                 current, ButtonSize::Medium) *
                 2.0f +
             disclosureWidth,
         button_visuals::buttonHeight(current, ButtonSize::Medium)});
}
void SplitButtonNode::paint(PaintContext& context)
{
    const auto& current = theme();
    constexpr float disclosureWidth = 32.0f;
    const float dividerX = context.snapToPhysicalPixel(
        bounds().x + std::max(0.0f, bounds().width - disclosureWidth));
    const RectF primaryBounds{
        bounds().x, bounds().y,
        std::max(0.0f, dividerX - bounds().x), bounds().height};
    const RectF disclosureBounds{
        dividerX, bounds().y,
        std::max(0.0f, bounds().x + bounds().width - dividerX),
        bounds().height};

    // SplitButtonNode is two adjacent commands, not one large pressed plate.
    // Paint the shared rest surface first, then apply hover/press only to the
    // region currently being operated while retaining a single outer radius.
    const auto interactiveMask =
        toMask(ControlVisualState::Hovered) |
        toMask(ControlVisualState::Pressed);
    const auto baseStates = visualStates() & ~interactiveMask;
    const auto visual = button_visuals::paintButtonSurface(
        context, bounds(), current, ButtonAppearance::Primary, !isEnabled(),
        false, baseStates);
    if (isEnabled()) {
        const bool pressed =
            (visualStates() & toMask(ControlVisualState::Pressed)) != 0;
        const bool hovered =
            (visualStates() & toMask(ControlVisualState::Hovered)) != 0;
        const bool disclosureActive =
            open_ || (pressed && disclosurePressed_);
        const bool primaryActive =
            pressed && !disclosurePressed_;
        const bool disclosureHot =
            !pressed && !open_ && hovered && disclosureHovered_;
        const bool primaryHot =
            !pressed && !open_ && hovered && !disclosureHovered_;
        const Color overlay =
            (disclosureActive || primaryActive)
            ? current.colors.brandBackground.pressed
            : current.colors.brandBackground.hover;
        if (disclosureActive || primaryActive || disclosureHot || primaryHot) {
            const bool disclosure = disclosureActive || disclosureHot;
            const RectF region = disclosure ? disclosureBounds : primaryBounds;
            const float radius = std::min(
                current.radius.medium,
                std::min(region.width, region.height) * 0.5f);
            // A path clip followed by a rectangular fill is not reliably
            // ordered by every OpenGL batching backend. Build the one-sided
            // rounded plate directly: a rounded rect preserves the outer
            // corners and a same-colour square fill removes only the two
            // corners that meet the internal separator.
            context.fillRoundRect(region, radius, overlay);
            if (disclosure) {
                context.fillRect(
                    {region.x, region.y,
                     std::max(0.0f, region.width - radius), region.height},
                    overlay);
            } else {
                context.fillRect(
                    {region.x + radius, region.y,
                     std::max(0.0f, region.width - radius), region.height},
                    overlay);
            }
        }
    }

    button_visuals::drawButtonContent(
        context, primaryBounds, label_, std::nullopt, IconStyle::Regular,
        ButtonIconPosition::Before, false, ButtonSize::Medium,
        visual.foreground, current);

    // The separator is a compound-brand stroke, inset vertically so it does
    // not collide with the four-DIP rounded outer corners.
    const Color divider = isEnabled()
        ? current.colors.onBrand
        : current.colors.neutralStrokeDisabled;
    context.fillRect(
        {dividerX, bounds().y + current.spacing.vertical.sNudge,
         context.snapStrokeWidth(current.stroke.thin),
         std::max(0.0f,
                  bounds().height - current.spacing.vertical.sNudge * 2.0f)},
        divider);
    const RectF disclosureSlot{
        dividerX + (disclosureWidth - 20.0f) * 0.5f,
        bounds().y + (bounds().height - 20.0f) * 0.5f, 20.0f, 20.0f};
    drawIcon(context, IconName::ChevronDown, disclosureSlot,
             visual.foreground, IconSize::Size16);
    clearDirty(DirtyFlag::Paint);
}
bool SplitButtonNode::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    const auto inDisclosure = [this](PointF point) {
        return point.x >= bounds().x + bounds().width - 32.0f;
    };
    switch (event.action) {
    case PointerAction::Down:
        if (event.button != MouseButton::Left) return false;
        disclosurePressed_ = inDisclosure(event.position);
        setVisualState(ControlVisualState::Pressed, true);
        setVisualState(ControlVisualState::Focused, true);
        return true;
    case PointerAction::Up:
        if (event.button != MouseButton::Left) return false;
        {
            const bool releaseDisclosure = inDisclosure(event.position);
            const bool active =
                (visualStates() & toMask(ControlVisualState::Pressed)) != 0 &&
                bounds().contains(event.position) &&
                releaseDisclosure == disclosurePressed_;
            setVisualState(ControlVisualState::Pressed, false);
            if (active) {
                if (releaseDisclosure) openMenu();
                else if (onClick_) onClick_();
            }
            if (!open_) disclosurePressed_ = false;
            return true;
        }
    case PointerAction::Enter:
        disclosureHovered_ = inDisclosure(event.position);
        setVisualState(ControlVisualState::Hovered, true);
        return true;
    case PointerAction::Move:
        {
            const bool nextDisclosureHovered =
                inDisclosure(event.position);
            if (disclosureHovered_ != nextDisclosureHovered) {
                disclosureHovered_ = nextDisclosureHovered;
                markDirty(DirtyFlag::Paint);
            }
        }
        setVisualState(ControlVisualState::Hovered,
                       bounds().contains(event.position));
        return true;
    case PointerAction::Leave:
        disclosureHovered_ = false;
        setVisualState(ControlVisualState::Hovered, false);
        return true;
    case PointerAction::Cancel:
        disclosurePressed_ = false;
        setVisualState(ControlVisualState::Pressed, false);
        return true;
    default: return false;
    }
}
bool SplitButtonNode::onKeyEvent(const KeyEvent& event)
{ if (!isEnabled() || event.action != KeyAction::Down) return false; if (event.keyCode == 40 || event.keyCode == 264 || event.keyCode == 293) { openMenu(); return true; } if (event.keyCode == 13 || event.keyCode == 32 || event.keyCode == 257) { if (onClick_) onClick_(); return true; } return false; }
AccessibilityActionCapabilities SplitButtonNode::accessibilityActions() const noexcept { AccessibilityActionCapabilities a; a.invoke = static_cast<bool>(onClick_); a.expandCollapse = overlayHost_ != nullptr && !items_.empty(); return a; }
AccessibilityActionStatus SplitButtonNode::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value)
{ (void)value; if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled; if (kind == AccessibilityActionKind::Invoke) { if (!onClick_) return AccessibilityActionStatus::NotSupported; onClick_(); return AccessibilityActionStatus::Succeeded; } if (kind == AccessibilityActionKind::Expand) { if (overlayHost_ == nullptr || items_.empty()) return AccessibilityActionStatus::NotSupported; openMenu(); return open_ ? AccessibilityActionStatus::Succeeded : AccessibilityActionStatus::Failed; } if (kind == AccessibilityActionKind::Collapse) { if (overlayHost_ == nullptr || items_.empty()) return AccessibilityActionStatus::NotSupported; if (open_) closeMenu(); return AccessibilityActionStatus::Succeeded; } return AccessibilityActionStatus::NotSupported; }
void SplitButtonNode::openMenu()
{
    if (open_ || overlayHost_ == nullptr || items_.empty()) return;
    auto menu = std::make_unique<MenuNode>(); MenuNode* const menuRaw = menu.get(); menu->anchor(bounds()).placement(PopupPlacement::BelowStart);
    for (const auto& item : items_) menu->addItem(item);
    menu->onDismiss([this] { closeMenu(); });
    open_ = true; disclosurePressed_ = true;
    setVisualState(ControlVisualState::Pressed, true);
    overlayId_ = overlayHost_->show(std::move(menu)); overlayHost_->focus(menuRaw); markDirty(DirtyFlag::Paint);
}
void SplitButtonNode::closeMenu()
{
    if (!open_) return;
    OverlayHost* const host = overlayHost_;
    const auto id = overlayId_;
    open_ = false; overlayId_ = 0; disclosurePressed_ = false;
    setVisualState(ControlVisualState::Pressed, false);
    if (host != nullptr && id != 0) {
        [[maybe_unused]] auto dismissed = host->dismiss(id);
        host->focus(this);
    }
    markDirty(DirtyFlag::Paint);
}

TooltipNode& TooltipNode::text(std::string value) { text_ = std::move(value); markDirty(DirtyFlag::Layout); return *this; }
TooltipNode& TooltipNode::appearance(TooltipAppearance value) noexcept { appearance_ = value; markDirty(DirtyFlag::Paint); return *this; }
TooltipNode& TooltipNode::delay(std::chrono::milliseconds value) noexcept { delay_ = std::max(std::chrono::milliseconds{0}, value); return *this; }
TooltipNode& TooltipNode::showAfter(std::chrono::milliseconds elapsed) noexcept { elapsed_ = std::max(std::chrono::milliseconds{0}, elapsed); const bool next = elapsed_ >= delay_ && !text_.empty(); if (visible_ != next) { visible_ = next; markDirty(DirtyFlag::Paint); } return *this; }
TooltipNode& TooltipNode::hide() noexcept { elapsed_ = std::chrono::milliseconds{0}; if (visible_) { visible_ = false; markDirty(DirtyFlag::Paint); } return *this; }
const std::string& TooltipNode::text() const noexcept { return text_; }
TooltipAppearance TooltipNode::appearance() const noexcept { return appearance_; }
bool TooltipNode::isVisible() const noexcept { return visible_; }
std::chrono::milliseconds TooltipNode::delay() const noexcept { return delay_; }
SizeF TooltipNode::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    return constraints.clamp({
        std::min(240.0f,
                 measuredTextWidth(text_, current.typography.caption1.size) +
                     current.spacing.horizontal.m * 2.0f),
        current.typography.caption1.lineHeight + 12.0f});
}
void TooltipNode::layout(const RectF& bounds) { PopupNode::layout(bounds); }
void TooltipNode::paint(PaintContext& context)
{
    if (!visible_) {
        clearDirty(DirtyFlag::Paint);
        return;
    }
    const RectF panel = context.snapRectEdges(panelBounds());
    const auto& current = theme();
    const auto& elevation = current.elevation.shadow8;
    for (const ShadowLayerToken* layer : {&elevation.ambient, &elevation.key}) {
        context.drawBoxShadow(panel, current.radius.medium, layer->blur,
                              layer->offsetX, layer->offsetY, layer->spread,
                              layer->color);
    }
    Color background = current.colors.neutralBackground1.rest;
    Color foreground = current.colors.neutralForeground1;
    if (appearance_ == TooltipAppearance::Brand) {
        background = current.colors.brandBackground.rest;
        foreground = current.colors.onBrand;
    } else if (appearance_ == TooltipAppearance::Inverted) {
        background = Color{41, 41, 41, 255};
        foreground = current.colors.onBrand;
    }
    context.fillRoundRect(panel, current.radius.medium, background);
    context.drawText(
        text_, panel.x + current.spacing.horizontal.m,
        context.centeredTextBottom(
            text_, panel, current.typography.caption1.size,
            current.typography.caption1.weight,
            current.typography.caption1.family),
        current.typography.caption1.size,
        foreground,
        current.typography.caption1.weight,
        current.typography.caption1.family);
    clearDirty(DirtyFlag::Paint);
}
Node* TooltipNode::hitTest(PointF point) { (void)point; return nullptr; }

IconButtonNode::IconButtonNode(std::string icon, std::string accessibleLabel) : icon_(std::move(icon)), accessibleLabel_(std::move(accessibleLabel)) {}
IconButtonNode::IconButtonNode(IconName icon, std::string accessibleLabel) : fluentIcon_(icon), accessibleLabel_(std::move(accessibleLabel)) {}
IconButtonNode& IconButtonNode::icon(std::string value) { setIcon(std::move(value)); return *this; }
IconButtonNode& IconButtonNode::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
IconButtonNode& IconButtonNode::checked(bool value) { setChecked(value); return *this; }
IconButtonNode& IconButtonNode::onClick(ClickHandler handler) { onClick_ = std::move(handler); return *this; }
void IconButtonNode::setIcon(std::string value) { icon_ = std::move(value); fluentIcon_.reset(); markDirty(DirtyFlag::Layout); }
void IconButtonNode::setIcon(IconName value) noexcept { fluentIcon_ = value; icon_.clear(); markDirty(DirtyFlag::Layout); }
void IconButtonNode::setIconStyle(IconStyle value) noexcept { if (iconStyle_ != value) { iconStyle_ = value; markDirty(DirtyFlag::Paint); } }
void IconButtonNode::setAccessibleLabel(std::string value) { accessibleLabel_ = std::move(value); }
void IconButtonNode::setChecked(std::optional<bool> value) noexcept { if (checked_ != value) { checked_ = value; markDirty(DirtyFlag::Paint); } }
const std::string& IconButtonNode::icon() const noexcept { return icon_; }
std::optional<IconName> IconButtonNode::fluentIcon() const noexcept { return fluentIcon_; }
IconStyle IconButtonNode::iconStyle() const noexcept { return iconStyle_; }
const std::string& IconButtonNode::accessibleLabel() const noexcept { return accessibleLabel_; }
std::optional<bool> IconButtonNode::checked() const noexcept { return checked_; }
SizeF IconButtonNode::measure(const Constraints& constraints) const { const float side = std::max(theme().controls.height, 32.0f); return constraints.clamp({side, side}); }
void IconButtonNode::paint(PaintContext& context)
{
    const auto& current = theme();
    const bool enabled = isEnabled();
    const bool selected = checked_.value_or(false);

    // IconButtonNode is the icon-only form of a medium Subtle ButtonNode. Reusing the
    // same resolver preserves rest/hover/pressed/selected/disabled/focus
    // semantics instead of maintaining a second state table.
    const auto visual = button_visuals::paintButtonSurface(
        context, bounds(), current, ButtonAppearance::Subtle, !enabled,
        selected, visualStates());
    if (fluentIcon_) {
        button_visuals::drawButtonContent(
            context, bounds(), {}, fluentIcon_,
            selected ? IconStyle::Filled : iconStyle_,
            ButtonIconPosition::Before, true, ButtonSize::Medium,
            visual.foreground, current);
    } else if (!icon_.empty()) {
        context.drawText(icon_, bounds().x + (bounds().width - measuredTextWidth(icon_, current.typography.body2.size)) * 0.5f,
                         context.centeredTextBottom(icon_, bounds(), current.typography.body2.size, current.typography.body2.weight, current.typography.body2.family), current.typography.body2.size,
                         visual.foreground,
                         current.typography.body2.weight, current.typography.body2.family);
    }
    clearDirty(DirtyFlag::Paint);
}
bool IconButtonNode::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    switch (event.action) { case PointerAction::Enter: setVisualState(ControlVisualState::Hovered, true); return true; case PointerAction::Leave: setVisualState(ControlVisualState::Hovered, false); return true; case PointerAction::Down: if (event.button == MouseButton::Left) { setVisualState(ControlVisualState::Pressed, true); setVisualState(ControlVisualState::Focused, true); return true; } return false; case PointerAction::Up: if (event.button == MouseButton::Left) { const bool invoke = (visualStates() & toMask(ControlVisualState::Pressed)) != 0 && bounds().contains(event.position); setVisualState(ControlVisualState::Pressed, false); if (invoke && onClick_) onClick_(); return true; } return false; case PointerAction::Cancel: setVisualState(ControlVisualState::Pressed, false); return true; default: return false; }
}
bool IconButtonNode::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || event.action != KeyAction::Down || (event.keyCode != 13 && event.keyCode != 32 && event.keyCode != 257)) return false;
    if (onClick_) onClick_();
    return true;
}
AccessibilityActionCapabilities IconButtonNode::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions;
    actions.toggle = checked_.has_value();
    actions.invoke = !checked_.has_value() && static_cast<bool>(onClick_);
    return actions;
}
AccessibilityActionStatus IconButtonNode::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value)
{
    (void)value;
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    if (kind == AccessibilityActionKind::Invoke && !checked_) {
        if (!onClick_) return AccessibilityActionStatus::NotSupported;
        onClick_();
        return AccessibilityActionStatus::Succeeded;
    }
    if (kind == AccessibilityActionKind::Toggle && checked_) {
        if (onClick_) onClick_(); else setChecked(!*checked_);
        return AccessibilityActionStatus::Succeeded;
    }
    return AccessibilityActionStatus::NotSupported;
}

SearchFieldNode::SearchFieldNode(std::string placeholder) : TextFieldNode(std::move(placeholder)) {}
SearchFieldNode& SearchFieldNode::query(std::string value) { TextFieldNode::text(std::move(value)); return *this; }
SearchFieldNode& SearchFieldNode::onQueryChange(ChangeHandler handler) { TextFieldNode::onChange(std::move(handler)); return *this; }
const std::string& SearchFieldNode::query() const noexcept { return controller().text(); }
bool SearchFieldNode::onKeyEvent(const KeyEvent& event) { if (event.action == KeyAction::Down && (event.keyCode == 27 || event.keyCode == 256) && !query().empty()) { TextFieldNode::text({}); return true; } return TextFieldNode::onKeyEvent(event); }

} // namespace wui
