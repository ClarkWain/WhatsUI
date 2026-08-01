#include "wui/feedback.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include "wui/animation.h"
#include "wui/icons.h"
#include "wui/runtime.h"
#include "wui/scheduler.h"
#include "wui/text_metrics.h"
#include "wui/theme.h"

namespace wui {
namespace {

constexpr float kToastWidth = 292.0f;
constexpr float kToastMinHeight = 72.0f;
constexpr float kToastMargin = 16.0f;
constexpr float kToastPadding = 12.0f;
constexpr float kToastIcon = 20.0f;
constexpr float kToastGap = 12.0f;

float measuredWidth(const std::string& text, float size, int weight = 400) noexcept
{
    if (const auto* measurer = textMeasurer()) return measurer->measureText(text, size, weight).width;
    std::size_t codepoints = 0;
    for (const unsigned char c : text) if ((c & 0xc0u) != 0x80u) ++codepoints;
    return static_cast<float>(codepoints) * size * 0.56f;
}

Color intentColor(const Theme& current, ToastIntent intent) noexcept
{
    switch (intent) {
    case ToastIntent::Success: return current.colors.statusSuccess;
    case ToastIntent::Warning: return current.colors.statusWarning;
    case ToastIntent::Error: return current.colors.statusDanger;
    case ToastIntent::Info: default: return current.colors.statusInfo;
    }
}

IconName intentIcon(ToastIntent intent) noexcept
{
    switch (intent) {
    case ToastIntent::Success: return IconName::Checkmark;
    case ToastIntent::Warning: return IconName::Warning;
    case ToastIntent::Error: return IconName::Dismiss;
    case ToastIntent::Info: default: return IconName::Info;
    }
}

float spinnerDiameter(SpinnerSize size) noexcept
{
    switch (size) {
    case SpinnerSize::ExtraTiny: return 16.0f;
    case SpinnerSize::Tiny: return 20.0f;
    case SpinnerSize::ExtraSmall: return 24.0f;
    case SpinnerSize::Small: return 28.0f;
    case SpinnerSize::Medium: return 32.0f;
    case SpinnerSize::Large: return 36.0f;
    case SpinnerSize::ExtraLarge: return 40.0f;
    case SpinnerSize::Huge: return 44.0f;
    }
    return 32.0f;
}

float spinnerStrokeWidth(SpinnerSize size) noexcept
{
    switch (size) {
    case SpinnerSize::Medium:
    case SpinnerSize::Large:
    case SpinnerSize::ExtraLarge:
        return 3.0f;
    case SpinnerSize::Huge:
        return 4.0f;
    default:
        return 2.0f;
    }
}

const TextStyleToken& spinnerLabelStyle(const Theme& current,
                                        SpinnerSize size) noexcept
{
    switch (size) {
    case SpinnerSize::Medium:
    case SpinnerSize::Large:
    case SpinnerSize::ExtraLarge:
        return current.typography.subtitle2;
    case SpinnerSize::Huge:
        return current.typography.subtitle1;
    default:
        return current.typography.body1;
    }
}

} // namespace

ToastNode::ToastNode(std::string title, std::string body)
    : title_(std::move(title)), body_(std::move(body)) {}

ToastNode::~ToastNode() { stopTimeoutTicker(); }
ToastNode& ToastNode::title(std::string value) { setTitle(std::move(value)); return *this; }
void ToastNode::setTitle(std::string value) { title_ = std::move(value); markDirty(DirtyFlag::Layout); }
const std::string& ToastNode::title() const noexcept { return title_; }
ToastNode& ToastNode::body(std::string value) { setBody(std::move(value)); return *this; }
void ToastNode::setBody(std::string value) { body_ = std::move(value); markDirty(DirtyFlag::Layout); }
const std::string& ToastNode::body() const noexcept { return body_; }
ToastNode& ToastNode::intent(ToastIntent value) noexcept { setIntent(value); return *this; }
void ToastNode::setIntent(ToastIntent value) noexcept { intent_ = value; markDirty(DirtyFlag::Paint); }
ToastIntent ToastNode::intent() const noexcept { return intent_; }
ToastNode& ToastNode::position(ToastPosition value) noexcept { setPosition(value); return *this; }
ToastPosition ToastNode::position() const noexcept { return position_; }
ToastNode& ToastNode::action(std::string label, Handler handler) { setAction(std::move(label), std::move(handler)); return *this; }
void ToastNode::setAction(std::string label, Handler handler) { actionLabel_ = std::move(label); onAction_ = std::move(handler); markDirty(DirtyFlag::Layout); }
const std::string& ToastNode::actionLabel() const noexcept { return actionLabel_; }
ToastNode& ToastNode::onDismiss(Handler handler) { onDismiss_ = std::move(handler); return *this; }
ToastNode& ToastNode::timeout(std::chrono::milliseconds value) noexcept { setTimeout(value); return *this; }
void ToastNode::setTimeout(std::chrono::milliseconds value) noexcept
{
    timeout_ = std::max(value, std::chrono::milliseconds{0}); elapsed_ = std::chrono::milliseconds{0};
    if (timeout_.count() == 0) stopTimeoutTicker(); else startTimeoutTicker();
}
std::chrono::milliseconds ToastNode::timeout() const noexcept { return timeout_; }
bool ToastNode::isPaused() const noexcept { return paused_; }
void ToastNode::setPaused(bool value) noexcept { paused_ = value; lastTick_ = std::chrono::steady_clock::now(); }
void ToastNode::advanceTimeout(std::chrono::milliseconds elapsed)
{
    if (dismissed_ || paused_ || timeout_.count() <= 0 || elapsed.count() <= 0) return;
    elapsed_ += elapsed;
    if (elapsed_ >= timeout_) dismiss();
}
void ToastNode::dismiss()
{
    if (dismissed_) return;
    dismissed_ = true;
    stopTimeoutTicker();
    if (onDismiss_) onDismiss_();
    if (hostDismiss_) hostDismiss_();
}

SizeF ToastNode::measure(const Constraints& constraints) const
{
    const auto& current = theme();
    const float maxText = std::max(80.0f, std::min(kToastWidth, constraints.maxWidth) - kToastPadding * 2.0f - kToastIcon - kToastGap - 32.0f);
    const float bodyLines = body_.empty() ? 0.0f : std::max(1.0f, std::ceil(measuredWidth(body_, current.typography.body1.size) / maxText));
    const float titleHeight = title_.empty() ? 0.0f : current.typography.body1Strong.lineHeight;
    const float bodyHeight = bodyLines * current.typography.caption1.lineHeight;
    const float actionHeight = actionLabel_.empty() ? 0.0f : current.controls.height;
    const float height = std::max(kToastMinHeight, kToastPadding * 2.0f + titleHeight + bodyHeight + actionHeight + (actionHeight > 0.0f && (titleHeight + bodyHeight) > 0.0f ? 4.0f : 0.0f));
    return constraints.clamp({std::min(kToastWidth, constraints.maxWidth), height});
}

void ToastNode::layout(const RectF& hostBounds)
{
    const SizeF size = measure({0.0f, hostBounds.width, 0.0f, hostBounds.height});
    const bool end = position_ == ToastPosition::TopEnd || position_ == ToastPosition::BottomEnd;
    const bool bottom = position_ == ToastPosition::BottomStart || position_ == ToastPosition::BottomEnd;
    const float x = end ? hostBounds.x + std::max(kToastMargin, hostBounds.width - size.width - kToastMargin)
                        : hostBounds.x + kToastMargin;
    const float y = bottom ? hostBounds.y + std::max(kToastMargin, hostBounds.height - size.height - kToastMargin)
                           : hostBounds.y + kToastMargin;
    Node::layout({x, y, size.width, size.height});
    clearLayoutDirtyRecursively();
}

RectF ToastNode::actionBounds() const noexcept
{
    if (actionLabel_.empty()) return {};
    const auto& current = theme();
    const float width = std::max(56.0f, measuredWidth(actionLabel_, current.typography.caption1Strong.size, current.typography.caption1Strong.weight) + 16.0f);
    return {bounds().x + bounds().width - kToastPadding - 28.0f - width,
            bounds().y + bounds().height - kToastPadding - current.controls.height,
            width, current.controls.height};
}
RectF ToastNode::dismissBounds() const noexcept
{
    return {bounds().x + bounds().width - kToastPadding - 24.0f, bounds().y + kToastPadding - 2.0f, 24.0f, 24.0f};
}

void ToastNode::paint(PaintContext& context)
{
    const auto& current = theme();
    const auto box = bounds();
    const auto& elevation = current.elevation.shadow8;
    context.drawBoxShadow(box, current.radius.medium, elevation.ambient.blur, elevation.ambient.offsetX,
                          elevation.ambient.offsetY, elevation.ambient.spread, elevation.ambient.color);
    context.drawBoxShadow(box, current.radius.medium, elevation.key.blur, elevation.key.offsetX,
                          elevation.key.offsetY, elevation.key.spread, elevation.key.color);
    context.fillStrokeRoundRect(context.snapRectEdges(box),
                                current.radius.medium,
                                context.snapStrokeWidth(current.stroke.thin),
                                current.colors.surfaceRaised,
                                Color{0, 0, 0, 0});

    const RectF icon{box.x + kToastPadding, box.y + kToastPadding, kToastIcon, kToastIcon};
    const Color color = intentColor(current, intent_);
    context.fillRoundRect(icon, current.radius.circular, color);
    drawIcon(context, intentIcon(intent_), icon, current.colors.onBrand,
             IconSize::Size16);
    const float textX = icon.x + icon.width + kToastGap;
    const float textWidth = std::max(0.0f, dismissBounds().x - 8.0f - textX);
    float y = box.y + kToastPadding;
    if (!title_.empty()) {
        context.drawText(title_, textX, y + current.typography.body1Strong.lineHeight, current.typography.body1Strong.size,
                         current.colors.neutralForeground1, current.typography.body1Strong.weight);
        y += current.typography.body1Strong.lineHeight;
    }
    if (!body_.empty()) {
        // Canvas text wrapping lives in TextNode; a toast intentionally clips to
        // two visual lines rather than stretching an overlay beyond the host.
        const std::size_t fitting = textWidth > 0.0f ? static_cast<std::size_t>(textWidth / std::max(1.0f, current.typography.caption1.size * 0.54f)) : body_.size();
        std::string visible = body_;
        if (fitting > 3 && visible.size() > fitting * 2) visible = visible.substr(0, fitting * 2 - 3) + "...";
        context.drawText(visible, textX, y + current.typography.caption1.lineHeight, current.typography.caption1.size,
                         current.colors.neutralForeground2, current.typography.caption1.weight);
    }
    const auto dismissBox = dismissBounds();
    drawIcon(context, IconName::Dismiss, dismissBox,
             current.colors.neutralForeground2, IconSize::Size16);
    const auto action = actionBounds();
    if (!actionLabel_.empty()) {
        context.drawText(actionLabel_, action.x + (action.width - measuredWidth(actionLabel_, current.typography.caption1Strong.size, current.typography.caption1Strong.weight)) * 0.5f,
                         context.centeredTextBottom(actionLabel_, action, current.typography.caption1Strong.size, current.typography.caption1Strong.weight),
                         current.typography.caption1Strong.size, current.colors.brandForeground1, current.typography.caption1Strong.weight);
    }
    clearDirty(DirtyFlag::Paint);
}

Node* ToastNode::hitTest(PointF point) { return bounds().contains(point) ? this : nullptr; }
bool ToastNode::onPointerEvent(const PointerEvent& event)
{
    if (event.action == PointerAction::Enter) { setPaused(true); return true; }
    if (event.action == PointerAction::Leave || event.action == PointerAction::Cancel) { setPaused(false); return true; }
    if (!bounds().contains(event.position)) return false;
    if (event.action == PointerAction::Down && event.button == MouseButton::Left) { setPaused(true); return true; }
    if (event.action == PointerAction::Up && event.button == MouseButton::Left) {
        if (dismissBounds().contains(event.position)) { dismiss(); return true; }
        if (!actionLabel_.empty() && actionBounds().contains(event.position)) { if (onAction_) onAction_(); dismiss(); return true; }
        setPaused(false); return true;
    }
    return true;
}
bool ToastNode::onKeyEvent(const KeyEvent& event)
{
    if (event.action == KeyAction::Down && (event.keyCode == 27 || event.keyCode == 256)) { dismiss(); return true; }
    return false;
}
AccessibilityActionCapabilities ToastNode::accessibilityActions() const noexcept
{
    AccessibilityActionCapabilities actions; actions.invoke = !actionLabel_.empty(); return actions;
}
AccessibilityActionStatus ToastNode::performAccessibilityAction(AccessibilityActionKind kind, std::string_view)
{
    if (kind != AccessibilityActionKind::Invoke || actionLabel_.empty()) return AccessibilityActionStatus::NotSupported;
    if (onAction_) onAction_(); dismiss(); return AccessibilityActionStatus::Succeeded;
}
void ToastNode::setPosition(ToastPosition value) noexcept { position_ = value; markDirty(DirtyFlag::Layout); }
void ToastNode::setHostDismiss(Handler handler) { hostDismiss_ = std::move(handler); }
void ToastNode::onAttach() noexcept { startTimeoutTicker(); }
void ToastNode::onDetach() noexcept { stopTimeoutTicker(); }
void ToastNode::startTimeoutTicker() noexcept
{
    if (!isAttached() || timeout_.count() <= 0 || tickerId_.has_value() || dismissed_) return;
    lastTick_ = std::chrono::steady_clock::now();
    tickerId_ = Ticker::instance().add(Animation(0.05f, [this](float) {
        const auto now = std::chrono::steady_clock::now();
        const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick_);
        lastTick_ = now;
        advanceTimeout(delta);
    }).repeat(-1));
}
void ToastNode::stopTimeoutTicker() noexcept { if (tickerId_) { Ticker::instance().cancel(*tickerId_); tickerId_.reset(); } }

