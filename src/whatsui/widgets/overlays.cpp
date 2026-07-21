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

Popup& Popup::content(std::unique_ptr<Node> content) { clearChildren(); if (content) appendChild(std::move(content)); return *this; }
Popup& Popup::anchor(RectF value) noexcept { anchor_ = value; markDirty(DirtyFlag::Layout); return *this; }
Popup& Popup::placement(PopupPlacement value) noexcept { placement_ = value; markDirty(DirtyFlag::Layout); return *this; }
Popup& Popup::preferredSize(SizeF value) noexcept { preferredSize_ = {std::max(0.0f, value.width), std::max(0.0f, value.height)}; markDirty(DirtyFlag::Layout); return *this; }
Popup& Popup::dismissOnOutsidePress(bool value) noexcept { dismissOnOutsidePress_ = value; return *this; }
Popup& Popup::onDismiss(DismissHandler handler) { onDismiss_ = std::move(handler); return *this; }
const RectF& Popup::anchor() const noexcept { return anchor_; }
const RectF& Popup::panelBounds() const noexcept { return panelBounds_; }
PopupPlacement Popup::placement() const noexcept { return placement_; }
bool Popup::dismissOnOutsidePress() const noexcept { return dismissOnOutsidePress_; }

SizeF Popup::measure(const Constraints& constraints) const
{
    SizeF desired = preferredSize_;
    if (!children().empty()) {
        const auto child = children().front()->measureWithConstraints({0.0f, constraints.maxWidth, 0.0f, constraints.maxHeight});
        if (desired.width <= 0.0f) desired.width = child.width;
        if (desired.height <= 0.0f) desired.height = child.height;
    }
    return constraints.clamp(desired);
}

RectF Popup::resolvePanelBounds(const RectF& host, SizeF desired) const noexcept
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

void Popup::layout(const RectF& bounds)
{
    Node::layout(bounds);
    panelBounds_ = resolvePanelBounds(bounds, measure({0.0f, bounds.width, 0.0f, bounds.height}));
    if (!children().empty()) children().front()->layout(panelBounds_);
    clearLayoutDirtyRecursively();
}

void Popup::paintSurface(PaintContext& context, const RectF& panel) const
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

void Popup::paint(PaintContext& context) { paintSurface(context, panelBounds_); ContainerNode::paint(context); clearDirty(DirtyFlag::Paint); }
Node* Popup::hitTest(PointF point)
{
    if (!bounds().contains(point)) return nullptr;
    if (panelBounds_.contains(point)) for (auto it = children().rbegin(); it != children().rend(); ++it) if (auto* hit = (*it)->hitTest(point)) return hit;
    return this;
}
bool Popup::onPointerEvent(const PointerEvent& event)
{
    if (event.action == PointerAction::Down && event.button == MouseButton::Left && !panelBounds_.contains(event.position)) { if (dismissOnOutsidePress_) dismiss(); return true; }
    return bounds().contains(event.position);
}
bool Popup::onKeyEvent(const KeyEvent& event) { if (event.action == KeyAction::Down && (event.keyCode == 27 || event.keyCode == 256)) { dismiss(); return true; } return false; }
void Popup::dismiss() { if (onDismiss_) onDismiss_(); }
const RectF& Popup::hostBounds() const noexcept { return bounds(); }

