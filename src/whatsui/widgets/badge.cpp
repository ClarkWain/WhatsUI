#include "wui/badge.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "wui/theme.h"

namespace wui {
namespace {

struct BadgeMetrics { float fontSize; float lineHeight; float padX; float height; };

BadgeMetrics metrics(BadgeSize size) noexcept
{
    switch (size) {
    case BadgeSize::Small: return {10.0f, 14.0f, 4.0f, 16.0f};
    case BadgeSize::Medium: return {12.0f, 16.0f, 6.0f, 20.0f};
    case BadgeSize::Large: return {14.0f, 20.0f, 8.0f, 24.0f};
    case BadgeSize::ExtraLarge: return {16.0f, 22.0f, 10.0f, 28.0f};
    }
    return {12.0f, 16.0f, 6.0f, 20.0f};
}

Color semanticColor(const Theme& current, BadgeColor color) noexcept
{
    switch (color) {
    case BadgeColor::Brand: return current.colors.brandForeground1;
    case BadgeColor::Danger: return current.colors.danger;
    case BadgeColor::Important: return {176, 0, 110, 255};
    case BadgeColor::Informative: return current.colors.statusInfo;
    case BadgeColor::Success: return current.colors.statusSuccess;
    case BadgeColor::Warning: return current.colors.statusWarning;
    case BadgeColor::Neutral: return current.colors.neutralForeground2;
    }
    return current.colors.neutralForeground2;
}

Color tint(Color color) noexcept { return Color{color.r, color.g, color.b, 28}; }
float estimateTextWidth(const std::string& text, float size) noexcept
{
    // Text layout is a rendering concern; this estimate gives badges a stable
    // intrinsic size even for headless composition. Paint uses actual metrics
    // for vertical placement, as all other Fluent controls do.
    return std::max(0.0f, static_cast<float>(text.size()) * size * 0.56f);
}

float badgeRadius(BadgeShape shape, float height) noexcept
{
    switch (shape) {
    case BadgeShape::Circular: return theme().radius.circular;
    case BadgeShape::Square: return theme().radius.none;
    case BadgeShape::Rounded: return theme().radius.medium;
    }
    return theme().radius.medium;
}

void palette(const Theme& current, BadgeAppearance appearance, BadgeColor color,
             Color& background, Color& foreground, Color& border) noexcept
{
    const Color semantic = semanticColor(current, color);
    foreground = semantic;
    border = Color{0, 0, 0, 0};
    switch (appearance) {
    case BadgeAppearance::Filled:
        background = semantic;
        foreground = current.colors.onBrand;
        break;
    case BadgeAppearance::Ghost:
        background = Color{0, 0, 0, 0};
        break;
    case BadgeAppearance::Outline:
        background = Color{0, 0, 0, 0};
        border = semantic;
        break;
    case BadgeAppearance::Tint:
        background = tint(semantic);
        break;
    }
    if (color == BadgeColor::Neutral) {
        if (appearance == BadgeAppearance::Filled) {
            background = current.colors.neutralForeground2;
            foreground = current.colors.onBrand;
        } else {
            foreground = current.colors.neutralForeground2;
        }
    }
}

Color presenceColor(const Theme& current, PresenceStatus status) noexcept
{
    switch (status) {
    case PresenceStatus::Available: return current.colors.statusSuccess;
    case PresenceStatus::Away: return {255, 185, 0, 255};
    case PresenceStatus::Busy: return current.colors.statusDanger;
    case PresenceStatus::DoNotDisturb: return current.colors.statusDanger;
    case PresenceStatus::Offline: return current.colors.neutralForegroundDisabled;
    case PresenceStatus::OutOfOffice: return {136, 23, 152, 255};
    case PresenceStatus::Unknown: return current.colors.neutralForeground3;
    }
    return current.colors.neutralForeground3;
}

const char* presenceText(PresenceStatus status) noexcept
{
    switch (status) {
    case PresenceStatus::Available: return "Available";
    case PresenceStatus::Away: return "Away";
    case PresenceStatus::Busy: return "Busy";
    case PresenceStatus::DoNotDisturb: return "Do not disturb";
    case PresenceStatus::Offline: return "Offline";
    case PresenceStatus::OutOfOffice: return "Out of office";
    case PresenceStatus::Unknown: return "Presence unknown";
    }
    return "Presence unknown";
}

} // namespace

Badge::Badge(std::string text) : text_(std::move(text)) {}
const std::string& Badge::text() const noexcept { return text_; }
Badge& Badge::text(std::string value) { setText(std::move(value)); return *this; }
void Badge::setText(std::string value) { if (text_ != value) { text_ = std::move(value); markDirty(DirtyFlag::Layout); } }
const std::string& Badge::accessibleLabel() const noexcept { return accessibleLabel_; }
Badge& Badge::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void Badge::setAccessibleLabel(std::string value) { accessibleLabel_ = std::move(value); markDirty(DirtyFlag::Style); }
std::string Badge::generatedAccessibleLabel() const { return accessibleLabel_.empty() ? text_ : accessibleLabel_; }
BadgeAppearance Badge::appearance() const noexcept { return appearance_; }
Badge& Badge::appearance(BadgeAppearance value) noexcept { setAppearance(value); return *this; }
void Badge::setAppearance(BadgeAppearance value) noexcept { if (appearance_ != value) { appearance_ = value; markDirty(DirtyFlag::Paint); } }
BadgeColor Badge::color() const noexcept { return color_; }
Badge& Badge::color(BadgeColor value) noexcept { setColor(value); return *this; }
void Badge::setColor(BadgeColor value) noexcept { if (color_ != value) { color_ = value; markDirty(DirtyFlag::Paint); } }
BadgeSize Badge::size() const noexcept { return size_; }
Badge& Badge::size(BadgeSize value) noexcept { setSize(value); return *this; }
void Badge::setSize(BadgeSize value) noexcept { if (size_ != value) { size_ = value; markDirty(DirtyFlag::Layout); } }
BadgeShape Badge::shape() const noexcept { return shape_; }
Badge& Badge::shape(BadgeShape value) noexcept { setShape(value); return *this; }
void Badge::setShape(BadgeShape value) noexcept { if (shape_ != value) { shape_ = value; markDirty(DirtyFlag::Paint); } }
SizeF Badge::measure(const Constraints& constraints) const
{
    const auto m = metrics(size_);
    return constraints.clamp({std::max(m.height, estimateTextWidth(text_, m.fontSize) + 2.0f * m.padX), m.height});
}
void Badge::paint(PaintContext& context)
{
    const Theme& current = theme(); const auto m = metrics(size_);
    Color background, foreground, border; palette(current, appearance_, color_, background, foreground, border);
    const RectF rect = bounds(); const float radius = badgeRadius(shape_, rect.height);
    if (background.a) context.fillRoundRect(rect, radius, background);
    if (border.a) context.strokeRoundRect({rect.x + 0.5f, rect.y + 0.5f, std::max(0.0f, rect.width - 1.0f), std::max(0.0f, rect.height - 1.0f)}, radius, current.stroke.thin, border);
    if (!text_.empty()) context.drawText(text_, rect.x + std::max(0.0f, (rect.width - estimateTextWidth(text_, m.fontSize)) * 0.5f),
        context.centeredTextBottom(text_, rect, m.fontSize, current.typography.weightSemibold), m.fontSize, foreground, current.typography.weightSemibold);
    clearDirty(DirtyFlag::Paint);
}

CounterBadge::CounterBadge(std::uint64_t count) : count_(count) {}
std::uint64_t CounterBadge::count() const noexcept { return count_; }
CounterBadge& CounterBadge::count(std::uint64_t value) noexcept { setCount(value); return *this; }
void CounterBadge::setCount(std::uint64_t value) noexcept { if (count_ != value) { count_ = value; markDirty(DirtyFlag::Layout); } }
std::uint64_t CounterBadge::max() const noexcept { return max_; }
CounterBadge& CounterBadge::max(std::uint64_t value) noexcept { setMax(value); return *this; }
void CounterBadge::setMax(std::uint64_t value) noexcept { value = std::max<std::uint64_t>(1, value); if (max_ != value) { max_ = value; markDirty(DirtyFlag::Layout); } }
bool CounterBadge::showZero() const noexcept { return showZero_; }
CounterBadge& CounterBadge::showZero(bool value) noexcept { setShowZero(value); return *this; }
void CounterBadge::setShowZero(bool value) noexcept { if (showZero_ != value) { showZero_ = value; markDirty(DirtyFlag::Layout); } }
std::string CounterBadge::text() const { if (count_ == 0 && !showZero_) return {}; return count_ > max_ ? std::to_string(max_) + "+" : std::to_string(count_); }
const std::string& CounterBadge::accessibleLabel() const noexcept { return accessibleLabel_; }
CounterBadge& CounterBadge::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void CounterBadge::setAccessibleLabel(std::string value) { accessibleLabel_ = std::move(value); markDirty(DirtyFlag::Style); }
std::string CounterBadge::generatedAccessibleLabel() const { if (!accessibleLabel_.empty()) return accessibleLabel_; if (count_ == 0 && !showZero_) return {}; return std::to_string(count_) + (count_ == 1 ? " notification" : " notifications"); }
BadgeSize CounterBadge::size() const noexcept { return size_; }
CounterBadge& CounterBadge::size(BadgeSize value) noexcept { setSize(value); return *this; }
void CounterBadge::setSize(BadgeSize value) noexcept { if (size_ != value) { size_ = value; markDirty(DirtyFlag::Layout); } }
SizeF CounterBadge::measure(const Constraints& constraints) const
{
    const auto m = metrics(size_); const std::string value = text();
    if (value.empty()) return constraints.clamp({0.0f, 0.0f});
    return constraints.clamp({std::max(m.height, estimateTextWidth(value, m.fontSize) + 2.0f * m.padX), m.height});
}
void CounterBadge::paint(PaintContext& context)
{
    const std::string value = text(); if (value.empty()) { clearDirty(DirtyFlag::Paint); return; }
    const Theme& current = theme(); const auto m = metrics(size_); const RectF rect = bounds();
    context.fillRoundRect(rect, current.radius.circular, current.colors.danger);
    context.drawText(value, rect.x + std::max(0.0f, (rect.width - estimateTextWidth(value, m.fontSize)) * 0.5f),
        context.centeredTextBottom(value, rect, m.fontSize, current.typography.weightSemibold), m.fontSize, current.colors.onBrand, current.typography.weightSemibold);
    clearDirty(DirtyFlag::Paint);
}

PresenceBadge::PresenceBadge(PresenceStatus status) : status_(status) {}
PresenceStatus PresenceBadge::status() const noexcept { return status_; }
PresenceBadge& PresenceBadge::status(PresenceStatus value) noexcept { setStatus(value); return *this; }
void PresenceBadge::setStatus(PresenceStatus value) noexcept { if (status_ != value) { status_ = value; markDirty(DirtyFlag::Paint); } }
PresenceBadgePosition PresenceBadge::position() const noexcept { return position_; }
PresenceBadge& PresenceBadge::position(PresenceBadgePosition value) noexcept { setPosition(value); return *this; }
void PresenceBadge::setPosition(PresenceBadgePosition value) noexcept { position_ = value; markDirty(DirtyFlag::Layout); }
float PresenceBadge::avatarSize() const noexcept { return avatarSize_; }
PresenceBadge& PresenceBadge::avatarSize(float value) noexcept { setAvatarSize(value); return *this; }
void PresenceBadge::setAvatarSize(float value) noexcept { value = std::isfinite(value) ? std::max(16.0f, value) : 32.0f; if (avatarSize_ != value) { avatarSize_ = value; markDirty(DirtyFlag::Layout); } }
RectF PresenceBadge::boundsForAvatar(const RectF& avatar) const noexcept
{
    const float extent = std::clamp(std::min(avatar.width, avatar.height) * 0.375f, 8.0f, 16.0f);
    const float overlap = extent * 0.28f;
    const bool right = position_ == PresenceBadgePosition::TopRight || position_ == PresenceBadgePosition::BottomRight;
    const bool bottom = position_ == PresenceBadgePosition::BottomRight || position_ == PresenceBadgePosition::BottomLeft;
    return {right ? avatar.x + avatar.width - extent + overlap : avatar.x - overlap,
            bottom ? avatar.y + avatar.height - extent + overlap : avatar.y - overlap,
            extent, extent};
}
const std::string& PresenceBadge::accessibleLabel() const noexcept { return accessibleLabel_; }
PresenceBadge& PresenceBadge::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void PresenceBadge::setAccessibleLabel(std::string value) { accessibleLabel_ = std::move(value); markDirty(DirtyFlag::Style); }
std::string PresenceBadge::generatedAccessibleLabel() const { return accessibleLabel_.empty() ? presenceText(status_) : accessibleLabel_; }
SizeF PresenceBadge::measure(const Constraints& constraints) const { const float extent = std::clamp(avatarSize_ * 0.375f, 8.0f, 16.0f); return constraints.clamp({extent, extent}); }
void PresenceBadge::paint(PaintContext& context)
{
    const Theme& current = theme(); const RectF rect = bounds(); const float radius = current.radius.circular;
    // White ring separates the status from the Avatar image at every Fluent
    // avatar size. Draw it before the colored disc so it is never clipped.
    context.fillRoundRect({rect.x - 1.0f, rect.y - 1.0f, rect.width + 2.0f, rect.height + 2.0f}, radius, current.colors.neutralBackground1.rest);
    context.fillRoundRect(rect, radius, presenceColor(current, status_));
    if (status_ == PresenceStatus::DoNotDisturb) {
        const float band = std::max(2.0f, rect.height * 0.24f);
        context.fillRoundRect({rect.x + rect.width * 0.2f, rect.y + (rect.height - band) * 0.5f, rect.width * 0.6f, band}, band, current.colors.onBrand);
    } else if (status_ == PresenceStatus::OutOfOffice) {
        context.strokeRoundRect({rect.x + rect.width * 0.23f, rect.y + rect.height * 0.23f, rect.width * 0.54f, rect.height * 0.54f}, radius, std::max(1.0f, rect.width * 0.13f), current.colors.onBrand);
    }
    clearDirty(DirtyFlag::Paint);
}

} // namespace wui
