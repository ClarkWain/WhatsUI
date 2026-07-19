#include "wui/popover.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "wui/runtime.h"
#include "wui/text_metrics.h"
#include "wui/theme.h"

namespace wui {
namespace {

constexpr float kPadding = 16.0f;
constexpr float kPopoverMinWidth = 240.0f;
constexpr float kPopoverMaxWidth = 420.0f;
constexpr float kArrowSize = 8.0f;

float textWidth(const std::string& value, const TextStyleToken& style) noexcept
{
    if (const auto* measurer = textMeasurer()) return measurer->measureText(value, style.size, style.weight).width;
    std::size_t count = 0;
    for (const unsigned char c : value) if ((c & 0xc0u) != 0x80u) ++count;
    return static_cast<float>(count) * style.size * 0.56f;
}

float wrappedHeight(const std::string& value, const TextStyleToken& style, float width) noexcept
{
    if (value.empty()) return 0.0f;
    const float safeWidth = std::max(1.0f, width);
    std::size_t lines = 1;
    std::string line;
    std::size_t start = 0;
    while (start < value.size()) {
        const auto end = value.find(' ', start);
        const std::string word = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && textWidth(candidate, style) > safeWidth) { ++lines; line = word; }
        else line = candidate;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return static_cast<float>(lines) * style.lineHeight;
}

std::vector<std::string> wrappedLines(const std::string& value, const TextStyleToken& style, float width)
{
    if (value.empty()) return {};
    std::vector<std::string> lines;
    std::string line;
    std::size_t start = 0;
    while (start < value.size()) {
        const auto end = value.find(' ', start);
        const std::string word = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && textWidth(candidate, style) > std::max(1.0f, width)) { lines.push_back(std::move(line)); line = word; }
        else line = candidate;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (!line.empty()) lines.push_back(std::move(line));
    return lines;
}

bool isAbove(PopupPlacement placement) noexcept
{
    return placement == PopupPlacement::AboveStart || placement == PopupPlacement::AboveEnd;
}

void paintElevation(PaintContext& context, const RectF& bounds, float radius, const Theme& current)
{
    const auto& elevation = current.elevation.shadow16;
    context.drawBoxShadow(bounds, radius, elevation.ambient.blur, elevation.ambient.offsetX,
                          elevation.ambient.offsetY, elevation.ambient.spread, elevation.ambient.color);
    context.drawBoxShadow(bounds, radius, elevation.key.blur, elevation.key.offsetX,
                          elevation.key.offsetY, elevation.key.spread, elevation.key.color);
}

} // namespace

Popover::Popover(std::string title, std::string body)
    : title_(std::move(title)), body_(std::move(body))
{
}

Popover& Popover::title(std::string value) { title_ = std::move(value); markDirty(DirtyFlag::Layout); return *this; }
Popover& Popover::body(std::string value) { body_ = std::move(value); markDirty(DirtyFlag::Layout); return *this; }
Popover& Popover::appearance(PopoverAppearance value) noexcept { appearance_ = value; markDirty(DirtyFlag::Paint); return *this; }
Popover& Popover::showArrow(bool value) noexcept { arrow_ = value; markDirty(DirtyFlag::Paint); return *this; }
Popover& Popover::accessibleLabel(std::string value) { accessibleLabel_ = std::move(value); return *this; }
const std::string& Popover::title() const noexcept { return title_; }
const std::string& Popover::body() const noexcept { return body_; }
PopoverAppearance Popover::appearance() const noexcept { return appearance_; }
bool Popover::hasArrow() const noexcept { return arrow_; }
const std::string& Popover::accessibleLabel() const noexcept { return accessibleLabel_; }
RectF Popover::contentBounds() const noexcept { return contentBounds_; }

SizeF Popover::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    const float maxWidth = std::min(kPopoverMaxWidth, std::max(kPopoverMinWidth, constraints.maxWidth));
    const float naturalText = std::max(textWidth(title_, current.typography.subtitle2), textWidth(body_, current.typography.body1));
    float width = std::clamp(naturalText + kPadding * 2.0f, kPopoverMinWidth, maxWidth);
    if (!children().empty()) width = std::max(width, std::min(maxWidth, children().front()->measureWithConstraints({0, maxWidth - kPadding * 2.0f, 0, constraints.maxHeight}).width + kPadding * 2.0f));
    const float contentWidth = std::max(1.0f, width - kPadding * 2.0f);
    float height = kPadding * 2.0f + headerHeight() + wrappedHeight(title_, current.typography.subtitle2, contentWidth);
    if (!title_.empty() && !body_.empty()) height += current.spacing.vertical.s;
    height += wrappedHeight(body_, current.typography.body1, contentWidth);
    if (!children().empty()) height += children().front()->measureWithConstraints({0, contentWidth, 0, constraints.maxHeight}).height + current.spacing.vertical.s;
    height += footerHeight();
    return constraints.clamp({width, std::max(48.0f, height)});
}