Menu& Menu::addItem(MenuItem item) { items_.push_back(std::move(item)); if (selectedIndex_ < 0) moveSelection(1); markDirty(DirtyFlag::Layout); return *this; }
Menu& Menu::clearItems() { items_.clear(); selectedIndex_ = hoveredIndex_ = pressedIndex_ = -1; markDirty(DirtyFlag::Layout); return *this; }
Menu& Menu::onDismiss(DismissHandler handler) { onDismiss_ = std::move(handler); return *this; }
const std::vector<MenuItem>& Menu::items() const noexcept { return items_; }
int Menu::selectedIndex() const noexcept { return selectedIndex_; }
void Menu::setSelectedIndex(int index) noexcept { if (index >= 0 && index < static_cast<int>(items_.size()) && items_[static_cast<std::size_t>(index)].enabled) { selectedIndex_ = index; markDirty(DirtyFlag::Paint); } }
float Menu::rowHeight() const noexcept { return std::max(28.0f, theme().controls.height); }
SizeF Menu::measure(const Constraints& constraints) const
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
void Menu::layout(const RectF& bounds) { Popup::layout(bounds); }
void Menu::paint(PaintContext& context)
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
Node* Menu::hitTest(PointF point) { return Popup::hitTest(point); }
int Menu::itemAt(PointF point) const noexcept
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
void Menu::moveSelection(int delta) noexcept
{
    if (items_.empty() || delta == 0) return; hoveredIndex_ = -1; pressedIndex_ = -1; const int count = static_cast<int>(items_.size()); int candidate = selectedIndex_;
    for (int attempt = 0; attempt < count; ++attempt) { candidate = (candidate + delta + count) % count; if (items_[static_cast<std::size_t>(candidate)].enabled) { selectedIndex_ = candidate; markDirty(DirtyFlag::Paint); return; } }
}
void Menu::invokeSelection() { if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(items_.size())) return; auto& item = items_[static_cast<std::size_t>(selectedIndex_)]; if (!item.enabled) return; if (item.onInvoke) item.onInvoke(); dismiss(); }
bool Menu::onPointerEvent(const PointerEvent& event)
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
    return Popup::onPointerEvent(event);
}
bool Menu::onKeyEvent(const KeyEvent& event)
{
    if (event.action != KeyAction::Down) return false;
    switch (event.keyCode) { case 38: case 265: moveSelection(-1); return true; case 40: case 264: moveSelection(1); return true; case 36: case 268: selectedIndex_ = -1; moveSelection(1); return true; case 35: case 269: selectedIndex_ = 0; moveSelection(-1); return true; case 13: case 32: case 257: invokeSelection(); return true; default: return Popup::onKeyEvent(event); }
}
void Menu::dismiss() { if (onDismiss_) onDismiss_(); }