Toaster::Toaster(OverlayHost& host, ToastPosition position) noexcept : host_(&host), position_(position) {}
Toaster::~Toaster() { *alive_ = false; clear(); }
void Toaster::show(std::unique_ptr<ToastNode> toast)
{
    if (!toast) return;
    queue_.push_back(std::move(toast));
    showNext();
}
void Toaster::showNext()
{
    if (!host_ || activeId_ || queue_.empty()) return;
    auto toast = std::move(queue_.front()); queue_.erase(queue_.begin());
    toast->setPosition(position_);
    active_ = toast.get();
    toast->setHostDismiss([this] { dismissActiveSafely(); });
    activeId_ = host_->show(std::move(toast));
}
void Toaster::dismissActiveSafely()
{
    const std::weak_ptr<bool> alive = alive_;
    (void)scheduleStructuralUpdate(this, [this, alive] {
        const auto guard = alive.lock();
        if (!guard || !*guard) return;
        if (host_ && activeId_) (void)host_->dismiss(*activeId_);
        activeId_.reset(); active_ = nullptr; showNext();
    });
}
void Toaster::dismiss() { if (active_) active_->dismiss(); }
void Toaster::clear()
{
    queue_.clear();
    if (host_ && activeId_) (void)host_->dismiss(*activeId_);
    activeId_.reset(); active_ = nullptr;
}
bool Toaster::hasActiveToast() const noexcept { return active_ != nullptr; }
std::size_t Toaster::queuedCount() const noexcept { return queue_.size(); }
ToastNode* Toaster::activeToast() const noexcept { return active_; }
void Toaster::setPosition(ToastPosition value) noexcept { position_ = value; if (active_) active_->setPosition(value); }
ToastPosition Toaster::position() const noexcept { return position_; }

