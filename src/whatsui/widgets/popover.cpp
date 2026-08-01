#include "wui/popover.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "wui/icons.h"
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

PopoverNode::PopoverNode(std::string title, std::string body)
    : title_(std::move(title)), body_(std::move(body))
{
}

PopoverNode& PopoverNode::title(std::string value) { title_ = std::move(value); markDirty(DirtyFlag::Layout); return *this; }
PopoverNode& PopoverNode::body(std::string value) { body_ = std::move(value); markDirty(DirtyFlag::Layout); return *this; }
PopoverNode& PopoverNode::appearance(PopoverAppearance value) noexcept { appearance_ = value; markDirty(DirtyFlag::Paint); return *this; }
PopoverNode& PopoverNode::showArrow(bool value) noexcept { arrow_ = value; markDirty(DirtyFlag::Paint); return *this; }
PopoverNode& PopoverNode::accessibleLabel(std::string value) { accessibleLabel_ = std::move(value); return *this; }
const std::string& PopoverNode::title() const noexcept { return title_; }
const std::string& PopoverNode::body() const noexcept { return body_; }
PopoverAppearance PopoverNode::appearance() const noexcept { return appearance_; }
bool PopoverNode::hasArrow() const noexcept { return arrow_; }
const std::string& PopoverNode::accessibleLabel() const noexcept { return accessibleLabel_; }
RectF PopoverNode::contentBounds() const noexcept { return contentBounds_; }

SizeF PopoverNode::measure(const Constraints& constraints) const
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