MenuButton::MenuButton(std::string label)
    : Button(std::move(label))
{
    // A MenuButton is a regular Fluent Button whose trailing content is the
    // standard disclosure glyph. Keeping it in Button's content layout gives
    // the pair the canonical 6-DIP gap and symmetric 12-DIP padding.
    setIcon(IconName::ChevronDown);
    setIconPosition(ButtonIconPosition::After);
    Button::onClick([this] { openMenu(); });
}
MenuButton& MenuButton::addItem(MenuItem item) { items_.push_back(std::move(item)); return *this; }
MenuButton& MenuButton::clearItems() { items_.clear(); return *this; }
MenuButton& MenuButton::bindOverlayHost(OverlayHost& host) noexcept { overlayHost_ = &host; return *this; }
const std::vector<MenuItem>& MenuButton::items() const noexcept { return items_; }
bool MenuButton::isOpen() const noexcept { return open_; }
SizeF MenuButton::measure(const Constraints& constraints) const
{
    return Button::measure(constraints);
}
void MenuButton::paint(PaintContext& context)
{
    Button::paint(context);
    clearDirty(DirtyFlag::Paint);
}
AccessibilityActionCapabilities MenuButton::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions;
    actions.expandCollapse = overlayHost_ != nullptr && !items_.empty();
    return actions;
}
AccessibilityActionStatus MenuButton::performAccessibilityAction(
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
void MenuButton::openMenu()
{
    if (open_ || overlayHost_ == nullptr || items_.empty()) return;
    auto menu = std::make_unique<Menu>();
    Menu* const menuRaw = menu.get();
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
void MenuButton::closeMenu()
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

SplitButton::SplitButton(std::string label) : label_(std::move(label)) {}
SplitButton& SplitButton::label(std::string value) { setLabel(std::move(value)); return *this; }
void SplitButton::setLabel(std::string value) { label_ = std::move(value); markDirty(DirtyFlag::Layout); }
const std::string& SplitButton::label() const noexcept { return label_; }
SplitButton& SplitButton::onClick(ClickHandler handler) { onClick_ = std::move(handler); return *this; }
SplitButton& SplitButton::addItem(MenuItem item) { items_.push_back(std::move(item)); return *this; }
SplitButton& SplitButton::bindOverlayHost(OverlayHost& host) noexcept { overlayHost_ = &host; return *this; }
bool SplitButton::isOpen() const noexcept { return open_; }
SizeF SplitButton::measure(const Constraints& constraints) const
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
void SplitButton::paint(PaintContext& context)
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

    // SplitButton is two adjacent commands, not one large pressed plate.
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
            const int checkpoint = context.save();
            context.clipRoundRect(bounds(), current.radius.medium);
            context.fillRect(
                (disclosureActive || disclosureHot)
                    ? disclosureBounds
                    : primaryBounds,
                overlay);
            context.restoreTo(checkpoint);
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
bool SplitButton::onPointerEvent(const PointerEvent& event)
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
bool SplitButton::onKeyEvent(const KeyEvent& event)
{ if (!isEnabled() || event.action != KeyAction::Down) return false; if (event.keyCode == 40 || event.keyCode == 264 || event.keyCode == 293) { openMenu(); return true; } if (event.keyCode == 13 || event.keyCode == 32 || event.keyCode == 257) { if (onClick_) onClick_(); return true; } return false; }
AccessibilityActionCapabilities SplitButton::accessibilityActions() const noexcept { AccessibilityActionCapabilities a; a.invoke = static_cast<bool>(onClick_); a.expandCollapse = overlayHost_ != nullptr && !items_.empty(); return a; }
AccessibilityActionStatus SplitButton::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value)
{ (void)value; if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled; if (kind == AccessibilityActionKind::Invoke) { if (!onClick_) return AccessibilityActionStatus::NotSupported; onClick_(); return AccessibilityActionStatus::Succeeded; } if (kind == AccessibilityActionKind::Expand) { if (overlayHost_ == nullptr || items_.empty()) return AccessibilityActionStatus::NotSupported; openMenu(); return open_ ? AccessibilityActionStatus::Succeeded : AccessibilityActionStatus::Failed; } if (kind == AccessibilityActionKind::Collapse) { if (overlayHost_ == nullptr || items_.empty()) return AccessibilityActionStatus::NotSupported; if (open_) closeMenu(); return AccessibilityActionStatus::Succeeded; } return AccessibilityActionStatus::NotSupported; }
void SplitButton::openMenu()
{
    if (open_ || overlayHost_ == nullptr || items_.empty()) return;
    auto menu = std::make_unique<Menu>(); Menu* const menuRaw = menu.get(); menu->anchor(bounds()).placement(PopupPlacement::BelowStart);
    for (const auto& item : items_) menu->addItem(item);
    menu->onDismiss([this] { closeMenu(); });
    open_ = true; disclosurePressed_ = true;
    setVisualState(ControlVisualState::Pressed, true);
    overlayId_ = overlayHost_->show(std::move(menu)); overlayHost_->focus(menuRaw); markDirty(DirtyFlag::Paint);
}
void SplitButton::closeMenu()
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

Tooltip& Tooltip::text(std::string value) { text_ = std::move(value); markDirty(DirtyFlag::Layout); return *this; }
Tooltip& Tooltip::appearance(TooltipAppearance value) noexcept { appearance_ = value; markDirty(DirtyFlag::Paint); return *this; }
Tooltip& Tooltip::delay(std::chrono::milliseconds value) noexcept { delay_ = std::max(std::chrono::milliseconds{0}, value); return *this; }
Tooltip& Tooltip::showAfter(std::chrono::milliseconds elapsed) noexcept { elapsed_ = std::max(std::chrono::milliseconds{0}, elapsed); const bool next = elapsed_ >= delay_ && !text_.empty(); if (visible_ != next) { visible_ = next; markDirty(DirtyFlag::Paint); } return *this; }
Tooltip& Tooltip::hide() noexcept { elapsed_ = std::chrono::milliseconds{0}; if (visible_) { visible_ = false; markDirty(DirtyFlag::Paint); } return *this; }
const std::string& Tooltip::text() const noexcept { return text_; }
TooltipAppearance Tooltip::appearance() const noexcept { return appearance_; }
bool Tooltip::isVisible() const noexcept { return visible_; }
std::chrono::milliseconds Tooltip::delay() const noexcept { return delay_; }
SizeF Tooltip::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    return constraints.clamp({
        std::min(240.0f,
                 measuredTextWidth(text_, current.typography.caption1.size) +
                     current.spacing.horizontal.m * 2.0f),
        current.typography.caption1.lineHeight + 12.0f});
}
void Tooltip::layout(const RectF& bounds) { Popup::layout(bounds); }
void Tooltip::paint(PaintContext& context)
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
Node* Tooltip::hitTest(PointF point) { (void)point; return nullptr; }

