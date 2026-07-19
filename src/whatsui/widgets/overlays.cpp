#include "wui/overlays.h"
#include "wui/runtime.h"

#include "wui/text_metrics.h"
#include "wui/theme.h"

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
void drawFocusRing(PaintContext& context, const RectF& bounds, const Theme& current, bool focused)
{
    if (!focused) return;
    const float inset = current.controls.focusInset;
    context.strokeRoundRect(
        {bounds.x - inset, bounds.y - inset,
         bounds.width + inset * 2.0f,
         bounds.height + inset * 2.0f},
        current.radius.medium + inset, current.stroke.thick,
        current.colors.strokeFocusOuter);
    const float innerInset =
        std::max(0.0f, inset - current.stroke.thin * 0.5f);
    context.strokeRoundRect(
        {bounds.x - innerInset, bounds.y - innerInset,
         bounds.width + innerInset * 2.0f,
         bounds.height + innerInset * 2.0f},
        current.radius.medium + innerInset, current.stroke.thin,
        current.colors.strokeFocusInner);
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
    const ElevationToken& elevation = current.elevation.shadow16;
    for (const ShadowLayerToken* layer : {&elevation.ambient, &elevation.key}) {
        context.drawBoxShadow(panel, current.radius.xLarge, layer->blur, layer->offsetX,
                              layer->offsetY, layer->spread, layer->color);
    }
    context.fillStrokeRoundRect(panel, current.radius.xLarge,
                                current.stroke.thin,
                                current.colors.surfaceRaised,
                                current.colors.neutralStroke1);
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
Menu& Menu::clearItems() { items_.clear(); selectedIndex_ = -1; markDirty(DirtyFlag::Layout); return *this; }
Menu& Menu::onDismiss(DismissHandler handler) { onDismiss_ = std::move(handler); return *this; }
const std::vector<MenuItem>& Menu::items() const noexcept { return items_; }
int Menu::selectedIndex() const noexcept { return selectedIndex_; }
void Menu::setSelectedIndex(int index) noexcept { if (index >= 0 && index < static_cast<int>(items_.size()) && items_[static_cast<std::size_t>(index)].enabled) { selectedIndex_ = index; markDirty(DirtyFlag::Paint); } }
float Menu::rowHeight() const noexcept { return std::max(28.0f, theme().controls.height); }
SizeF Menu::measure(const Constraints& constraints) const
{
    float width = 160.0f;
    for (const auto& item : items_) {
        width = std::max(
            width,
            48.0f +
                static_cast<float>(item.label.size()) *
                    theme().typography.body1.size * 0.56f +
                static_cast<float>(item.shortcut.size()) *
                    theme().typography.caption1.size * 0.56f);
    }
    return constraints.clamp(
        {width, theme().spacing.vertical.xs * 2.0f +
                    rowHeight() * static_cast<float>(items_.size())});
}
void Menu::layout(const RectF& bounds) { Popup::layout(bounds); }
void Menu::paint(PaintContext& context)
{
    const auto panel = panelBounds(); const auto& current = theme(); paintSurface(context, panel);
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        const RectF row{
            panel.x + current.spacing.horizontal.xs,
            panel.y + current.spacing.vertical.xs +
                rowHeight() * static_cast<float>(i),
            std::max(0.0f,
                     panel.width -
                         current.spacing.horizontal.xs * 2.0f),
            rowHeight()};
        if (static_cast<int>(i) == selectedIndex_) {
            context.fillRoundRect(
                row, current.radius.medium,
                current.colors.neutralBackground1.hover);
        }
        const Color fg =
            item.enabled ? current.colors.neutralForeground1
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
                item.shortcut, current.typography.caption1.size);
            context.drawText(
                item.shortcut,
                row.x + row.width - current.spacing.horizontal.s -
                    sw,
                context.centeredTextBottom(
                    item.shortcut, row,
                    current.typography.caption1.size,
                    current.typography.caption1.weight,
                    current.typography.caption1.family),
                current.typography.caption1.size,
                current.colors.neutralForeground3,
                current.typography.caption1.weight,
                current.typography.caption1.family);
        }
    }
    clearDirty(DirtyFlag::Paint);
}
Node* Menu::hitTest(PointF point) { return Popup::hitTest(point); }
int Menu::itemAt(PointF point) const noexcept { const auto panel = panelBounds(); if (!panel.contains(point)) return -1; const int index = static_cast<int>((point.y - panel.y - theme().spacing.xs) / rowHeight()); return index >= 0 && index < static_cast<int>(items_.size()) ? index : -1; }
void Menu::moveSelection(int delta) noexcept
{
    if (items_.empty() || delta == 0) return; const int count = static_cast<int>(items_.size()); int candidate = selectedIndex_;
    for (int attempt = 0; attempt < count; ++attempt) { candidate = (candidate + delta + count) % count; if (items_[static_cast<std::size_t>(candidate)].enabled) { selectedIndex_ = candidate; markDirty(DirtyFlag::Paint); return; } }
}
void Menu::invokeSelection() { if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(items_.size())) return; auto& item = items_[static_cast<std::size_t>(selectedIndex_)]; if (!item.enabled) return; if (item.onInvoke) item.onInvoke(); dismiss(); }
bool Menu::onPointerEvent(const PointerEvent& event)
{
    if (event.action == PointerAction::Down && event.button == MouseButton::Left) { const int index = itemAt(event.position); if (index >= 0) { setSelectedIndex(index); return true; } }
    if (event.action == PointerAction::Up && event.button == MouseButton::Left) { const int index = itemAt(event.position); if (index >= 0 && index == selectedIndex_) { invokeSelection(); return true; } }
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
    Button::onClick([this] { openMenu(); });
}
MenuButton& MenuButton::addItem(MenuItem item) { items_.push_back(std::move(item)); return *this; }
MenuButton& MenuButton::clearItems() { items_.clear(); return *this; }
MenuButton& MenuButton::bindOverlayHost(OverlayHost& host) noexcept { overlayHost_ = &host; return *this; }
const std::vector<MenuItem>& MenuButton::items() const noexcept { return items_; }
bool MenuButton::isOpen() const noexcept { return open_; }
SizeF MenuButton::measure(const Constraints& constraints) const
{
    const SizeF base = Button::measure({constraints.minWidth,
                                        std::max(0.0f, constraints.maxWidth - 20.0f),
                                        constraints.minHeight, constraints.maxHeight});
    return constraints.clamp({base.width + 20.0f, base.height});
}
void MenuButton::paint(PaintContext& context)
{
    Button::paint(context);
    const auto& current = theme();
    Color foreground = current.colors.neutralForeground1;
    if (!isEnabled()) foreground = current.colors.neutralForegroundDisabled;
    else if (appearance() == ButtonAppearance::Primary || appearance() == ButtonAppearance::Danger) {
        foreground = current.colors.onBrand;
    }
    const float cx = bounds().x + bounds().width - 14.0f;
    const float cy = bounds().y + bounds().height * 0.5f;
    drawIcon(context, IconName::ChevronDown,
             {cx - 8.0f, cy - 8.0f, 16.0f, 16.0f}, foreground,
             IconSize::Size16);
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
{ const auto& current = theme(); return constraints.clamp({measuredTextWidth(label_, current.typography.body1Strong.size) + current.spacing.horizontal.m * 2.0f + 32.0f, 32.0f}); }
void SplitButton::paint(PaintContext& context)
{
    const auto& current = theme(); const bool focused = isEnabled() && (visualStates() & toMask(ControlVisualState::Focused));
    Color bg = current.colors.brandBackground.rest;
    if (!isEnabled()) bg = current.colors.neutralBackground3.rest;
    else if ((visualStates() & toMask(ControlVisualState::Pressed)) != 0) bg = current.colors.brandBackground.pressed;
    else if ((visualStates() & toMask(ControlVisualState::Hovered)) != 0) bg = current.colors.brandBackground.hover;
    drawFocusRing(context, bounds(), current, focused);
    context.fillRoundRect(bounds(), current.radius.medium, bg);
    const float arrowWidth = 32.0f; const float dividerX = bounds().x + bounds().width - arrowWidth;
    const Color foreground = isEnabled() ? current.colors.onBrand : current.colors.neutralForegroundDisabled;
    context.fillRect({dividerX, bounds().y + current.stroke.thin, current.stroke.thin, std::max(0.0f,bounds().height-current.stroke.thin*2.0f)}, foreground);
    context.drawText(label_, bounds().x + current.spacing.horizontal.m, context.centeredTextBottom(label_, bounds(), current.typography.body1Strong.size, current.typography.body1Strong.weight), current.typography.body1Strong.size, foreground, current.typography.body1Strong.weight);
    const float cx = dividerX + arrowWidth * 0.5f; const float cy = bounds().y + bounds().height * 0.5f;
    drawIcon(context, IconName::ChevronDown,
             {cx - 8.0f, cy - 8.0f, 16.0f, 16.0f}, foreground,
             IconSize::Size16);
    clearDirty(DirtyFlag::Paint);
}
bool SplitButton::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled()) return false;
    switch (event.action) {
    case PointerAction::Down: if (event.button == MouseButton::Left) { setVisualState(ControlVisualState::Pressed,true); setVisualState(ControlVisualState::Focused,true); return true; } return false;
    case PointerAction::Up: if (event.button == MouseButton::Left) { const bool active = (visualStates() & toMask(ControlVisualState::Pressed)) && bounds().contains(event.position); setVisualState(ControlVisualState::Pressed,false); if (active) { if (event.position.x >= bounds().x + bounds().width - 32.0f) openMenu(); else if (onClick_) onClick_(); } return true; } return false;
    case PointerAction::Enter: setVisualState(ControlVisualState::Hovered,true); return true;
    case PointerAction::Move: setVisualState(ControlVisualState::Hovered,bounds().contains(event.position)); return true;
    case PointerAction::Leave: setVisualState(ControlVisualState::Hovered,false); return true;
    case PointerAction::Cancel: setVisualState(ControlVisualState::Pressed,false); return true;
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
    open_ = true; setVisualState(ControlVisualState::Pressed, true);
    overlayId_ = overlayHost_->show(std::move(menu)); overlayHost_->focus(menuRaw); markDirty(DirtyFlag::Paint);
}
void SplitButton::closeMenu()
{
    if (!open_) return;
    OverlayHost* const host = overlayHost_;
    const auto id = overlayId_;
    open_ = false; overlayId_ = 0; setVisualState(ControlVisualState::Pressed, false);
    if (host != nullptr && id != 0) {
        [[maybe_unused]] auto dismissed = host->dismiss(id);
        host->focus(this);
    }
    markDirty(DirtyFlag::Paint);
}