SpinnerNode::SpinnerNode(std::string label) : label_(std::move(label)) {}
SpinnerNode::~SpinnerNode() { stopTicker(); }
SpinnerNode& SpinnerNode::label(std::string value) { setLabel(std::move(value)); return *this; }
void SpinnerNode::setLabel(std::string value) { label_ = std::move(value); markDirty(DirtyFlag::Layout); }
const std::string& SpinnerNode::label() const noexcept { return label_; }
SpinnerNode& SpinnerNode::size(SpinnerSize value) noexcept { setSize(value); return *this; }
void SpinnerNode::setSize(SpinnerSize value) noexcept { size_ = value; markDirty(DirtyFlag::Layout); }
SpinnerSize SpinnerNode::size() const noexcept { return size_; }
SpinnerNode& SpinnerNode::labelPosition(SpinnerLabelPosition value) noexcept { setLabelPosition(value); return *this; }
void SpinnerNode::setLabelPosition(SpinnerLabelPosition value) noexcept { labelPosition_ = value; markDirty(DirtyFlag::Layout); }
SpinnerLabelPosition SpinnerNode::labelPosition() const noexcept { return labelPosition_; }
SpinnerNode& SpinnerNode::motionEnabled(bool value) noexcept { setMotionEnabled(value); return *this; }
void SpinnerNode::setMotionEnabled(bool value) noexcept { motionEnabled_ = value; if (value) startTicker(); else stopTicker(); markDirty(DirtyFlag::Paint); }
bool SpinnerNode::isMotionEnabled() const noexcept { return motionEnabled_; }
float SpinnerNode::indicatorSize() const noexcept { return spinnerDiameter(size_); }
SizeF SpinnerNode::measure(const Constraints& constraints) const
{
    const auto& current = theme(); const float indicator = indicatorSize();
    if (label_.empty()) return constraints.clamp({indicator, indicator});
    const auto& labelStyle = spinnerLabelStyle(current, size_);
    const float text =
        measuredWidth(label_, labelStyle.size, labelStyle.weight);
    if (labelPosition_ == SpinnerLabelPosition::Above || labelPosition_ == SpinnerLabelPosition::Below)
        return constraints.clamp(
            {std::max(indicator, text),
             indicator + current.spacing.vertical.s +
                 labelStyle.lineHeight});
    return constraints.clamp(
        {indicator + current.spacing.horizontal.s + text,
         std::max(indicator, labelStyle.lineHeight)});
}
void SpinnerNode::paint(PaintContext& context)
{
    const auto& current = theme();
    const float diameter = indicatorSize();
    const float gap = current.spacing.horizontal.s;
    const auto& labelStyle = spinnerLabelStyle(current, size_);
    const bool vertical =
        labelPosition_ == SpinnerLabelPosition::Above ||
        labelPosition_ == SpinnerLabelPosition::Below;
    const float labelWidth =
        measuredWidth(label_, labelStyle.size, labelStyle.weight);
    const float contentWidth =
        label_.empty()
            ? diameter
            : vertical ? std::max(diameter, labelWidth)
                       : diameter + gap + labelWidth;
    const float contentHeight =
        label_.empty()
            ? diameter
            : vertical ? diameter + current.spacing.vertical.s +
                             labelStyle.lineHeight
                       : std::max(diameter, labelStyle.lineHeight);
    const float startX =
        bounds().x + std::max(0.0f, (bounds().width - contentWidth) * 0.5f);
    const float startY =
        bounds().y +
        std::max(0.0f, (bounds().height - contentHeight) * 0.5f);
    float indicatorX =
        vertical ? bounds().x + (bounds().width - diameter) * 0.5f
                 : startX;
    float indicatorY =
        vertical
            ? startY +
                  (labelPosition_ == SpinnerLabelPosition::Above
                       ? labelStyle.lineHeight +
                             current.spacing.vertical.s
                       : 0.0f)
            : bounds().y + (bounds().height - diameter) * 0.5f;
    if (!vertical &&
        labelPosition_ == SpinnerLabelPosition::Before) {
        indicatorX = startX + labelWidth + gap;
    }

    const RectF indicatorRect = context.snapRectEdges(
        {indicatorX, indicatorY, diameter, diameter});
    const float stroke = context.snapStrokeWidth(spinnerStrokeWidth(size_));
    const float arcInset = stroke * 0.5f;
    const RectF arcBounds{
        indicatorRect.x + arcInset, indicatorRect.y + arcInset,
        std::max(0.0f, indicatorRect.width - stroke),
        std::max(0.0f, indicatorRect.height - stroke)};
    constexpr float twoPi = 6.28318530718f;
    const Color base{current.colors.brandForeground1.r,
                     current.colors.brandForeground1.g,
                     current.colors.brandForeground1.b, 56};
    context.strokeArc(arcBounds, -1.57079632679f,
                      twoPi - 0.001f, stroke, base);
    context.strokeArc(
        arcBounds, -1.57079632679f + phase_ * twoPi,
        twoPi * 0.67f, stroke, current.colors.brandForeground1);

    if (!label_.empty()) {
        RectF labelBox{};
        if (vertical) {
            labelBox = {
                bounds().x +
                    std::max(0.0f,
                             (bounds().width - labelWidth) * 0.5f),
                labelPosition_ == SpinnerLabelPosition::Above
                    ? startY
                    : indicatorY + diameter +
                          current.spacing.vertical.s,
                labelWidth, labelStyle.lineHeight};
        } else if (labelPosition_ ==
                   SpinnerLabelPosition::Before) {
            labelBox = {startX, startY, labelWidth,
                        contentHeight};
        } else {
            labelBox = {indicatorX + diameter + gap, startY,
                        labelWidth, contentHeight};
        }
        context.drawText(
            label_, labelBox.x,
            context.centeredTextBottom(label_, labelBox,
                                       labelStyle.size,
                                       labelStyle.weight,
                                       labelStyle.family),
            labelStyle.size, current.colors.neutralForeground1,
            labelStyle.weight, labelStyle.family);
    }
    clearDirty(DirtyFlag::Paint);
}
AccessibilityActionCapabilities SpinnerNode::accessibilityActions() const noexcept { AccessibilityActionCapabilities result; result.valueReadOnly = true; return result; }
void SpinnerNode::onAttach() noexcept { startTicker(); }
void SpinnerNode::onDetach() noexcept { stopTicker(); }
void SpinnerNode::startTicker() noexcept
{
    if (!isAttached() || !motionEnabled_ || tickerId_) return;
    tickerId_ = Ticker::instance().add(Animation(1.5f, [this](float progress) { phase_ = progress; markDirty(DirtyFlag::Paint); }).repeat(-1));
}
void SpinnerNode::stopTicker() noexcept { if (tickerId_) { Ticker::instance().cancel(*tickerId_); tickerId_.reset(); } }

} // namespace wui