void PopoverNode::layout(const RectF& bounds)
{
    PopupNode::layout(bounds);
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

Color PopoverNode::backgroundColor() const noexcept
{
    const auto& colors = theme().colors;
    switch (appearance_) {
    case PopoverAppearance::Inverted: return Color{41, 41, 41, 255};
    case PopoverAppearance::Brand: return colors.brandBackground.rest;
    case PopoverAppearance::Surface: default: return colors.neutralBackground1.rest;
    }
}

Color PopoverNode::foregroundColor() const noexcept
{
    return appearance_ == PopoverAppearance::Surface ? theme().colors.neutralForeground1 : theme().colors.onBrand;
}

void PopoverNode::paintArrow(PaintContext& context, const RectF& panel, Color color) const
{
    if (!arrow_) return;
    const float center = context.snapToPhysicalPixel(std::clamp(
        anchor().x + anchor().width * 0.5f, panel.x + 16.0f,
        panel.x + panel.width - 16.0f));
    const bool above = isAbove(placement());
    // The arrow is painted over the panel, then its base seam is erased with
    // the panel fill.  This keeps the sloped sides visually connected to the
    // surface rather than leaving a standalone outlined triangle perched on
    // top of a horizontal border.
    const float pixel = context.physicalPixel();
    const float y = context.snapToPhysicalPixel(
        above ? panel.y + panel.height : panel.y);
    const std::vector<PointF> outer = above
        ? std::vector<PointF>{{center - kArrowSize, y - pixel}, {center + kArrowSize, y - pixel}, {center, y + kArrowSize}}
        : std::vector<PointF>{{center - kArrowSize, y + pixel}, {center + kArrowSize, y + pixel}, {center, y - kArrowSize}};
    const float inset = pixel;
    const std::vector<PointF> inner = above
        ? std::vector<PointF>{{center - kArrowSize + inset, y - pixel}, {center + kArrowSize - inset, y - pixel}, {center, y + kArrowSize - inset}}
        : std::vector<PointF>{{center - kArrowSize + inset, y + pixel}, {center + kArrowSize - inset, y + pixel}, {center, y - kArrowSize + inset}};
    // The official surface reserves a transparent 1-DIP border.  A faint
    // Shadow16 key edge is used only around the pointer because the current
    // backend cannot include arbitrary polygons in drawBoxShadow().
    context.fillPolygon(outer, theme().elevation.shadow16.key.color);
    context.fillPolygon(inner, color);
    // Remove the horizontal panel border only at the arrow base.  The stroke
    // on the two sloped arrow edges remains, producing a continuous callout.
    context.fillRect(
        above ? RectF{center - kArrowSize, y - pixel,
                      kArrowSize * 2.0f, pixel}
              : RectF{center - kArrowSize, y,
                      kArrowSize * 2.0f, pixel},
        color);
}

void PopoverNode::paint(PaintContext& context)
{
    const RectF panel = context.snapRectEdges(panelBounds());
    const auto& current = theme();
    const Color bg = backgroundColor();
    paintElevation(context, panel, current.radius.medium, current);
    // Fluent PopoverSurface uses borderRadiusMedium (4 DIP) and a transparent
    // border.  A neutral outline here doubled the edge and made the optional
    // pointer look detached from the surface at fractional DPI.
    context.fillRoundRect(panel, current.radius.medium, bg);
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

float PopoverNode::headerHeight() const noexcept { return 0.0f; }
float PopoverNode::footerHeight() const noexcept { return 0.0f; }
float PopoverNode::bodyBottom() const noexcept { return contentBounds_.y + contentBounds_.height; }

PopoverButtonNode::PopoverButtonNode(std::string label) : ButtonNode(std::move(label))
{
    ButtonNode::onClick([this] { if (open_) closePopover(); else openPopover(); });
}
PopoverButtonNode& PopoverButtonNode::bindOverlayHost(OverlayHost& host) noexcept { overlayHost_ = &host; return *this; }
PopoverButtonNode& PopoverButtonNode::popoverFactory(PopoverFactory factory) { factory_ = std::move(factory); return *this; }
PopoverButtonNode& PopoverButtonNode::popover(std::string title, std::string body)
{
    factory_ = [title = std::move(title), body = std::move(body)] { return std::make_unique<PopoverNode>(title, body); };
    return *this;
}
bool PopoverButtonNode::isOpen() const noexcept { return open_; }
AccessibilityActionCapabilities PopoverButtonNode::accessibilityActions() const noexcept
{
    auto actions = ButtonNode::accessibilityActions();
    actions.invoke = false;
    actions.expandCollapse = true;
    return actions;
}
AccessibilityActionStatus PopoverButtonNode::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value)
{
    (void)value;
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    if (kind == AccessibilityActionKind::Expand) { openPopover(); return open_ ? AccessibilityActionStatus::Succeeded : AccessibilityActionStatus::NotSupported; }
    if (kind == AccessibilityActionKind::Collapse) { closePopover(); return AccessibilityActionStatus::Succeeded; }
    return AccessibilityActionStatus::NotSupported;
}
void PopoverButtonNode::openPopover()
{
    if (open_ || overlayHost_ == nullptr || !factory_) return;
    auto surface = factory_();
    if (!surface) return;
    PopoverNode* raw = surface.get();
    surface->anchor(bounds()).placement(PopupPlacement::BelowStart).onDismiss([this] { closePopover(); });
    open_ = true;
    setVisualState(ControlVisualState::Pressed, true);
    overlayId_ = overlayHost_->show(std::move(surface));
    // A regular PopoverNode is non-modal supplemental content and must not steal
    // keyboard focus. A TeachingPopoverNode defaults to a guided dialog boundary;
    // applications may make it explicitly non-modal when appropriate.
    const auto* teaching = dynamic_cast<const TeachingPopoverNode*>(raw);
    if (teaching != nullptr && teaching->focusPolicy() == TeachingPopoverFocusPolicy::TrapFocus) {
        overlayHost_->focus(raw);
    }
    markDirty(DirtyFlag::Paint);
}
void PopoverButtonNode::closePopover()
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

TeachingPopoverNode::TeachingPopoverNode(std::string title, std::string body)
    : PopoverNode(std::move(title), std::move(body))
{
    // Unlike the general PopoverNode (whose pointer is opt-in), Fluent's guided
    // teaching surface is anchored by default.
    showArrow(true);
}
TeachingPopoverNode& TeachingPopoverNode::primaryAction(std::string label, ActionHandler handler) { primaryLabel_ = std::move(label); primaryHandler_ = std::move(handler); markDirty(DirtyFlag::Layout); return *this; }
TeachingPopoverNode& TeachingPopoverNode::secondaryAction(std::string label, ActionHandler handler) { secondaryLabel_ = std::move(label); secondaryHandler_ = std::move(handler); markDirty(DirtyFlag::Layout); return *this; }
TeachingPopoverNode& TeachingPopoverNode::dismissLabel(std::string label) { dismissLabel_ = std::move(label); markDirty(DirtyFlag::Layout); return *this; }
TeachingPopoverNode& TeachingPopoverNode::stepText(std::string value) { stepText_ = std::move(value); markDirty(DirtyFlag::Layout); return *this; }
TeachingPopoverNode& TeachingPopoverNode::focusPolicy(TeachingPopoverFocusPolicy value) noexcept { focusPolicy_ = value; return *this; }
TeachingPopoverNode& TeachingPopoverNode::onDismiss(DismissHandler handler) { dismissHandler_ = std::move(handler); return *this; }
const std::string& TeachingPopoverNode::primaryActionLabel() const noexcept { return primaryLabel_; }
const std::string& TeachingPopoverNode::secondaryActionLabel() const noexcept { return secondaryLabel_; }
const std::string& TeachingPopoverNode::dismissLabel() const noexcept { return dismissLabel_; }
const std::string& TeachingPopoverNode::stepText() const noexcept { return stepText_; }
TeachingPopoverFocusPolicy TeachingPopoverNode::focusPolicy() const noexcept { return focusPolicy_; }
float TeachingPopoverNode::headerHeight() const noexcept { return stepText_.empty() ? 0.0f : theme().typography.caption1.lineHeight + theme().spacing.vertical.xs; }
float TeachingPopoverNode::footerHeight() const noexcept { return primaryLabel_.empty() && secondaryLabel_.empty() ? 0.0f : theme().controls.height + theme().spacing.vertical.m + theme().spacing.vertical.l; }
SizeF TeachingPopoverNode::measure(const Constraints& constraints) const
{
    SizeF result = PopoverNode::measure(constraints);
    // The Fluent teaching surface is a 288-DIP content column with 16-DIP
    // padding on each side.
    result.width = std::max(result.width, std::min(320.0f, constraints.maxWidth));
    return constraints.clamp(result);
}
void TeachingPopoverNode::layout(const RectF& bounds) { PopoverNode::layout(bounds); }
RectF TeachingPopoverNode::primaryBounds() const noexcept
{
    if (primaryLabel_.empty()) return {};
    const auto panel = panelBounds();
    const float width = std::max(
        96.0f,
        textWidth(primaryLabel_, theme().typography.body1Strong) + 24.0f);
    float right = panel.x + panel.width - kPadding;
    if (!secondaryLabel_.empty()) {
        const float secondaryWidth = std::max(
            96.0f,
            textWidth(secondaryLabel_, theme().typography.body1Strong) +
                24.0f);
        right -= secondaryWidth + theme().spacing.horizontal.s;
    }
    return {right - width,
            panel.y + panel.height - kPadding - theme().controls.height,
            width, theme().controls.height};
}
RectF TeachingPopoverNode::secondaryBounds() const noexcept
{
    if (secondaryLabel_.empty()) return {};
    const auto panel = panelBounds();
    const float width = std::max(
        96.0f,
        textWidth(secondaryLabel_, theme().typography.body1Strong) + 24.0f);
    return {panel.x + panel.width - kPadding - width,
            panel.y + panel.height - kPadding - theme().controls.height,
            width, theme().controls.height};
}
RectF TeachingPopoverNode::dismissBounds() const noexcept { const auto panel = panelBounds(); return {panel.x + panel.width - kPadding - 24.0f, panel.y + kPadding - 2.0f, 24.0f, 24.0f}; }
void TeachingPopoverNode::paint(PaintContext& context)
{
    PopoverNode::paint(context);
    const auto& current = theme(); const auto panel = panelBounds();
    if (!stepText_.empty()) context.drawText(stepText_, panel.x + kPadding, panel.y + kPadding * 0.5f + current.typography.caption1.lineHeight,
        current.typography.caption1.size, current.colors.brandForeground1, current.typography.caption1.weight);
    const auto dismiss = dismissBounds();
    drawIcon(context, IconName::Dismiss, dismiss,
             current.colors.neutralForeground2, IconSize::Size20);
    const auto drawButton = [&](const RectF& rect, const std::string& label, bool primary) {
        if (rect.width <= 0.0f) return;
        const RectF aligned = context.snapRectEdges(rect);
        context.fillStrokeRoundRect(
            aligned, current.radius.medium,
            context.snapStrokeWidth(current.stroke.thin),
            primary ? current.colors.brandBackground.rest
                    : current.colors.neutralBackground1.rest,
            primary ? current.colors.brandBackground.rest
                    : current.colors.neutralStroke1);
        context.drawText(label, aligned.x + (aligned.width - textWidth(label, current.typography.body1Strong)) * 0.5f,
            context.centeredTextBottom(label, aligned, current.typography.body1Strong.size, current.typography.body1Strong.weight),
            current.typography.body1Strong.size, primary ? current.colors.onBrand : current.colors.neutralForeground1, current.typography.body1Strong.weight);
    };
    drawButton(secondaryBounds(), secondaryLabel_, false);
    drawButton(primaryBounds(), primaryLabel_, true);
}
void TeachingPopoverNode::invoke(ActionHandler& handler) { if (handler) handler(); dismiss(); }
bool TeachingPopoverNode::onPointerEvent(const PointerEvent& event)
{
    if (event.action == PointerAction::Up && event.button == MouseButton::Left) {
        if (primaryBounds().contains(event.position)) { invoke(primaryHandler_); return true; }
        if (secondaryBounds().contains(event.position)) { invoke(secondaryHandler_); return true; }
        if (dismissBounds().contains(event.position)) { dismiss(); return true; }
    }
    return PopoverNode::onPointerEvent(event);
}
bool TeachingPopoverNode::onKeyEvent(const KeyEvent& event)
{
    if (event.action == KeyAction::Down && (event.keyCode == 9 || event.keyCode == 258) &&
        focusPolicy_ == TeachingPopoverFocusPolicy::TrapFocus) {
        // Do not synthesize invisible focus stops: retaining Tab at the dialog
        // boundary is deterministic until one of its exposed actions closes.
        return true;
    }
    return PopoverNode::onKeyEvent(event);
}
void TeachingPopoverNode::dismiss() { if (dismissHandler_) dismissHandler_(); PopoverNode::dismiss(); }

} // namespace wui