void Popover::layout(const RectF& bounds)
{
    Popup::layout(bounds);
    const auto panel = panelBounds();
    const float top = panel.y + kPadding;
    const auto& current = theme();
    const float titleHeight = wrappedHeight(title_, current.typography.subtitle2, std::max(1.0f, panel.width - kPadding * 2.0f));
    const float bodyHeight = wrappedHeight(body_, current.typography.body1, std::max(1.0f, panel.width - kPadding * 2.0f));
    float y = top + headerHeight() + titleHeight + ((!title_.empty() && !body_.empty()) ? current.spacing.vertical.s : 0.0f) + bodyHeight;
    if (!children().empty()) {
        y += current.spacing.vertical.s;
        contentBounds_ = {panel.x + kPadding, y, std::max(0.0f, panel.width - kPadding * 2.0f), std::max(0.0f, panel.y + panel.height - footerHeight() - kPadding - y)};
        children().front()->layout(contentBounds_);
    } else {
        contentBounds_ = {};
    }
    clearLayoutDirtyRecursively();
}

Color Popover::backgroundColor() const noexcept
{
    const auto& colors = theme().colors;
    switch (appearance_) {
    case PopoverAppearance::Inverted: return Color{36, 36, 36, 255};
    case PopoverAppearance::Brand: return colors.brandBackground.rest;
    case PopoverAppearance::Surface: default: return colors.neutralBackground1.rest;
    }
}

Color Popover::foregroundColor() const noexcept
{
    return appearance_ == PopoverAppearance::Surface ? theme().colors.neutralForeground1 : theme().colors.onBrand;
}

void Popover::paintArrow(PaintContext& context, const RectF& panel, Color color) const
{
    if (!arrow_) return;
    const float center = std::clamp(anchor().x + anchor().width * 0.5f, panel.x + 16.0f, panel.x + panel.width - 16.0f);
    const bool above = isAbove(placement());
    // The arrow is painted over the panel, then its base seam is erased with
    // the panel fill.  This keeps the sloped sides visually connected to the
    // surface rather than leaving a standalone outlined triangle perched on
    // top of a horizontal border.
    const float y = above ? panel.y + panel.height : panel.y;
    const std::vector<PointF> outer = above
        ? std::vector<PointF>{{center - kArrowSize, y - 1.0f}, {center + kArrowSize, y - 1.0f}, {center, y + kArrowSize}}
        : std::vector<PointF>{{center - kArrowSize, y + 1.0f}, {center + kArrowSize, y + 1.0f}, {center, y - kArrowSize}};
    constexpr float inset = 1.25f;
    const std::vector<PointF> inner = above
        ? std::vector<PointF>{{center - kArrowSize + inset, y - 1.0f}, {center + kArrowSize - inset, y - 1.0f}, {center, y + kArrowSize - inset}}
        : std::vector<PointF>{{center - kArrowSize + inset, y + 1.0f}, {center + kArrowSize - inset, y + 1.0f}, {center, y - kArrowSize + inset}};
    context.fillPolygon(outer, theme().colors.neutralStroke1);
    context.fillPolygon(inner, color);
    // Remove the horizontal panel border only at the arrow base.  The stroke
    // on the two sloped arrow edges remains, producing a continuous callout.
    context.fillRect(above ? RectF{center - kArrowSize, y - 1.0f, kArrowSize * 2.0f, 1.0f}
                            : RectF{center - kArrowSize, y, kArrowSize * 2.0f, 1.0f}, color);
}

void Popover::paint(PaintContext& context)
{
    const auto panel = panelBounds();
    const auto& current = theme();
    const Color bg = backgroundColor();
    paintElevation(context, panel, current.radius.large, current);
    context.fillStrokeRoundRect(panel, current.radius.large,
                                current.stroke.thin, bg,
                                current.colors.neutralStroke1);
    paintArrow(context, panel, bg);
    const float contentWidth = std::max(1.0f, panel.width - kPadding * 2.0f);
    float y = panel.y + kPadding + headerHeight();
    for (const auto& lineText : wrappedLines(title_, current.typography.subtitle2, contentWidth)) {
        const RectF line{panel.x + kPadding, y, contentWidth, current.typography.subtitle2.lineHeight};
        context.drawText(lineText, line.x, context.centeredTextBottom(lineText, line, current.typography.subtitle2.size,
                         current.typography.subtitle2.weight), current.typography.subtitle2.size, foregroundColor(), current.typography.subtitle2.weight);
        y += current.typography.subtitle2.lineHeight;
    }
    if (!title_.empty() && !body_.empty()) y += current.spacing.vertical.s;
    for (const auto& lineText : wrappedLines(body_, current.typography.body1, contentWidth)) {
        const RectF line{panel.x + kPadding, y, contentWidth, current.typography.body1.lineHeight};
        context.drawText(lineText, line.x, context.centeredTextBottom(lineText, line, current.typography.body1.size,
                         current.typography.body1.weight), current.typography.body1.size, foregroundColor(), current.typography.body1.weight);
        y += current.typography.body1.lineHeight;
    }
    for (const auto& child : children()) if (child) child->paint(context);
    clearDirty(DirtyFlag::Paint);
}

