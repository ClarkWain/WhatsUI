#include "wui/persona.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "wui/text_metrics.h"
#include "wui/theme.h"

namespace wui {
namespace {

AvatarSize toAvatarSize(PersonaSize value) noexcept
{
    // This is the Fluent v9 Persona mapping: 20, 28, 32, 36, 40 and 56 DIP.
    switch (value) {
    case PersonaSize::ExtraSmall: return AvatarSize::Size20;
    case PersonaSize::Small: return AvatarSize::Size28;
    case PersonaSize::Medium: return AvatarSize::Size32;
    case PersonaSize::Large: return AvatarSize::Size36;
    case PersonaSize::ExtraLarge: return AvatarSize::Size40;
    case PersonaSize::Huge: return AvatarSize::Size56;
    }
    return AvatarSize::Size32;
}

float avatarExtent(PersonaSize value) noexcept
{
    return static_cast<float>(static_cast<int>(toAvatarSize(value)));
}

std::size_t nextCodePoint(const std::string& text, std::size_t index) noexcept
{
    if (index >= text.size()) return text.size();
    const unsigned char lead = static_cast<unsigned char>(text[index]);
    const std::size_t count = lead < 0x80 ? 1 : (lead & 0xE0) == 0xC0 ? 2
        : (lead & 0xF0) == 0xE0 ? 3 : (lead & 0xF8) == 0xF0 ? 4 : 1;
    return std::min(text.size(), index + count);
}

bool isPrimary(const PointerEvent& event) noexcept
{
    return event.button == MouseButton::Left;
}

} // namespace

Persona::Persona(std::string name, PersonaSize size)
    : name_(std::move(name)), size_(size)
{
    syncChildren();
}

const std::string& Persona::name() const noexcept { return name_; }
Persona& Persona::name(std::string value) { setName(std::move(value)); return *this; }
void Persona::setName(std::string value)
{
    if (name_ == value) return;
    name_ = std::move(value);
    syncChildren();
    markDirty(DirtyFlag::Layout);
}
const std::string& Persona::primaryText() const noexcept { return primaryText_.empty() ? name_ : primaryText_; }
Persona& Persona::primaryText(std::string value) { setPrimaryText(std::move(value)); return *this; }
void Persona::setPrimaryText(std::string value) { if (primaryText_ != value) { primaryText_ = std::move(value); markDirty(DirtyFlag::Layout); } }
const std::string& Persona::secondaryText() const noexcept { return secondaryText_; }
Persona& Persona::secondaryText(std::string value) { setSecondaryText(std::move(value)); return *this; }
void Persona::setSecondaryText(std::string value) { if (secondaryText_ != value) { secondaryText_ = std::move(value); markDirty(DirtyFlag::Layout); } }
const std::string& Persona::tertiaryText() const noexcept { return tertiaryText_; }
Persona& Persona::tertiaryText(std::string value) { setTertiaryText(std::move(value)); return *this; }
void Persona::setTertiaryText(std::string value) { if (tertiaryText_ != value) { tertiaryText_ = std::move(value); markDirty(DirtyFlag::Layout); } }
const std::string& Persona::quaternaryText() const noexcept { return quaternaryText_; }
Persona& Persona::quaternaryText(std::string value) { setQuaternaryText(std::move(value)); return *this; }
void Persona::setQuaternaryText(std::string value) { if (quaternaryText_ != value) { quaternaryText_ = std::move(value); markDirty(DirtyFlag::Layout); } }

Persona& Persona::avatarColor(AvatarColor value) noexcept { setAvatarColor(value); return *this; }
void Persona::setAvatarColor(AvatarColor value) noexcept { if (avatarColor_ != value) { avatarColor_ = value; syncChildren(); markDirty(DirtyFlag::Paint); } }
AvatarColor Persona::avatarColor() const noexcept { return avatarColor_; }
Persona& Persona::avatarShape(AvatarShape value) noexcept { setAvatarShape(value); return *this; }
void Persona::setAvatarShape(AvatarShape value) noexcept { if (avatarShape_ != value) { avatarShape_ = value; syncChildren(); markDirty(DirtyFlag::Paint); } }
AvatarShape Persona::avatarShape() const noexcept { return avatarShape_; }
Persona& Persona::avatarImage(ImageSource value) { setAvatarImage(std::move(value)); return *this; }
void Persona::setAvatarImage(ImageSource value) { avatarImage_ = std::move(value); syncChildren(); markDirty(DirtyFlag::Paint); }
void Persona::clearAvatarImage() noexcept { if (avatarImage_) { avatarImage_.reset(); syncChildren(); markDirty(DirtyFlag::Paint); } }

Persona& Persona::presence(PresenceStatus value) noexcept { setPresence(value); return *this; }
void Persona::setPresence(PresenceStatus value) noexcept { if (presence_ != value) { presence_ = value; syncChildren(); markDirty(DirtyFlag::Layout); } }
void Persona::clearPresence() noexcept { if (presence_) { presence_.reset(); syncChildren(); markDirty(DirtyFlag::Layout); } }
std::optional<PresenceStatus> Persona::presence() const noexcept { return presence_; }
Persona& Persona::presenceOnly(bool value) noexcept { setPresenceOnly(value); return *this; }
void Persona::setPresenceOnly(bool value) noexcept { if (presenceOnly_ != value) { presenceOnly_ = value; syncChildren(); markDirty(DirtyFlag::Layout); } }
bool Persona::isPresenceOnly() const noexcept { return presenceOnly_; }

Persona& Persona::size(PersonaSize value) noexcept { setSize(value); return *this; }
void Persona::setSize(PersonaSize value) noexcept { if (size_ != value) { size_ = value; syncChildren(); markDirty(DirtyFlag::Layout); } }
PersonaSize Persona::size() const noexcept { return size_; }
Persona& Persona::textPosition(PersonaTextPosition value) noexcept { setTextPosition(value); return *this; }
void Persona::setTextPosition(PersonaTextPosition value) noexcept { if (textPosition_ != value) { textPosition_ = value; markDirty(DirtyFlag::Layout); } }
PersonaTextPosition Persona::textPosition() const noexcept { return textPosition_; }
Persona& Persona::textAlignment(PersonaTextAlignment value) noexcept { setTextAlignment(value); return *this; }
void Persona::setTextAlignment(PersonaTextAlignment value) noexcept { if (textAlignment_ != value) { textAlignment_ = value; markDirty(DirtyFlag::Paint); } }
PersonaTextAlignment Persona::textAlignment() const noexcept { return textAlignment_; }
Persona& Persona::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void Persona::setAccessibleLabel(std::string value) { if (accessibleLabel_ != value) { accessibleLabel_ = std::move(value); markDirty(DirtyFlag::Style); } }
const std::string& Persona::accessibleLabel() const noexcept { return accessibleLabel_; }
std::string Persona::generatedAccessibleLabel() const
{
    if (!accessibleLabel_.empty()) return accessibleLabel_;
    std::string result = primaryText();
    for (const auto& line : {secondaryText_, tertiaryText_, quaternaryText_}) {
        if (!line.empty()) { if (!result.empty()) result += ", "; result += line; }
    }
    if (presence_) { if (!result.empty()) result += ", "; result += PresenceBadge(*presence_).generatedAccessibleLabel(); }
    return result;
}

Persona& Persona::onClick(ClickHandler handler) { onClick_ = std::move(handler); markDirty(DirtyFlag::Style); return *this; }
bool Persona::isInteractive() const noexcept { return static_cast<bool>(onClick_); }
AvatarSize Persona::avatarSize() const noexcept { return toAvatarSize(size_); }

void Persona::syncChildren()
{
    clearChildren();
    if (presenceOnly_) {
        if (presence_) {
            auto badge = std::make_unique<PresenceBadge>(*presence_);
            badge->setAvatarSize(avatarExtent(size_));
            appendChild(std::move(badge));
        }
        return;
    }
    auto avatar = std::make_unique<Avatar>(name_, avatarSize());
    avatar->setColor(avatarColor_);
    avatar->setShape(avatarShape_);
    avatar->setAccessibleLabel(name_);
    if (avatarImage_) avatar->setImage(*avatarImage_);
    appendChild(std::move(avatar));
    if (presence_) {
        auto badge = std::make_unique<PresenceBadge>(*presence_);
        badge->setAvatarSize(avatarExtent(size_));
        appendChild(std::move(badge));
    }
}

std::vector<std::string> Persona::textLines() const
{
    std::vector<std::string> result;
    const std::string& primary = primaryText();
    if (!primary.empty()) result.push_back(primary);
    if (!secondaryText_.empty()) result.push_back(secondaryText_);
    if (!tertiaryText_.empty()) result.push_back(tertiaryText_);
    if (!quaternaryText_.empty()) result.push_back(quaternaryText_);
    return result;
}

Persona::TextLineStyle Persona::textStyle(std::size_t index) const noexcept
{
    const auto& typography = theme().typography;
    if (index == 0) {
        // Fluent Persona uses body1 by default and upgrades the primary line
        // to subtitle2 for ExtraLarge/Huge identities.
        if (size_ == PersonaSize::ExtraLarge || size_ == PersonaSize::Huge) {
            return {typography.subtitle2.size, typography.subtitle2.lineHeight,
                    typography.subtitle2.weight, theme().colors.neutralForeground1};
        }
        return {typography.body1.size, typography.body1.lineHeight,
                typography.body1.weight, theme().colors.neutralForeground1};
    }
    if (!presenceOnly_ && size_ == PersonaSize::Huge) {
        return {typography.body1.size, typography.body1.lineHeight,
                typography.body1.weight, theme().colors.neutralForeground2};
    }
    return {typography.caption1.size, typography.caption1.lineHeight,
            typography.caption1.weight, theme().colors.neutralForeground2};
}

float Persona::textWidth(const std::string& text, const TextLineStyle& style) const noexcept
{
    if (const auto* measurer = textMeasurer()) return measurer->measureText(text, style.size, style.weight).width;
    return static_cast<float>(text.size()) * style.size * 0.56f;
}

std::string Persona::ellipsize(const std::string& text, float width, const TextLineStyle& style) const
{
    if (width <= 0.0f) return {};
    if (textWidth(text, style) <= width) return text;
    const std::string mark = "…";
    const float markWidth = textWidth(mark, style);
    if (markWidth > width) return {};
    std::size_t end = 0;
    while (end < text.size()) {
        const std::size_t next = nextCodePoint(text, end);
        if (textWidth(text.substr(0, next) + mark, style) > width) break;
        end = next;
    }
    return text.substr(0, end) + mark;
}

float Persona::textBlockHeight() const noexcept
{
    const auto lines = textLines();
    float total = 0.0f;
    for (std::size_t i = 0; i < lines.size(); ++i) total += textStyle(i).lineHeight;
    // Fluent's secondary line overlaps the primary line box by 2 DIP. This
    // is deliberate optical leading, not an arbitrary glyph-baseline nudge.
    if (lines.size() > 1) total -= theme().spacing.vertical.xxs;
    return total;
}

float Persona::preferredTextWidth() const noexcept
{
    const auto lines = textLines();
    float width = 0.0f;
    for (std::size_t i = 0; i < lines.size(); ++i) width = std::max(width, textWidth(lines[i], textStyle(i)));
    return width;
}

float Persona::mediaSpacing() const noexcept
{
    // Fluent Persona's media spacing uses the nudge scale, rather than one
    // universal gap, so compact rows do not look loose and huge identities do
    // not crowd their text block.
    const auto& spacing = theme().spacing.horizontal;
    if (presenceOnly_ && size_ == PersonaSize::Small) return spacing.sNudge;
    switch (size_) {
    case PersonaSize::ExtraSmall: return spacing.sNudge;
    case PersonaSize::Small:
    case PersonaSize::Medium: return spacing.s;
    case PersonaSize::Large:
    case PersonaSize::ExtraLarge: return spacing.mNudge;
    case PersonaSize::Huge: return spacing.m;
    }
    return spacing.s;
}

SizeF Persona::measure(const Constraints& constraints) const
{
    const float gap = children().empty() ? 0.0f : mediaSpacing();
    const float indicator = children().empty() ? 0.0f : (presenceOnly_ ? children().front()->measureWithConstraints({}).width : avatarExtent(size_));
    const float textWidthValue = preferredTextWidth();
    const float textHeight = textBlockHeight();
    if (textPosition_ == PersonaTextPosition::Below) {
        return constraints.clamp({std::max(indicator, textWidthValue), indicator + (textHeight > 0.0f && indicator > 0.0f ? gap : 0.0f) + textHeight});
    }
    return constraints.clamp({indicator + (indicator > 0.0f && textWidthValue > 0.0f ? gap : 0.0f) + textWidthValue,
                              std::max(indicator, textHeight)});
}

void Persona::layout(const RectF& bounds)
{
    Node::layout(bounds);
    const float gap = children().empty() ? 0.0f : mediaSpacing();
    const float indicator = children().empty() ? 0.0f : (presenceOnly_ ? children().front()->measureWithConstraints({}).width : avatarExtent(size_));
    const float textHeight = textBlockHeight();
    RectF indicatorBounds{};
    if (textPosition_ == PersonaTextPosition::Below) {
        indicatorBounds = {bounds.x + std::max(0.0f, (bounds.width - indicator) * 0.5f), bounds.y, indicator, indicator};
    } else if (textPosition_ == PersonaTextPosition::Before) {
        indicatorBounds = {bounds.x + std::max(0.0f, bounds.width - indicator), bounds.y + std::max(0.0f, (bounds.height - indicator) * 0.5f), indicator, indicator};
    } else {
        indicatorBounds = {bounds.x, bounds.y + std::max(0.0f, (bounds.height - indicator) * 0.5f), indicator, indicator};
    }
    if (presenceOnly_ && textAlignment_ == PersonaTextAlignment::Start &&
        textPosition_ != PersonaTextPosition::Below &&
        size_ != PersonaSize::ExtraLarge && size_ != PersonaSize::Huge) {
        const float primaryLine = textLines().empty()
            ? 0.0f
            : textStyle(0).lineHeight;
        indicatorBounds.y =
            bounds.y + std::max(0.0f, (primaryLine - indicator) * 0.5f);
    }
    if (!children().empty()) {
        children().front()->layout(indicatorBounds);
        if (!presenceOnly_ && children().size() > 1) {
            auto* badge = static_cast<PresenceBadge*>(children()[1].get());
            badge->layout(badge->boundsForAvatar(indicatorBounds));
        }
    }
    (void)gap;
    (void)textHeight;
    clearLayoutDirtyRecursively();
}

void Persona::paint(PaintContext& context)
{
    const auto& current = theme();
    const bool interactive = isInteractive() && isEnabled();
    const bool hovered = interactive && (visualStates() & toMask(ControlVisualState::Hovered)) != 0;
    const bool pressed = interactive && (visualStates() & toMask(ControlVisualState::Pressed)) != 0;
    const bool focused = interactive && (visualStates() & toMask(ControlVisualState::Focused)) != 0;
    const RectF renderedBounds = context.snapRectEdges(bounds());
    if (hovered || pressed) {
        context.fillRoundRect(
            renderedBounds, current.radius.medium,
            pressed ? current.colors.neutralBackground1.pressed
                    : current.colors.neutralBackground1.hover);
    }
    if (focused) {
        const float outer = current.controls.focusInset;
        context.strokeRoundRect(
            context.snapRectEdges(
                {renderedBounds.x - outer, renderedBounds.y - outer,
                 renderedBounds.width + outer * 2.0f,
                 renderedBounds.height + outer * 2.0f}),
            current.radius.medium + outer,
            context.snapStrokeWidth(current.stroke.thick),
            current.colors.strokeFocusOuter);
        context.strokeRoundRect(
            context.snapRectEdges(
                {renderedBounds.x - current.stroke.thin,
                 renderedBounds.y - current.stroke.thin,
                 renderedBounds.width + current.stroke.thin * 2.0f,
                 renderedBounds.height + current.stroke.thin * 2.0f}),
            current.radius.medium + current.stroke.thin,
            context.snapStrokeWidth(current.stroke.thin),
            current.colors.strokeFocusInner);
    }

    ContainerNode::paint(context);
    const auto lines = textLines();
    if (lines.empty()) { clearDirty(DirtyFlag::Paint); return; }
    const float indicator = children().empty() ? 0.0f : children().front()->bounds().width;
    const float gap = children().empty() ? 0.0f : mediaSpacing();
    RectF textBounds{};
    const float totalHeight = textBlockHeight();
    if (textPosition_ == PersonaTextPosition::Below) {
        textBounds = {bounds().x, bounds().y + indicator + (indicator > 0.0f ? gap : 0.0f), bounds().width, std::max(0.0f, bounds().height - indicator - gap)};
    } else if (textPosition_ == PersonaTextPosition::Before) {
        textBounds = {bounds().x, bounds().y, std::max(0.0f, bounds().width - indicator - (indicator > 0.0f ? gap : 0.0f)), bounds().height};
    } else {
        textBounds = {bounds().x + indicator + (indicator > 0.0f ? gap : 0.0f), bounds().y, std::max(0.0f, bounds().width - indicator - (indicator > 0.0f ? gap : 0.0f)), bounds().height};
    }
    float y = textBounds.y + (textPosition_ == PersonaTextPosition::Below || textAlignment_ == PersonaTextAlignment::Start ? 0.0f : std::max(0.0f, (textBounds.height - totalHeight) * 0.5f));
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index == 1) y -= current.spacing.vertical.xxs;
        const TextLineStyle style = textStyle(index);
        const std::string rendered = ellipsize(lines[index], textBounds.width, style);
        const float lineWidth = textWidth(rendered, style);
        const float x = textAlignment_ == PersonaTextAlignment::Center ? textBounds.x + std::max(0.0f, (textBounds.width - lineWidth) * 0.5f) : textBounds.x;
        const RectF lineBox{x, y, std::min(lineWidth, textBounds.width), style.lineHeight};
        context.drawText(rendered, x, context.centeredTextBottom(rendered, lineBox, style.size, style.weight), style.size, style.color, style.weight, current.typography.familyBase);
        y += style.lineHeight;
    }
    clearDirty(DirtyFlag::Paint);
}

