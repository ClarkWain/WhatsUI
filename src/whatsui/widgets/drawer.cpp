#include "wui/drawer.h"

#include <algorithm>

#include "wui/icons.h"
#include <utility>

#include "wui/text_metrics.h"
#include "wui/theme.h"

namespace wui {
namespace {
constexpr float kPadding = 24.0f;
constexpr float kHeaderGap = 8.0f;
constexpr float kFooterGap = 12.0f;

float measureTextWidth(const std::string& value, const TextStyleToken& style) noexcept
{
    if (const auto* measurer = textMeasurer()) return measurer->measureText(value, style.size, style.weight).width;
    return static_cast<float>(value.size()) * style.size * 0.56f;
}

void shadow(PaintContext& context, const RectF& bounds, float radius, const ElevationToken& token)
{
    context.drawBoxShadow(bounds, radius, token.ambient.blur, token.ambient.offsetX, token.ambient.offsetY,
                          token.ambient.spread, token.ambient.color);
    context.drawBoxShadow(bounds, radius, token.key.blur, token.key.offsetX, token.key.offsetY,
                          token.key.spread, token.key.color);
}
} // namespace

Drawer::Drawer(std::string title, std::string subtitle)
    : title_(std::move(title)), subtitle_(std::move(subtitle)) {}

Drawer& Drawer::content(std::unique_ptr<Node> value) { clearChildren(); if (value) appendChild(std::move(value)); markDirty(DirtyFlag::Layout); return *this; }
Drawer& Drawer::title(std::string value) { title_ = std::move(value); markDirty(DirtyFlag::Layout); return *this; }
Drawer& Drawer::subtitle(std::string value) { subtitle_ = std::move(value); markDirty(DirtyFlag::Layout); return *this; }
Drawer& Drawer::primaryAction(std::string label, ActionHandler handler) { primaryLabel_ = std::move(label); primaryHandler_ = std::move(handler); markDirty(DirtyFlag::Layout); return *this; }
Drawer& Drawer::secondaryAction(std::string label, ActionHandler handler) { secondaryLabel_ = std::move(label); secondaryHandler_ = std::move(handler); markDirty(DirtyFlag::Layout); return *this; }
Drawer& Drawer::type(DrawerType value) noexcept { type_ = value; markDirty(DirtyFlag::Layout); return *this; }
Drawer& Drawer::position(DrawerPosition value) noexcept { position_ = value; markDirty(DirtyFlag::Layout); return *this; }
Drawer& Drawer::size(DrawerSize value) noexcept { size_ = value; explicitExtent_ = 0.0f; markDirty(DirtyFlag::Layout); return *this; }
Drawer& Drawer::width(float value) noexcept { explicitExtent_ = std::max(0.0f, value); markDirty(DirtyFlag::Layout); return *this; }
Drawer& Drawer::modal(bool value) noexcept { modal_ = value; markDirty(DirtyFlag::Paint); return *this; }
Drawer& Drawer::dismissOnOutsidePress(bool value) noexcept { dismissOnOutsidePress_ = value; return *this; }
Drawer& Drawer::closeOnEscape(bool value) noexcept { closeOnEscape_ = value; return *this; }
Drawer& Drawer::onDismiss(DismissHandler handler) { dismissHandler_ = std::move(handler); return *this; }
const std::string& Drawer::title() const noexcept { return title_; }
const std::string& Drawer::subtitle() const noexcept { return subtitle_; }
DrawerType Drawer::type() const noexcept { return type_; }
DrawerPosition Drawer::position() const noexcept { return position_; }
DrawerSize Drawer::size() const noexcept { return size_; }
bool Drawer::isModal() const noexcept { return modal_; }
bool Drawer::dismissesOnOutsidePress() const noexcept { return dismissOnOutsidePress_; }
bool Drawer::closesOnEscape() const noexcept { return closeOnEscape_; }
bool Drawer::trapsFocus() const noexcept { return type_ == DrawerType::Overlay && modal_; }
const RectF& Drawer::panelBounds() const noexcept { return panelBounds_; }
const RectF& Drawer::contentBounds() const noexcept { return contentBounds_; }
float Drawer::contentScrollOffset() const noexcept { return contentScrollOffset_; }
float Drawer::maxContentScrollOffset() const noexcept { return std::max(0.0f, contentSize_.height - contentBounds_.height); }
void Drawer::setContentScrollOffset(float value) noexcept { contentScrollOffset_ = value; clampScrollOffset(); markDirty(DirtyFlag::Paint); }

float Drawer::desiredExtent(const RectF& host) const noexcept
{
    if (explicitExtent_ > 0.0f) return explicitExtent_;
    if (size_ == DrawerSize::Full) return position_ == DrawerPosition::Bottom ? host.height : host.width;
    const float base = size_ == DrawerSize::Small ? 320.0f : size_ == DrawerSize::Large ? 640.0f : 480.0f;
    return std::min(base, position_ == DrawerPosition::Bottom ? host.height : host.width);
}

RectF Drawer::resolvePanel(const RectF& host) const noexcept
{
    if (type_ == DrawerType::Inline) return host;
    const float extent = desiredExtent(host);
    if (position_ == DrawerPosition::Start) return {host.x, host.y, extent, host.height};
    if (position_ == DrawerPosition::Bottom) return {host.x, host.y + host.height - extent, host.width, extent};
    return {host.x + host.width - extent, host.y, extent, host.height};
}

float Drawer::headerHeight() const noexcept
{
    const auto& typography = theme().typography;
    if (title_.empty() && subtitle_.empty()) return kPadding;
    return kPadding + typography.subtitle1.lineHeight + (subtitle_.empty() ? 0.0f : kHeaderGap + typography.body1.lineHeight) + kPadding;
}
float Drawer::footerHeight() const noexcept { return primaryLabel_.empty() && secondaryLabel_.empty() ? 0.0f : theme().controls.height + kPadding * 2.0f; }

SizeF Drawer::measure(const Constraints& constraints) const
{
    const float extent = position_ == DrawerPosition::Bottom ? desiredExtent({0, 0, constraints.maxWidth, constraints.maxHeight}) : desiredExtent({0, 0, constraints.maxWidth, constraints.maxHeight});
    if (type_ == DrawerType::Overlay) return constraints.clamp({constraints.maxWidth, constraints.maxHeight});
    return position_ == DrawerPosition::Bottom ? constraints.clamp({constraints.maxWidth, extent}) : constraints.clamp({extent, constraints.maxHeight});
}

void Drawer::layout(const RectF& bounds)
{
    Node::layout(bounds);
    panelBounds_ = resolvePanel(bounds);
    const float footer = footerHeight();
    contentBounds_ = {panelBounds_.x + kPadding, panelBounds_.y + headerHeight(),
                      std::max(0.0f, panelBounds_.width - kPadding * 2.0f),
                      std::max(0.0f, panelBounds_.height - headerHeight() - footer - kPadding)};
    contentSize_ = {};
    if (!children().empty()) {
        contentSize_ = children().front()->measureWithConstraints({0.0f, contentBounds_.width, 0.0f, 1000000.0f});
        children().front()->layout({contentBounds_.x, contentBounds_.y, contentBounds_.width, contentSize_.height});
    }
    clampScrollOffset();
    clearLayoutDirtyRecursively();
}

RectF Drawer::closeBounds() const noexcept { return {panelBounds_.x + panelBounds_.width - kPadding - 32.0f, panelBounds_.y + kPadding - 4.0f, 32.0f, 32.0f}; }
RectF Drawer::primaryBounds() const noexcept
{
    if (primaryLabel_.empty()) return {};
    const float width = std::max(80.0f, measureTextWidth(primaryLabel_, theme().typography.body1Strong) + 32.0f);
    return {panelBounds_.x + panelBounds_.width - kPadding - width, panelBounds_.y + panelBounds_.height - kPadding - theme().controls.height, width, theme().controls.height};
}
RectF Drawer::secondaryBounds() const noexcept
{
    if (secondaryLabel_.empty()) return {};
    const auto primary = primaryBounds(); const float width = std::max(80.0f, measureTextWidth(secondaryLabel_, theme().typography.body1Strong) + 32.0f);
    return {primary.x - kFooterGap - width, panelBounds_.y + panelBounds_.height - kPadding - theme().controls.height, width, theme().controls.height};
}

void Drawer::paint(PaintContext& context)
{
    const auto& current = theme();
    if (type_ == DrawerType::Overlay && modal_) context.fillRect(bounds(), current.colors.scrim);
    shadow(context, panelBounds_, position_ == DrawerPosition::Bottom ? current.radius.large : 0.0f, current.elevation.shadow64);
    context.fillRect(panelBounds_, current.colors.neutralBackground1.rest);
    if (position_ == DrawerPosition::Start) context.fillRect({panelBounds_.x + panelBounds_.width - current.stroke.thin, panelBounds_.y, current.stroke.thin, panelBounds_.height}, current.colors.neutralStroke1);
    else if (position_ == DrawerPosition::End) context.fillRect({panelBounds_.x, panelBounds_.y, current.stroke.thin, panelBounds_.height}, current.colors.neutralStroke1);
    else context.fillRect({panelBounds_.x, panelBounds_.y, panelBounds_.width, current.stroke.thin}, current.colors.neutralStroke1);
    float y = panelBounds_.y + kPadding;
    if (!title_.empty()) { context.drawText(title_, panelBounds_.x + kPadding, y + current.typography.subtitle1.lineHeight, current.typography.subtitle1.size, current.colors.neutralForeground1, current.typography.subtitle1.weight, current.typography.subtitle1.family); y += current.typography.subtitle1.lineHeight; }
    if (!subtitle_.empty()) { y += kHeaderGap; context.drawText(subtitle_, panelBounds_.x + kPadding, y + current.typography.body1.lineHeight, current.typography.body1.size, current.colors.neutralForeground2, current.typography.body1.weight, current.typography.body1.family); }
    const auto close = closeBounds();
    context.fillRoundRect(close, current.radius.medium, current.colors.neutralBackground1.hover);
    drawIcon(context, IconName::Dismiss, close,
             current.colors.neutralForeground1, IconSize::Size20);
    if (!children().empty()) { const int save = context.save(); context.clipRect(contentBounds_); context.translate(0.0f, -contentScrollOffset_); children().front()->paint(context); context.restore(); }
    const auto button = [&](const RectF& box, const std::string& label, bool primary) { if (box.width <= 0) return; context.fillRoundRect(box, current.radius.medium, primary ? current.colors.brandBackground.rest : current.colors.neutralBackground1.hover); context.drawText(label, box.x + (box.width - measureTextWidth(label, current.typography.body1Strong)) * .5f, context.centeredTextBottom(label, box, current.typography.body1Strong.size, current.typography.body1Strong.weight, current.typography.body1Strong.family), current.typography.body1Strong.size, primary ? current.colors.onBrand : current.colors.neutralForeground1, current.typography.body1Strong.weight, current.typography.body1Strong.family); };
    button(secondaryBounds(), secondaryLabel_, false); button(primaryBounds(), primaryLabel_, true);
    clearDirty(DirtyFlag::Paint);
}

Node* Drawer::hitTest(PointF point)
{
    if (!bounds().contains(point) || !panelBounds_.contains(point)) return type_ == DrawerType::Overlay ? this : nullptr;
    if (contentBounds_.contains(point) && !children().empty()) { const PointF translated{point.x, point.y + contentScrollOffset_}; if (auto* hit = children().front()->hitTest(translated)) return hit; }
    return this;
}

bool Drawer::onPointerEvent(const PointerEvent& event)
{
    if (event.action == PointerAction::Scroll && contentBounds_.contains(event.position)) { const float before = contentScrollOffset_; setContentScrollOffset(contentScrollOffset_ - event.scrollDelta.y); return before != contentScrollOffset_; }
    if (event.action != PointerAction::Up || event.button != MouseButton::Left) return type_ == DrawerType::Overlay;
    if (type_ == DrawerType::Overlay && !panelBounds_.contains(event.position)) { if (modal_ && dismissOnOutsidePress_) dismiss(); return true; }
    if (closeBounds().contains(event.position)) { dismiss(); return true; }
    if (primaryBounds().contains(event.position)) { invoke(primaryHandler_); return true; }
    if (secondaryBounds().contains(event.position)) { invoke(secondaryHandler_); return true; }
    return panelBounds_.contains(event.position);
}
bool Drawer::onKeyEvent(const KeyEvent& event)
{
    if (event.action != KeyAction::Down) return false;
    if (closeOnEscape_ && (event.keyCode == 27 || event.keyCode == 256)) { dismiss(); return true; }
    if (trapsFocus() && (event.keyCode == 9 || event.keyCode == 258)) return true;
    return false;
}
void Drawer::dismiss()
{
    // Either callback can remove this Drawer from OverlayHost synchronously.
    // Copy both first and do not access object state once the ownership
    // callback begins.
    auto author = dismissHandler_;
    auto overlay = overlayDismissHandler_;
    if (author) author();
    if (overlay) overlay();
}
void Drawer::clampScrollOffset() noexcept { contentScrollOffset_ = std::clamp(contentScrollOffset_, 0.0f, maxContentScrollOffset()); }
void Drawer::invoke(ActionHandler& action) { auto handler = action; if (handler) handler(); dismiss(); }
void Drawer::setOverlayDismissHandler(DismissHandler handler) noexcept { overlayDismissHandler_ = std::move(handler); }


} // namespace wui