float Popover::headerHeight() const noexcept { return 0.0f; }
float Popover::footerHeight() const noexcept { return 0.0f; }
float Popover::bodyBottom() const noexcept { return contentBounds_.y + contentBounds_.height; }

PopoverButton::PopoverButton(std::string label) : Button(std::move(label))
{
    Button::onClick([this] { if (open_) closePopover(); else openPopover(); });
}
PopoverButton& PopoverButton::bindOverlayHost(OverlayHost& host) noexcept { overlayHost_ = &host; return *this; }
PopoverButton& PopoverButton::popoverFactory(PopoverFactory factory) { factory_ = std::move(factory); return *this; }
PopoverButton& PopoverButton::popover(std::string title, std::string body)
{
    factory_ = [title = std::move(title), body = std::move(body)] { return std::make_unique<Popover>(title, body); };
    return *this;
}
bool PopoverButton::isOpen() const noexcept { return open_; }
AccessibilityActionCapabilities PopoverButton::accessibilityActions() const noexcept
{
    auto actions = Button::accessibilityActions();
    actions.invoke = false;
    actions.expandCollapse = true;
    return actions;
}
AccessibilityActionStatus PopoverButton::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value)
{
    (void)value;
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    if (kind == AccessibilityActionKind::Expand) { openPopover(); return open_ ? AccessibilityActionStatus::Succeeded : AccessibilityActionStatus::NotSupported; }
    if (kind == AccessibilityActionKind::Collapse) { closePopover(); return AccessibilityActionStatus::Succeeded; }
    return AccessibilityActionStatus::NotSupported;
}
void PopoverButton::openPopover()
{
    if (open_ || overlayHost_ == nullptr || !factory_) return;
    auto surface = factory_();
    if (!surface) return;
    Popover* raw = surface.get();
    surface->anchor(bounds()).placement(PopupPlacement::BelowStart).onDismiss([this] { closePopover(); });
    open_ = true;
    setVisualState(ControlVisualState::Pressed, true);
    overlayId_ = overlayHost_->show(std::move(surface));
    // A regular Popover is non-modal supplemental content and must not steal
    // keyboard focus. A TeachingPopover defaults to a guided dialog boundary;
    // applications may make it explicitly non-modal when appropriate.
    const auto* teaching = dynamic_cast<const TeachingPopover*>(raw);
    if (teaching != nullptr && teaching->focusPolicy() == TeachingPopoverFocusPolicy::TrapFocus) {
        overlayHost_->focus(raw);
    }
    markDirty(DirtyFlag::Paint);
}
void PopoverButton::closePopover()
{
    if (!open_) return;
    OverlayHost* const host = overlayHost_;
    const auto id = overlayId_;
    open_ = false;
    overlayId_ = 0;
    setVisualState(ControlVisualState::Pressed, false);
    if (host && id != 0) { [[maybe_unused]] auto dismissed = host->dismiss(id); host->focus(this); }
    markDirty(DirtyFlag::Paint);
}

