#include "wui/widgets.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#ifdef WHATSUI_HAS_WHATSCANVAS
#include "wsc/Canvas.h"
#include "wsc/Image.h"
#include "wsc/Paint.h"
#endif

namespace wui {

#ifdef WHATSUI_HAS_WHATSCANVAS
namespace detail {
class ImageResource {
public:
    wsc::Image image;
    wsc::Canvas* canvas{nullptr};
};
} // namespace detail
#endif

Image::Image()
#ifdef WHATSUI_HAS_WHATSCANVAS
    : resource_(std::make_unique<detail::ImageResource>())
#endif
{
}

Image::Image(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight)
    : Image()
{
    setSource(std::move(rgbaPixels), pixelWidth, pixelHeight);
}

Image::~Image() = default;

Image& Image::source(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight)
{
    setSource(std::move(rgbaPixels), pixelWidth, pixelHeight);
    return *this;
}

void Image::setSource(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight)
{
    if (pixelWidth <= 0 || pixelHeight <= 0) {
        throw std::invalid_argument("Image dimensions must be positive");
    }
    const auto expected = static_cast<std::size_t>(pixelWidth) * static_cast<std::size_t>(pixelHeight) * 4u;
    if (rgbaPixels.size() != expected) {
        throw std::invalid_argument("Image RGBA data size does not match its dimensions");
    }
    pixels_ = std::move(rgbaPixels);
    pixelWidth_ = pixelWidth;
    pixelHeight_ = pixelHeight;
#ifdef WHATSUI_HAS_WHATSCANVAS
    resource_ = std::make_unique<detail::ImageResource>();
#endif
    markDirty(DirtyFlag::Layout);
    markDirty(DirtyFlag::Paint);
}

void Image::clearSource() noexcept
{
    pixels_.clear();
    pixelWidth_ = 0;
    pixelHeight_ = 0;
#ifdef WHATSUI_HAS_WHATSCANVAS
    resource_ = std::make_unique<detail::ImageResource>();
#endif
    markDirty(DirtyFlag::Layout);
    markDirty(DirtyFlag::Paint);
}

Image& Image::fit(ImageFit fit) noexcept
{
    setFit(fit);
    return *this;
}

void Image::setFit(ImageFit fit) noexcept
{
    fit_ = fit;
    markDirty(DirtyFlag::Paint);
}

ImageFit Image::fit() const noexcept
{
    return fit_;
}

Image& Image::align(float x, float y) noexcept
{
    setAlignment(x, y);
    return *this;
}

void Image::setAlignment(float x, float y) noexcept
{
    alignment_ = {std::clamp(x, 0.0f, 1.0f), std::clamp(y, 0.0f, 1.0f)};
    markDirty(DirtyFlag::Paint);
}

PointF Image::alignment() const noexcept
{
    return alignment_;
}

SizeF Image::intrinsicSize() const noexcept
{
    return {static_cast<float>(pixelWidth_), static_cast<float>(pixelHeight_)};
}

bool Image::hasSource() const noexcept
{
    return pixelWidth_ > 0 && pixelHeight_ > 0 && !pixels_.empty();
}

SizeF Image::measure(const Constraints& constraints) const
{
    return constraints.clamp(intrinsicSize());
}

void Image::prepare(PaintContext& context)
{
#ifdef WHATSUI_HAS_WHATSCANVAS
    auto* canvas = context.canvas();
    if (canvas != nullptr && hasSource()) {
        if (!resource_) {
            resource_ = std::make_unique<detail::ImageResource>();
        }
        if (resource_->canvas != canvas || !resource_->image.isTextureValid()) {
            resource_->image = wsc::Image{};
            if (resource_->image.loadFromRGBA(*canvas, pixels_, pixelWidth_, pixelHeight_)) {
                resource_->canvas = canvas;
            }
        }
    }
#else
    (void)context;
#endif
}

void Image::paint(PaintContext& context)
{
#ifdef WHATSUI_HAS_WHATSCANVAS
    auto* canvas = context.canvas();
    if (canvas != nullptr && resource_ && resource_->canvas == canvas
        && resource_->image.isTextureValid()) {
            wsc::Paint paint;
            // Images are untinted by default. WhatsCanvas modulates texture
            // samples by the paint color, whose default is not guaranteed to
            // be opaque white across backends.
            paint.setColor(wsc::Color(255, 255, 255, 255));
            paint.setImageSampling(wsc::Paint::ImageSampling::LINEAR);
            const float scale = context.scaleFactor();
            const wsc::RectF destination(bounds().x * scale, bounds().y * scale,
                                          bounds().width * scale, bounds().height * scale);
            wsc::Canvas::ImageFit canvasFit = wsc::Canvas::ImageFit::CONTAIN;
            switch (fit_) {
            case ImageFit::Fill: canvasFit = wsc::Canvas::ImageFit::FILL; break;
            case ImageFit::Cover: canvasFit = wsc::Canvas::ImageFit::COVER; break;
            case ImageFit::Contain: break;
            }
            canvas->drawImageFit(resource_->image, destination, canvasFit,
                                 alignment_.x, alignment_.y, paint);
    }
#else
    (void)context;
#endif
    clearDirty(DirtyFlag::Paint);
}

} // namespace wui