IconButton::IconButton(std::string icon, std::string accessibleLabel) : icon_(std::move(icon)), accessibleLabel_(std::move(accessibleLabel)) {}
IconButton::IconButton(IconName icon, std::string accessibleLabel) : fluentIcon_(icon), accessibleLabel_(std::move(accessibleLabel)) {}
IconButton& IconButton::icon(std::string value) { setIcon(std::move(value)); return *this; }
IconButton& IconButton::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
IconButton& IconButton::checked(bool value) { setChecked(value); return *this; }
IconButton& IconButton::onClick(ClickHandler handler) { onClick_ = std::move(handler); return *this; }
void IconButton::setIcon(std::string value) { icon_ = std::move(value); fluentIcon_.reset(); markDirty(DirtyFlag::Layout); }
void IconButton::setIcon(IconName value) noexcept { fluentIcon_ = value; icon_.clear(); markDirty(DirtyFlag::Layout); }
void IconButton::setIconStyle(IconStyle value) noexcept { if (iconStyle_ != value) { iconStyle_ = value; markDirty(DirtyFlag::Paint); } }
void IconButton::setAccessibleLabel(std::string value) { accessibleLabel_ = std::move(value); }
void IconButton::setChecked(std::optional<bool> value) noexcept { if (checked_ != value) { checked_ = value; markDirty(DirtyFlag::Paint); } }
const std::string& IconButton::icon() const noexcept { return icon_; }
std::optional<IconName> IconButton::fluentIcon() const noexcept { return fluentIcon_; }
IconStyle IconButton::iconStyle() const noexcept { return iconStyle_; }
const std::string& IconButton::accessibleLabel() const noexcept { return accessibleLabel_; }
std::optional<bool> IconButton::checked() const noexcept { return checked_; }
SizeF IconButton::measure(const Constraints& constraints) const { const float side = std::max(theme().controls.height, 32.0f); return constraints.clamp({side, side}); }
void IconButton::paint(PaintContext& context)
{
    const auto& current = theme();
    const bool enabled = isEnabled();
    const bool selected = checked_.value_or(false);

    // IconButton is the icon-only form of a medium Subtle Button. Reusing the
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
bool IconButton::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    switch (event.action) { case PointerAction::Enter: setVisualState(ControlVisualState::Hovered, true); return true; case PointerAction::Leave: setVisualState(ControlVisualState::Hovered, false); return true; case PointerAction::Down: if (event.button == MouseButton::Left) { setVisualState(ControlVisualState::Pressed, true); setVisualState(ControlVisualState::Focused, true); return true; } return false; case PointerAction::Up: if (event.button == MouseButton::Left) { const bool invoke = (visualStates() & toMask(ControlVisualState::Pressed)) != 0 && bounds().contains(event.position); setVisualState(ControlVisualState::Pressed, false); if (invoke && onClick_) onClick_(); return true; } return false; case PointerAction::Cancel: setVisualState(ControlVisualState::Pressed, false); return true; default: return false; }
}
bool IconButton::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || event.action != KeyAction::Down || (event.keyCode != 13 && event.keyCode != 32 && event.keyCode != 257)) return false;
    if (onClick_) onClick_();
    return true;
}
AccessibilityActionCapabilities IconButton::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions;
    actions.toggle = checked_.has_value();
    actions.invoke = !checked_.has_value() && static_cast<bool>(onClick_);
    return actions;
}
AccessibilityActionStatus IconButton::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value)
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

SearchField::SearchField(std::string placeholder) : TextInput(std::move(placeholder)) {}
SearchField& SearchField::query(std::string value) { TextInput::text(std::move(value)); return *this; }
SearchField& SearchField::onQueryChange(ChangeHandler handler) { TextInput::onChange(std::move(handler)); return *this; }
const std::string& SearchField::query() const noexcept { return controller().text(); }
bool SearchField::onKeyEvent(const KeyEvent& event) { if (event.action == KeyAction::Down && (event.keyCode == 27 || event.keyCode == 256) && !query().empty()) { TextInput::text({}); return true; } return TextInput::onKeyEvent(event); }

} // namespace wui
