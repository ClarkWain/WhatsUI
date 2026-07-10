#include "wui/widgets.h"

#include <algorithm>
#include <utility>

#include "wui/text_metrics.h"
#include "wui/theme.h"

namespace wui {

Text::Text(std::string value)
    : value_(std::move(value))
{
}

const std::string& Text::value() const noexcept
{
    return value_;
}

Text& Text::value(std::string value)
{
    setValue(std::move(value));
    return *this;
}

void Text::setValue(std::string value)
{
    value_ = std::move(value);
    markDirty(DirtyFlag::Layout);
}

float Text::fontSize() const noexcept
{
    return fontSize_;
}

void Text::setFontSize(float size) noexcept
{
    fontSize_ = std::max(1.0f, size);
    markDirty(DirtyFlag::Layout);
}

float Text::lineHeight() const noexcept
{
    return lineHeight_;
}

void Text::setLineHeight(float height) noexcept
{
    lineHeight_ = std::max(0.0f, height);
    markDirty(DirtyFlag::Layout);
}

void Text::setColor(Color color) noexcept
{
    color_ = color;
    hasColor_ = true;
    markDirty(DirtyFlag::Paint);
}

void Text::clearColor() noexcept
{
    hasColor_ = false;
    markDirty(DirtyFlag::Paint);
}

SizeF Text::measure(const Constraints& constraints) const
{
    if (const TextMeasurer* measurer = textMeasurer()) {
        const TextExtents extents = measurer->measureText(value_, fontSize_);
        return constraints.clamp({extents.width, lineHeight_ > 0.0f ? lineHeight_ : extents.height});
    }
    const auto width = static_cast<float>(value_.size()) * (fontSize_ * 0.5f);
    const auto height = lineHeight_ > 0.0f ? lineHeight_ : fontSize_ * 1.25f;
    return constraints.clamp({width, height});
}

float Text::baselineOffset() const noexcept
{
    TextExtents extents;
    extents.height = fontSize_ * 1.25f;
    extents.ascent = fontSize_ * 0.8f;
    extents.descent = fontSize_ * 0.2f;
    if (const TextMeasurer* measurer = textMeasurer()) {
        extents = measurer->measureText(value_, fontSize_);
    }
    const float height = lineHeight_ > 0.0f ? lineHeight_ : extents.height;
    const float glyphHeight = extents.ascent + extents.descent;
    return std::max(0.0f, height - glyphHeight) * 0.5f + extents.ascent;
}

void Text::paint(PaintContext& context)
{
    if (!value_.empty()) {
        const float baseline = bounds().y + baselineOffset();
        const Color color = hasColor_ ? color_ : theme().colors.text;
        context.drawText(value_, bounds().x, baseline, fontSize_, color);
    }
    clearDirty(DirtyFlag::Paint);
}

Spacer::Spacer(SizeF size) noexcept
    : size_(size)
{
}

SizeF Spacer::size() const noexcept
{
    return size_;
}

void Spacer::setSize(SizeF size) noexcept
{
    size_ = size;
    markDirty(DirtyFlag::Layout);
}

SizeF Spacer::measure(const Constraints& constraints) const
{
    return constraints.clamp(size_);
}

void Spacer::paint(PaintContext& context)
{
    (void)context;
    clearDirty(DirtyFlag::Paint);
}

} // namespace wui
