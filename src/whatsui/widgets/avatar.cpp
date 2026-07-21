#include "wui/avatar.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "wui/theme.h"
#include "wui/text_metrics.h"

namespace wui {
namespace {

float asFloat(AvatarSize size) noexcept { return static_cast<float>(static_cast<int>(size)); }

float activityRingInset(float extent) noexcept
{
    // Fluent's Avatar variants use a 2-DIP ring through 48 DIP and a 3-DIP
    // ring from 56 DIP upward. The ring intentionally overflows the Avatar
    // root; it is therefore painted before the fill rather than expanding
    // measure()/layout().
    return extent >= 56.0f ? 3.0f : 2.0f;
}

float personGlyphExtent(float extent) noexcept
{
    // The design system uses a discrete optical scale rather than a fixed
    // percentage. In particular, the 32-DIP default Avatar contains a
    // 20-DIP person glyph, while the 64-DIP form contains 32 DIP.
    if (extent < 20.0f) return 12.0f;
    if (extent < 28.0f) return 16.0f;
    if (extent < 40.0f) return 20.0f;
    if (extent < 56.0f) return 24.0f;
    if (extent < 72.0f) return 32.0f;
    if (extent < 96.0f) return 36.0f;
    return 48.0f;
}

void drawDefaultPerson(PaintContext& context, const RectF& avatar,
                       Color color) noexcept
{
    const float glyphExtent = personGlyphExtent(std::min(avatar.width, avatar.height));
    const RectF glyph = context.snapRectEdges(
        {avatar.x + (avatar.width - glyphExtent) * 0.5f,
         avatar.y + (avatar.height - glyphExtent) * 0.5f,
         glyphExtent, glyphExtent});
    const float stroke = context.snapStrokeWidth(glyphExtent >= 20.0f ? 1.5f : 1.0f);
    const float halfStroke = stroke * 0.5f;
    const PointF headCenter{glyph.x + glyph.width * 0.5f,
                            glyph.y + glyph.height * 0.30f};
    const float headRadius = glyphExtent * 0.18f;
    context.strokeCircle(headCenter, std::max(0.0f, headRadius - halfStroke),
                         stroke, color);

    // This follows the Fluent "Person" silhouette: a circular head above a
    // shallow capsule shoulder line. Keeping the stroke within the glyph
    // avoids clipping at 125/150% fractional scales.
    const RectF shoulders{
        glyph.x + glyph.width * 0.13f + halfStroke,
        glyph.y + glyph.height * 0.56f + halfStroke,
        std::max(0.0f, glyph.width * 0.74f - stroke),
        std::max(0.0f, glyph.height * 0.29f - stroke)};
    context.strokeRoundRect(shoulders, std::max(0.0f, shoulders.height * 0.5f),
                            stroke, color);
}

float groupSpacing(AvatarGroupLayout layout, float extent) noexcept
{
    if (layout == AvatarGroupLayout::Stack) {
        if (extent < 24.0f) return 2.0f;
        if (extent < 48.0f) return 4.0f;
        if (extent < 96.0f) return 8.0f;
        return 16.0f;
    }
    if (extent < 20.0f) return 8.0f;
    if (extent < 32.0f) return 10.0f;
    if (extent < 64.0f) return 16.0f;
    return 24.0f;
}

float groupDivider(float extent) noexcept
{
    if (extent < 56.0f) return 2.0f;
    if (extent < 72.0f) return 3.0f;
    return 4.0f;
}

Color avatarFill(AvatarColor color, const Theme& current) noexcept
{
    switch (color) {
    case AvatarColor::Brand: return current.colors.brandBackground.rest;
    case AvatarColor::Red: return {196, 49, 75, 255};
    case AvatarColor::Cranberry: return {192, 0, 125, 255};
    case AvatarColor::Green: return {16, 124, 16, 255};
    case AvatarColor::DarkGreen: return {13, 102, 35, 255};
    case AvatarColor::Marigold: return {202, 80, 16, 255};
    case AvatarColor::Plum: return {136, 23, 152, 255};
    case AvatarColor::Purple: return {93, 47, 145, 255};
    case AvatarColor::Teal: return {0, 130, 114, 255};
    case AvatarColor::Neutral: default: return current.colors.neutralBackground3.rest;
    }
}

Color avatarText(AvatarColor color, const Theme& current) noexcept
{
    return color == AvatarColor::Neutral ? current.colors.neutralForeground2 : current.colors.onBrand;
}

std::size_t nextCodePoint(const std::string& value, std::size_t offset) noexcept
{
    if (offset >= value.size()) return value.size();
    const unsigned char lead = static_cast<unsigned char>(value[offset]);
    const std::size_t length = lead < 0x80 ? 1 : (lead & 0xE0) == 0xC0 ? 2
        : (lead & 0xF0) == 0xE0 ? 3 : (lead & 0xF8) == 0xF0 ? 4 : 1;
    return std::min(value.size(), offset + length);
}

std::string firstCodePoint(const std::string& value)
{
    return value.empty() ? std::string{} : value.substr(0, nextCodePoint(value, 0));
}

std::string derivedInitials(const std::string& name)
{
    std::size_t first = 0;
    while (first < name.size() && name[first] == ' ') ++first;
    if (first == name.size()) return {};
    const std::size_t afterFirst = nextCodePoint(name, first);
    std::size_t last = first;
    for (std::size_t index = afterFirst; index < name.size();) {
        if (name[index] == ' ') {
            ++index;
            while (index < name.size() && name[index] == ' ') ++index;
            if (index < name.size()) last = index;
        } else {
            index = nextCodePoint(name, index);
        }
    }
    std::string result = name.substr(first, afterFirst - first);
    if (last != first) result += name.substr(last, nextCodePoint(name, last) - last);
    return result;
}

float centeredTextX(const std::string& text, const RectF& box, float textSize, int weight) noexcept
{
    // Text may be an UTF-8 initial (rather than one ASCII byte).  Use the
    // active renderer's advance width whenever it is available so initials
    // and the AvatarGroup overflow counter remain genuinely centred.
    float width = textSize * 0.60f * static_cast<float>(text.size());
    if (const auto* measurer = textMeasurer()) {
        width = measurer->measureText(text, textSize, weight).width;
    }
    return box.x + (box.width - width) * 0.5f;
}

} // namespace

Avatar::Avatar(std::string name, AvatarSize size) : name_(std::move(name)), size_(size) {}
const std::string& Avatar::name() const noexcept { return name_; }
Avatar& Avatar::name(std::string value) { setName(std::move(value)); return *this; }
void Avatar::setName(std::string value) { if (name_ != value) { name_ = std::move(value); markDirty(DirtyFlag::Paint); } }
const std::string& Avatar::initials() const noexcept { return initials_; }
Avatar& Avatar::initials(std::string value) { setInitials(std::move(value)); return *this; }
void Avatar::setInitials(std::string value) { if (initials_ != value) { initials_ = std::move(value); markDirty(DirtyFlag::Paint); } }
std::string Avatar::displayedInitials() const { return initials_.empty() ? derivedInitials(name_) : initials_; }
Avatar& Avatar::image(ImageSource source) { setImage(std::move(source)); return *this; }
void Avatar::setImage(ImageSource source) { image_ = std::move(source); syncImageChild(); markDirty(DirtyFlag::Paint); }
void Avatar::clearImage() noexcept { if (image_) { image_.reset(); clearChildren(); markDirty(DirtyFlag::Paint); } }
bool Avatar::hasImage() const noexcept { return image_.has_value() && !image_->empty(); }
Avatar& Avatar::size(AvatarSize value) noexcept { setSize(value); return *this; }
void Avatar::setSize(AvatarSize value) noexcept { if (size_ != value) { size_ = value; markDirty(DirtyFlag::Layout); } }
AvatarSize Avatar::size() const noexcept { return size_; }
Avatar& Avatar::shape(AvatarShape value) noexcept { setShape(value); return *this; }
void Avatar::setShape(AvatarShape value) noexcept { if (shape_ != value) { shape_ = value; syncImageChild(); markDirty(DirtyFlag::Paint); } }
AvatarShape Avatar::shape() const noexcept { return shape_; }
Avatar& Avatar::color(AvatarColor value) noexcept { setColor(value); return *this; }
void Avatar::setColor(AvatarColor value) noexcept { if (color_ != value) { color_ = value; markDirty(DirtyFlag::Paint); } }
AvatarColor Avatar::color() const noexcept { return color_; }
Avatar& Avatar::active(bool value) noexcept { setActive(value); return *this; }
void Avatar::setActive(bool value) noexcept
{
    if (active_ != value) {
        active_ = value;
        markDirty(DirtyFlag::Paint);
    }
}
bool Avatar::isActive() const noexcept { return active_; }
Avatar& Avatar::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void Avatar::setAccessibleLabel(std::string value) { if (accessibleLabel_ != value) { accessibleLabel_ = std::move(value); markDirty(DirtyFlag::Style); } }
const std::string& Avatar::accessibleLabel() const noexcept { return accessibleLabel_; }
SizeF Avatar::measure(const Constraints& constraints) const { const float extent = asFloat(size_); return constraints.clamp({extent, extent}); }
void Avatar::layout(const RectF& bounds)
{
    Node::layout(bounds);
    for (const auto& child : children()) child->layout(bounds);
}
void Avatar::paint(PaintContext& context)
{
    const auto& current = theme();
    const float extent = std::min(bounds().width, bounds().height);
    const RectF visual = context.snapRectEdges(
        {bounds().x + (bounds().width - extent) * 0.5f,
         bounds().y + (bounds().height - extent) * 0.5f, extent, extent});
    const float radius = shape_ == AvatarShape::Circular ? extent * 0.5f : current.radius.medium;

    if (active_ && shape_ == AvatarShape::Circular) {
        const float ringInset = activityRingInset(extent);
        const float ringStroke = context.snapStrokeWidth(ringInset);
        const PointF center{visual.x + visual.width * 0.5f,
                            visual.y + visual.height * 0.5f};
        const float ringRadius = extent * 0.5f + ringInset;
        // Figma's Activity ring has a white guard fill and a blue OUTSIDE
        // stroke. Painting it first leaves a 1/1.5-DIP white separator after
        // the Avatar fill has covered its inner half.
        context.fillCircle(center, ringRadius, current.colors.neutralBackground1.rest);
        context.strokeCircle(center, ringRadius, ringStroke,
                             current.colors.brandBackground.rest);
    }
    if (hasImage() && !children().empty()) {
        const int checkpoint = context.save();
        if (shape_ == AvatarShape::Circular) context.clipRoundRect(visual, radius);
        else context.clipRoundRect(visual, radius);
        children().front()->paint(context);
        context.restoreTo(checkpoint);
    } else {
        context.fillRoundRect(visual, radius, avatarFill(color_, current));
        const std::string initials = displayedInitials();
        if (!initials.empty()) {
            const float textSize = std::max(8.0f, extent * 0.375f);
            context.drawText(initials, centeredTextX(initials, visual, textSize, current.typography.weightSemibold),
                             context.centeredTextBottom(initials, visual, textSize, current.typography.weightSemibold),
                             textSize, avatarText(color_, current), current.typography.weightSemibold);
        } else {
            drawDefaultPerson(context, visual, current.colors.neutralForeground2);
        }
    }
    clearDirty(DirtyFlag::Paint);
}
void Avatar::syncImageChild()
{
    clearChildren();
    if (!hasImage()) return;
    auto child = std::make_unique<Image>(*image_);
    child->setFit(ImageFit::Cover);
    child->setShape(shape_ == AvatarShape::Circular ? ImageShape::Circular : ImageShape::Rounded);
    appendChild(std::move(child));
}

Avatar& AvatarGroup::addAvatar(std::string name, AvatarSize size)
{
    // A Fluent AvatarGroup is deliberately homogeneous.  Preserve the
    // convenient per-avatar size argument for an unconfigured group by
    // adopting it for the whole group before the first child is created.
    if (children().empty() && size_ == AvatarSize::Size32 && size != AvatarSize::Size32) size_ = size;
    auto avatar = std::make_unique<Avatar>(std::move(name), size_);
    auto* result = avatar.get();
    appendChild(std::move(avatar));
    return *result;
}
AvatarGroup& AvatarGroup::maxVisible(std::size_t value) noexcept { setMaxVisible(value); return *this; }
void AvatarGroup::setMaxVisible(std::size_t value) noexcept { value = std::max<std::size_t>(1, value); if (maxVisible_ != value) { maxVisible_ = value; markDirty(DirtyFlag::Layout); } }
std::size_t AvatarGroup::maxVisible() const noexcept { return maxVisible_; }
AvatarGroup& AvatarGroup::groupLayout(AvatarGroupLayout value) noexcept { setGroupLayout(value); return *this; }
void AvatarGroup::setGroupLayout(AvatarGroupLayout value) noexcept { if (layout_ != value) { layout_ = value; markDirty(DirtyFlag::Layout); } }
AvatarGroupLayout AvatarGroup::groupLayout() const noexcept { return layout_; }
AvatarGroup& AvatarGroup::size(AvatarSize value) noexcept { setSize(value); return *this; }
void AvatarGroup::setSize(AvatarSize value) noexcept
{
    if (size_ == value) return;
    size_ = value;
    for (const auto& child : children()) if (auto* avatar = dynamic_cast<Avatar*>(child.get())) avatar->setSize(value);
    markDirty(DirtyFlag::Layout);
}
AvatarSize AvatarGroup::size() const noexcept { return size_; }
AvatarGroup& AvatarGroup::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void AvatarGroup::setAccessibleLabel(std::string value) { if (accessibleLabel_ != value) { accessibleLabel_ = std::move(value); markDirty(DirtyFlag::Style); } }
const std::string& AvatarGroup::accessibleLabel() const noexcept { return accessibleLabel_; }
std::size_t AvatarGroup::visibleCount() const noexcept { return std::min(maxVisible_, children().size()); }
float AvatarGroup::avatarExtent() const noexcept { return asFloat(size_); }
float AvatarGroup::overlap() const noexcept
{
    return layout_ == AvatarGroupLayout::Stack
               ? groupSpacing(layout_, avatarExtent())
               : -groupSpacing(layout_, avatarExtent());
}
SizeF AvatarGroup::measure(const Constraints& constraints) const
{
    const std::size_t shown = visibleCount();
    if (shown == 0) return constraints.clamp({0.0f, 0.0f});
    const float extent = avatarExtent();
    const float width = extent * static_cast<float>(shown) -
                        overlap() * static_cast<float>(shown - 1)
        + (children().size() > shown ? extent * 0.72f : 0.0f);
    return constraints.clamp({width, extent});
}
void AvatarGroup::layout(const RectF& bounds)
{
    Node::layout(bounds);
    const float extent = avatarExtent();
    const std::size_t shown = visibleCount();
    const float step = extent - overlap();
    for (std::size_t index = 0; index < shown; ++index) {
        children()[index]->layout({bounds.x + step * static_cast<float>(index), bounds.y, extent, extent});
    }
}
void AvatarGroup::paint(PaintContext& context)
{
    const std::size_t shown = visibleCount();
    const auto& current = theme();
    if (layout_ == AvatarGroupLayout::Stack) {
        const float divider =
            context.snapStrokeWidth(groupDivider(avatarExtent()));
        for (std::size_t index = 0; index < shown; ++index) {
            const RectF child = context.snapRectEdges(children()[index]->bounds());
            const PointF center{child.x + child.width * 0.5f,
                                child.y + child.height * 0.5f};
            context.fillCircle(
                center, std::min(child.width, child.height) * 0.5f + divider,
                current.colors.neutralBackground2.rest);
            children()[index]->paint(context);
        }
    } else {
        for (std::size_t index = 0; index < shown; ++index)
            children()[index]->paint(context);
    }
    if (children().size() > shown) {
        const float extent = avatarExtent() * 0.72f;
        const float x = bounds().x + (avatarExtent() - overlap()) * static_cast<float>(shown);
        const RectF counter{x, bounds().y + (avatarExtent() - extent) * 0.5f, extent, extent};
        context.fillRoundRect(counter, extent * 0.5f, current.colors.neutralBackground3.rest);
        const std::string value = "+" + std::to_string(children().size() - shown);
        const float textSize = std::max(9.0f, extent * 0.36f);
        context.drawText(value, centeredTextX(value, counter, textSize, current.typography.weightSemibold),
                         context.centeredTextBottom(value, counter, textSize, current.typography.weightSemibold),
                         textSize, current.colors.neutralForeground1, current.typography.weightSemibold);
    }
    clearDirty(DirtyFlag::Paint);
}

} // namespace wui
