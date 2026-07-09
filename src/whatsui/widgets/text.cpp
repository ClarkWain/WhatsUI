#include "wui/widgets.h"

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

SizeF Text::measure(const Constraints& constraints) const
{
    if (const TextMeasurer* measurer = textMeasurer()) {
        const TextExtents extents = measurer->measureText(value_, fontSize_);
        return constraints.clamp({extents.width, extents.height});
    }
    const auto width = static_cast<float>(value_.size()) * (fontSize_ * 0.5f);
    const auto height = fontSize_ * 1.25f;
    return constraints.clamp({width, height});
}

void Text::paint(PaintContext& context)
{
    if (!value_.empty()) {
        float baseline = bounds().y + fontSize_ * 0.8f;
        if (const TextMeasurer* measurer = textMeasurer()) {
            baseline = bounds().y + measurer->measureText(value_, fontSize_).ascent;
        }
        context.drawText(value_, bounds().x, baseline, fontSize_, theme().colors.text);
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
