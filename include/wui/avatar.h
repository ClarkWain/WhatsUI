#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "wui/widgets.h"

namespace wui {

// Fluent Avatar sizes are physical-looking logical DIP sizes.  Keeping the
// size in the type avoids each application inventing a slightly different
// circular person indicator.
enum class AvatarSize {
    Size16 = 16,
    Size20 = 20,
    Size24 = 24,
    Size28 = 28,
    Size32 = 32,
    Size36 = 36,
    Size40 = 40,
    Size48 = 48,
    Size56 = 56,
    Size64 = 64,
    Size72 = 72,
    Size96 = 96,
    Size120 = 120,
    Size128 = 128,
};

enum class AvatarShape { Circular, Square };
enum class AvatarColor {
    Neutral,
    Brand,
    Red,
    Cranberry,
    Green,
    DarkGreen,
    Marigold,
    Plum,
    Purple,
    Teal,
};

class Avatar : public Node {
public:
    explicit Avatar(std::string name = {}, AvatarSize size = AvatarSize::Size32);

    [[nodiscard]] const std::string& name() const noexcept;
    Avatar& name(std::string value);
    void setName(std::string value);
    [[nodiscard]] const std::string& initials() const noexcept;
    Avatar& initials(std::string value);
    void setInitials(std::string value);
    [[nodiscard]] std::string displayedInitials() const;

    Avatar& image(ImageSource source);
    void setImage(ImageSource source);
    void clearImage() noexcept;
    [[nodiscard]] bool hasImage() const noexcept;

    Avatar& size(AvatarSize value) noexcept;
    void setSize(AvatarSize value) noexcept;
    [[nodiscard]] AvatarSize size() const noexcept;
    Avatar& shape(AvatarShape value) noexcept;
    void setShape(AvatarShape value) noexcept;
    [[nodiscard]] AvatarShape shape() const noexcept;
    Avatar& color(AvatarColor value) noexcept;
    void setColor(AvatarColor value) noexcept;
    [[nodiscard]] AvatarColor color() const noexcept;

    // Fluent calls this the activity ring. It is a presence-of-activity
    // affordance, not a keyboard focus indicator: the ring stays outside the
    // Avatar's visual circle and does not change its layout footprint.
    Avatar& active(bool value = true) noexcept;
    void setActive(bool value) noexcept;
    [[nodiscard]] bool isActive() const noexcept;
    Avatar& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;

private:
    void syncImageChild();

    std::string name_;
    std::string initials_;
    std::string accessibleLabel_;
    std::optional<ImageSource> image_;
    AvatarSize size_{AvatarSize::Size32};
    AvatarShape shape_{AvatarShape::Circular};
    AvatarColor color_{AvatarColor::Neutral};
    bool active_{false};
};

enum class AvatarGroupLayout { Stack, Spread };

// An ordered collection of avatars. Children remain real Avatar nodes (rather
// than a flattened picture) so every person retains an accessible name and a
// stable image resource. Excess avatars collapse into a deterministic +N
// indicator.
class AvatarGroup : public ContainerNode {
public:
    Avatar& addAvatar(std::string name = {}, AvatarSize size = AvatarSize::Size32);
    AvatarGroup& maxVisible(std::size_t value) noexcept;
    void setMaxVisible(std::size_t value) noexcept;
    [[nodiscard]] std::size_t maxVisible() const noexcept;
    AvatarGroup& groupLayout(AvatarGroupLayout value) noexcept;
    void setGroupLayout(AvatarGroupLayout value) noexcept;
    [[nodiscard]] AvatarGroupLayout groupLayout() const noexcept;
    AvatarGroup& size(AvatarSize value) noexcept;
    void setSize(AvatarSize value) noexcept;
    [[nodiscard]] AvatarSize size() const noexcept;
    AvatarGroup& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;

private:
    [[nodiscard]] std::size_t visibleCount() const noexcept;
    [[nodiscard]] float avatarExtent() const noexcept;
    [[nodiscard]] float overlap() const noexcept;
    std::size_t maxVisible_{5};
    AvatarGroupLayout layout_{AvatarGroupLayout::Stack};
    AvatarSize size_{AvatarSize::Size32};
    std::string accessibleLabel_;
};

} // namespace wui