void Persona::invoke() { if (onClick_) onClick_(); }
bool Persona::onPointerEvent(const PointerEvent& event)
{
    if (!isInteractive() || !isEnabled()) return false;
    switch (event.action) {
    case PointerAction::Down: if (!isPrimary(event)) return false; setVisualState(ControlVisualState::Pressed, true); setVisualState(ControlVisualState::Focused, true); return true;
    case PointerAction::Up: {
        if (!isPrimary(event)) return false;
        const bool activate = (visualStates() & toMask(ControlVisualState::Pressed)) != 0 && bounds().contains(event.position);
        setVisualState(ControlVisualState::Pressed, false);
        if (activate) invoke();
        return true;
    }
    case PointerAction::Enter: setVisualState(ControlVisualState::Hovered, true); return true;
    case PointerAction::Move: setVisualState(ControlVisualState::Hovered, bounds().contains(event.position)); return true;
    case PointerAction::Leave: setVisualState(ControlVisualState::Hovered, false); return true;
    case PointerAction::Cancel: setVisualState(ControlVisualState::Pressed, false); return true;
    }
    return false;
}
bool Persona::onKeyEvent(const KeyEvent& event)
{
    if (!isInteractive() || !isEnabled() || event.action != KeyAction::Down || (event.keyCode != 13 && event.keyCode != 32)) return false;
    invoke();
    return true;
}
AccessibilityActionCapabilities Persona::accessibilityActions() const noexcept { AccessibilityActionCapabilities actions; actions.invoke = isInteractive(); return actions; }
AccessibilityActionStatus Persona::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value)
{
    (void)value;
    if (kind != AccessibilityActionKind::Invoke || !isInteractive()) return AccessibilityActionStatus::NotSupported;
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    invoke();
    return AccessibilityActionStatus::Succeeded;
}

} // namespace wui
