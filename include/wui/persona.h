#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "wui/avatar.h"
#include "wui/badge.h"

namespace wui {

// Fluent PersonaNode combines an AvatarNode (or a PresenceBadgeNode) with up to four
// concise text lines.  It remains passive unless an application explicitly
// supplies an activation handler; this prevents a directory/list presentation
// from accidentally becoming a collection of unnamed buttons.
enum class PersonaSize { ExtraSmall, Small, Medium, Large, ExtraLarge, Huge };
enum class PersonaTextPosition { After, Before, Below };
enum class PersonaTextAlignment { Start, Center };

class PersonaNode : public ControlNode {
public:
    using ClickHandler = std::function<void()>;

    explicit PersonaNode(std::string name = {}, PersonaSize size = PersonaSize::Medium);

    [[nodiscard]] const std::string& name() const noexcept;
    PersonaNode& name(std::string value);
    void setName(std::string value);
    [[nodiscard]] const std::string& primaryText() const noexcept;
    PersonaNode& primaryText(std::string value);
    void setPrimaryText(std::string value);
    [[nodiscard]] const std::string& secondaryText() const noexcept;
    PersonaNode& secondaryText(std::string value);
    void setSecondaryText(std::string value);
    [[nodiscard]] const std::string& tertiaryText() const noexcept;
    PersonaNode& tertiaryText(std::string value);
    void setTertiaryText(std::string value);
    [[nodiscard]] const std::string& quaternaryText() const noexcept;
    PersonaNode& quaternaryText(std::string value);
    void setQuaternaryText(std::string value);

    PersonaNode& avatarColor(AvatarColor value) noexcept;
    void setAvatarColor(AvatarColor value) noexcept;
    [[nodiscard]] AvatarColor avatarColor() const noexcept;
    PersonaNode& avatarShape(AvatarShape value) noexcept;
    void setAvatarShape(AvatarShape value) noexcept;
    [[nodiscard]] AvatarShape avatarShape() const noexcept;
    PersonaNode& avatarImage(ImageSource value);
    void setAvatarImage(ImageSource value);
    void clearAvatarImage() noexcept;

    PersonaNode& presence(PresenceStatus value) noexcept;
    void setPresence(PresenceStatus value) noexcept;
    void clearPresence() noexcept;
    [[nodiscard]] std::optional<PresenceStatus> presence() const noexcept;
    PersonaNode& presenceOnly(bool value = true) noexcept;
    void setPresenceOnly(bool value = true) noexcept;
    [[nodiscard]] bool isPresenceOnly() const noexcept;

    PersonaNode& size(PersonaSize value) noexcept;
    void setSize(PersonaSize value) noexcept;
    [[nodiscard]] PersonaSize size() const noexcept;
    PersonaNode& textPosition(PersonaTextPosition value) noexcept;
    void setTextPosition(PersonaTextPosition value) noexcept;
    [[nodiscard]] PersonaTextPosition textPosition() const noexcept;
    PersonaNode& textAlignment(PersonaTextAlignment value) noexcept;
    void setTextAlignment(PersonaTextAlignment value) noexcept;
    [[nodiscard]] PersonaTextAlignment textAlignment() const noexcept;
    PersonaNode& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    [[nodiscard]] std::string generatedAccessibleLabel() const;

    // Supplying an activation handler opt-ins to button semantics, focus and
    // pressed/hovered feedback. A PersonaNode without one is deliberately not
    // focusable or pointer-interactive.
    PersonaNode& onClick(ClickHandler handler);
    [[nodiscard]] bool isInteractive() const noexcept;

    [[nodiscard]] AvatarSize avatarSize() const noexcept;
    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(
        AccessibilityActionKind kind, std::string_view value) override;

private:
    struct TextLineStyle { float size; float lineHeight; int weight; Color color; };
    void syncChildren();
    [[nodiscard]] std::vector<std::string> textLines() const;
    [[nodiscard]] TextLineStyle textStyle(std::size_t index) const noexcept;
    [[nodiscard]] float textWidth(const std::string& text, const TextLineStyle& style) const noexcept;
    [[nodiscard]] std::string ellipsize(const std::string& text, float width, const TextLineStyle& style) const;
    [[nodiscard]] float textBlockHeight() const noexcept;
    [[nodiscard]] float preferredTextWidth() const noexcept;
    [[nodiscard]] float mediaSpacing() const noexcept;
    void invoke();

    std::string name_;
    std::string primaryText_;
    std::string secondaryText_;
    std::string tertiaryText_;
    std::string quaternaryText_;
    std::string accessibleLabel_;
    std::optional<ImageSource> avatarImage_;
    std::optional<PresenceStatus> presence_;
    AvatarColor avatarColor_{AvatarColor::Neutral};
    AvatarShape avatarShape_{AvatarShape::Circular};
    PersonaSize size_{PersonaSize::Medium};
    PersonaTextPosition textPosition_{PersonaTextPosition::After};
    PersonaTextAlignment textAlignment_{PersonaTextAlignment::Start};
    bool presenceOnly_{false};
    ClickHandler onClick_;
};

} // namespace wui
