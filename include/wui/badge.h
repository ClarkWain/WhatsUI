#pragma once

// Fluent 2 status badges.  Badges are passive content: they deliberately do
// not accept focus or pointer activation, but expose concise text semantics
// to assistive technology.

#include <cstdint>
#include <optional>
#include <string>

#include "wui/node.h"

namespace wui {

enum class BadgeAppearance { Filled, Ghost, Outline, Tint };
enum class BadgeColor { Neutral, Brand, Danger, Important, Informative, Success, Warning };
enum class BadgeSize { Small, Medium, Large, ExtraLarge };
enum class BadgeShape { Rounded, Circular, Square };

class Badge : public Node {
public:
    explicit Badge(std::string text = {});

    [[nodiscard]] const std::string& text() const noexcept;
    Badge& text(std::string value);
    void setText(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    Badge& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] std::string generatedAccessibleLabel() const;

    [[nodiscard]] BadgeAppearance appearance() const noexcept;
    Badge& appearance(BadgeAppearance value) noexcept;
    void setAppearance(BadgeAppearance value) noexcept;
    [[nodiscard]] BadgeColor color() const noexcept;
    Badge& color(BadgeColor value) noexcept;
    void setColor(BadgeColor value) noexcept;
    [[nodiscard]] BadgeSize size() const noexcept;
    Badge& size(BadgeSize value) noexcept;
    void setSize(BadgeSize value) noexcept;
    [[nodiscard]] BadgeShape shape() const noexcept;
    Badge& shape(BadgeShape value) noexcept;
    void setShape(BadgeShape value) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;

private:
    std::string text_;
    std::string accessibleLabel_;
    BadgeAppearance appearance_{BadgeAppearance::Tint};
    BadgeColor color_{BadgeColor::Neutral};
    BadgeSize size_{BadgeSize::Medium};
    BadgeShape shape_{BadgeShape::Rounded};
};

// CounterBadge preserves its numeric value independently from its rendered
// text.  `max` controls Fluent's overflow presentation, e.g. 100 with max 99
// renders "99+" while accessibility receives "100 notifications".
class CounterBadge : public Node {
public:
    explicit CounterBadge(std::uint64_t count = 0);

    [[nodiscard]] std::uint64_t count() const noexcept;
    CounterBadge& count(std::uint64_t value) noexcept;
    void setCount(std::uint64_t value) noexcept;
    [[nodiscard]] std::uint64_t max() const noexcept;
    CounterBadge& max(std::uint64_t value) noexcept;
    void setMax(std::uint64_t value) noexcept;
    [[nodiscard]] bool showZero() const noexcept;
    CounterBadge& showZero(bool value = true) noexcept;
    void setShowZero(bool value = true) noexcept;
    [[nodiscard]] std::string text() const;
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    CounterBadge& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] std::string generatedAccessibleLabel() const;
    [[nodiscard]] BadgeSize size() const noexcept;
    CounterBadge& size(BadgeSize value) noexcept;
    void setSize(BadgeSize value) noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;

private:
    std::uint64_t count_{0};
    std::uint64_t max_{99};
    bool showZero_{false};
    std::string accessibleLabel_;
    BadgeSize size_{BadgeSize::Medium};
};

enum class PresenceStatus { Available, Away, Busy, DoNotDisturb, Offline, OutOfOffice, Unknown };
enum class PresenceBadgePosition { TopRight, BottomRight, BottomLeft, TopLeft };

// PresenceBadge can be laid out independently or anchored over an Avatar by
// calling boundsForAvatar().  This keeps Avatar free to choose its own image
// source while giving consumers stable, DPI-safe overlay geometry.
class PresenceBadge : public Node {
public:
    explicit PresenceBadge(PresenceStatus status = PresenceStatus::Available);

    [[nodiscard]] PresenceStatus status() const noexcept;
    PresenceBadge& status(PresenceStatus value) noexcept;
    void setStatus(PresenceStatus value) noexcept;
    [[nodiscard]] PresenceBadgePosition position() const noexcept;
    PresenceBadge& position(PresenceBadgePosition value) noexcept;
    void setPosition(PresenceBadgePosition value) noexcept;
    [[nodiscard]] float avatarSize() const noexcept;
    PresenceBadge& avatarSize(float value) noexcept;
    void setAvatarSize(float value) noexcept;
    [[nodiscard]] RectF boundsForAvatar(const RectF& avatarBounds) const noexcept;
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    PresenceBadge& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] std::string generatedAccessibleLabel() const;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void paint(PaintContext& context) override;

private:
    PresenceStatus status_{PresenceStatus::Available};
    PresenceBadgePosition position_{PresenceBadgePosition::BottomRight};
    float avatarSize_{32.0f};
    std::string accessibleLabel_;
};

} // namespace wui