TeachingPopover::TeachingPopover(std::string title, std::string body) : Popover(std::move(title), std::move(body)) {}
TeachingPopover& TeachingPopover::primaryAction(std::string label, ActionHandler handler) { primaryLabel_ = std::move(label); primaryHandler_ = std::move(handler); markDirty(DirtyFlag::Layout); return *this; }
TeachingPopover& TeachingPopover::secondaryAction(std::string label, ActionHandler handler) { secondaryLabel_ = std::move(label); secondaryHandler_ = std::move(handler); markDirty(DirtyFlag::Layout); return *this; }
TeachingPopover& TeachingPopover::dismissLabel(std::string label) { dismissLabel_ = std::move(label); markDirty(DirtyFlag::Layout); return *this; }
TeachingPopover& TeachingPopover::stepText(std::string value) { stepText_ = std::move(value); markDirty(DirtyFlag::Layout); return *this; }
TeachingPopover& TeachingPopover::focusPolicy(TeachingPopoverFocusPolicy value) noexcept { focusPolicy_ = value; return *this; }
TeachingPopover& TeachingPopover::onDismiss(DismissHandler handler) { dismissHandler_ = std::move(handler); return *this; }
const std::string& TeachingPopover::primaryActionLabel() const noexcept { return primaryLabel_; }
const std::string& TeachingPopover::secondaryActionLabel() const noexcept { return secondaryLabel_; }
const std::string& TeachingPopover::dismissLabel() const noexcept { return dismissLabel_; }
const std::string& TeachingPopover::stepText() const noexcept { return stepText_; }
TeachingPopoverFocusPolicy TeachingPopover::focusPolicy() const noexcept { return focusPolicy_; }
float TeachingPopover::headerHeight() const noexcept { return stepText_.empty() ? 0.0f : theme().typography.caption1.lineHeight + theme().spacing.vertical.xs; }
float TeachingPopover::footerHeight() const noexcept { return primaryLabel_.empty() && secondaryLabel_.empty() ? 0.0f : theme().controls.height + theme().spacing.vertical.m + theme().spacing.vertical.l; }
SizeF TeachingPopover::measure(const Constraints& constraints) const { return Popover::measure(constraints); }
void TeachingPopover::layout(const RectF& bounds) { Popover::layout(bounds); }
RectF TeachingPopover::primaryBounds() const noexcept
{
    if (primaryLabel_.empty()) return {};
    const auto panel = panelBounds(); const float width = std::max(72.0f, textWidth(primaryLabel_, theme().typography.body1Strong) + 24.0f);
    return {panel.x + panel.width - kPadding - width, panel.y + panel.height - kPadding - theme().controls.height, width, theme().controls.height};
}
RectF TeachingPopover::secondaryBounds() const noexcept
{
    if (secondaryLabel_.empty()) return {};
    const auto primary = primaryBounds(); const auto panel = panelBounds(); const float width = std::max(72.0f, textWidth(secondaryLabel_, theme().typography.body1Strong) + 24.0f);
    return {primary.x - theme().spacing.horizontal.s - width, panel.y + panel.height - kPadding - theme().controls.height, width, theme().controls.height};
}
RectF TeachingPopover::dismissBounds() const noexcept { const auto panel = panelBounds(); return {panel.x + panel.width - kPadding - 24.0f, panel.y + kPadding - 2.0f, 24.0f, 24.0f}; }
void TeachingPopover::paint(PaintContext& context)
{
    Popover::paint(context);
    const auto& current = theme(); const auto panel = panelBounds();
    if (!stepText_.empty()) context.drawText(stepText_, panel.x + kPadding, panel.y + kPadding * 0.5f + current.typography.caption1.lineHeight,
        current.typography.caption1.size, current.colors.brandForeground1, current.typography.caption1.weight);
    const auto dismiss = dismissBounds();
    context.drawText("x", dismiss.x + 7.0f, context.centeredTextBottom("x", dismiss, current.typography.body1.size,
        current.typography.body1.weight), current.typography.body1.size, current.colors.neutralForeground2, current.typography.body1.weight);
    const auto drawButton = [&](const RectF& rect, const std::string& label, bool primary) {
        if (rect.width <= 0.0f) return;
        context.fillRoundRect(rect, current.radius.medium, primary ? current.colors.brandBackground.rest : current.colors.neutralBackground1.hover);
        context.drawText(label, rect.x + (rect.width - textWidth(label, current.typography.body1Strong)) * 0.5f,
            context.centeredTextBottom(label, rect, current.typography.body1Strong.size, current.typography.body1Strong.weight),
            current.typography.body1Strong.size, primary ? current.colors.onBrand : current.colors.neutralForeground1, current.typography.body1Strong.weight);
    };
    drawButton(secondaryBounds(), secondaryLabel_, false);
    drawButton(primaryBounds(), primaryLabel_, true);
}
void TeachingPopover::invoke(ActionHandler& handler) { if (handler) handler(); dismiss(); }
bool TeachingPopover::onPointerEvent(const PointerEvent& event)
{
    if (event.action == PointerAction::Up && event.button == MouseButton::Left) {
        if (primaryBounds().contains(event.position)) { invoke(primaryHandler_); return true; }
        if (secondaryBounds().contains(event.position)) { invoke(secondaryHandler_); return true; }
        if (dismissBounds().contains(event.position)) { dismiss(); return true; }
    }
    return Popover::onPointerEvent(event);
}
bool TeachingPopover::onKeyEvent(const KeyEvent& event)
{
    if (event.action == KeyAction::Down && (event.keyCode == 9 || event.keyCode == 258) &&
        focusPolicy_ == TeachingPopoverFocusPolicy::TrapFocus) {
        // Do not synthesize invisible focus stops: retaining Tab at the dialog
        // boundary is deterministic until one of its exposed actions closes.
        return true;
    }
    return Popover::onKeyEvent(event);
}
void TeachingPopover::dismiss() { if (dismissHandler_) dismissHandler_(); Popover::dismiss(); }

} // namespace wui