Tooltip& Tooltip::text(std::string value) { text_ = std::move(value); markDirty(DirtyFlag::Layout); return *this; }
Tooltip& Tooltip::delay(std::chrono::milliseconds value) noexcept { delay_ = std::max(std::chrono::milliseconds{0}, value); return *this; }
Tooltip& Tooltip::showAfter(std::chrono::milliseconds elapsed) noexcept { elapsed_ = std::max(std::chrono::milliseconds{0}, elapsed); const bool next = elapsed_ >= delay_ && !text_.empty(); if (visible_ != next) { visible_ = next; markDirty(DirtyFlag::Paint); } return *this; }
Tooltip& Tooltip::hide() noexcept { elapsed_ = std::chrono::milliseconds{0}; if (visible_) { visible_ = false; markDirty(DirtyFlag::Paint); } return *this; }
const std::string& Tooltip::text() const noexcept { return text_; }
bool Tooltip::isVisible() const noexcept { return visible_; }
std::chrono::milliseconds Tooltip::delay() const noexcept { return delay_; }
SizeF Tooltip::measure(const Constraints& constraints) const { const auto& current = theme(); return constraints.clamp({std::max(48.0f, measuredTextWidth(text_, current.typography.caption1.size) + current.spacing.horizontal.m * 2.0f), current.typography.caption1.lineHeight + current.spacing.vertical.s * 2.0f}); }
void Tooltip::layout(const RectF& bounds) { Popup::layout(bounds); }
void Tooltip::paint(PaintContext& context) { if (!visible_) { clearDirty(DirtyFlag::Paint); return; } const auto panel = panelBounds(); const auto& current = theme(); context.fillRoundRect(panel, current.radius.medium, Color{50, 49, 48, 245}); context.drawText(text_, panel.x + current.spacing.horizontal.m, context.centeredTextBottom(text_, panel, current.typography.caption1.size, current.typography.caption1.weight, current.typography.caption1.family), current.typography.caption1.size, Color{255, 255, 255, 255}, current.typography.caption1.weight, current.typography.caption1.family); clearDirty(DirtyFlag::Paint); }
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
    const auto& current = theme(); Color background{0, 0, 0, 0};
    const bool focused = isEnabled() && (visualStates() & toMask(ControlVisualState::Focused)) != 0;
    if (!isEnabled()) background = current.colors.neutralBackground3.rest; else if ((visualStates() & toMask(ControlVisualState::Pressed)) != 0) background = current.colors.neutralBackground1.pressed; else if ((visualStates() & toMask(ControlVisualState::Hovered)) != 0) background = current.colors.neutralBackground1.hover;
    drawFocusRing(context, bounds(), current, focused);
    if (background.a != 0) context.fillRoundRect(bounds(), current.radius.medium, background);
    if (fluentIcon_) {
        drawIcon(context, *fluentIcon_, bounds(),
                 isEnabled() ? current.colors.neutralForeground1
                             : current.colors.neutralForegroundDisabled,
                 IconSize::Size20, iconStyle_);
    } else if (!icon_.empty()) {
        context.drawText(icon_, bounds().x + (bounds().width - measuredTextWidth(icon_, current.typography.body2.size)) * 0.5f,
                         context.centeredTextBottom(icon_, bounds(), current.typography.body2.size, current.typography.body2.weight, current.typography.body2.family), current.typography.body2.size,
                         isEnabled() ? current.colors.neutralForeground1 : current.colors.neutralForegroundDisabled,
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
