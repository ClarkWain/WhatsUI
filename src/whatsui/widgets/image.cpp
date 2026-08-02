#include "wui/widgets.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "wui/theme.h"

#ifdef WHATSUI_HAS_WHATSCANVAS
#include "wsc/Canvas.h"
#include "wsc/Image.h"
#include "wsc/Paint.h"
#endif

namespace wui {

namespace detail {
class ImageResource {
public:
    std::vector<unsigned char> pixels;
    int pixelWidth{0};
    int pixelHeight{0};
};

class ImageTexture {
public:
#ifdef WHATSUI_HAS_WHATSCANVAS
    wsc::Image image;
    wsc::Canvas* canvas{nullptr};
#endif
};
} // namespace detail

namespace {

using ImageResourcePtr = std::shared_ptr<detail::ImageResource>;

std::uint64_t sourceHash(const std::vector<unsigned char>& pixels, int width, int height) noexcept
{
    // FNV-1a; equality is still checked below, so a collision is harmless.
    std::uint64_t hash = 1469598103934665603ull;
    const auto add = [&hash](unsigned char value) { hash = (hash ^ value) * 1099511628211ull; };
    for (int value : {width, height}) {
        for (unsigned int shift = 0; shift < sizeof(value) * 8; shift += 8) {
            add(static_cast<unsigned char>(static_cast<unsigned int>(value) >> shift));
        }
    }
    for (const auto value : pixels) {
        add(value);
    }
    return hash;
}

ImageResourcePtr internImageResource(std::vector<unsigned char> pixels, int width, int height)
{
    static std::mutex cacheMutex;
    static std::unordered_map<std::uint64_t, std::vector<std::weak_ptr<detail::ImageResource>>> cache;
    const auto hash = sourceHash(pixels, width, height);
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto& candidates = cache[hash];
    for (auto it = candidates.begin(); it != candidates.end();) {
        if (auto existing = it->lock()) {
            if (existing->pixelWidth == width && existing->pixelHeight == height && existing->pixels == pixels) {
                return existing;
            }
            ++it;
        } else {
            it = candidates.erase(it);
        }
    }
    auto resource = std::make_shared<detail::ImageResource>();
    resource->pixels = std::move(pixels);
    resource->pixelWidth = width;
    resource->pixelHeight = height;
    candidates.emplace_back(resource);
    return resource;
}

} // namespace

ImageSource::ImageSource(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight)
{
    if (pixelWidth <= 0 || pixelHeight <= 0) {
        throw std::invalid_argument("ImageNode dimensions must be positive");
    }
    const auto expected = static_cast<std::size_t>(pixelWidth) * static_cast<std::size_t>(pixelHeight) * 4u;
    if (rgbaPixels.size() != expected) {
        throw std::invalid_argument("ImageNode RGBA data size does not match its dimensions");
    }
    resource_ = internImageResource(std::move(rgbaPixels), pixelWidth, pixelHeight);
}

ImageSource::ImageSource(std::shared_ptr<detail::ImageResource> resource) noexcept
    : resource_(std::move(resource))
{
}

int ImageSource::pixelWidth() const noexcept { return resource_ ? resource_->pixelWidth : 0; }
int ImageSource::pixelHeight() const noexcept { return resource_ ? resource_->pixelHeight : 0; }
bool ImageSource::empty() const noexcept { return !resource_; }
bool ImageSource::operator==(const ImageSource& other) const noexcept { return resource_ == other.resource_; }

ImageNode::ImageNode() = default;

ImageNode::ImageNode(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight)
{
    setSource(std::move(rgbaPixels), pixelWidth, pixelHeight);
}

ImageNode::ImageNode(ImageSource source)
    : source_(std::move(source))
{
}

ImageNode::~ImageNode() = default;

ImageNode& ImageNode::source(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight)
{
    setSource(std::move(rgbaPixels), pixelWidth, pixelHeight);
    return *this;
}

ImageNode& ImageNode::source(ImageSource source)
{
    setSource(std::move(source));
    return *this;
}

void ImageNode::setSource(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight)
{
    setSource(ImageSource(std::move(rgbaPixels), pixelWidth, pixelHeight));
}

void ImageNode::setSource(ImageSource source)
{
    source_ = std::move(source);
    texture_.reset();
    markDirty(DirtyFlag::Layout);
    markDirty(DirtyFlag::Paint);
}

void ImageNode::clearSource() noexcept
{
    source_ = ImageSource{};
    texture_.reset();
    markDirty(DirtyFlag::Layout);
    markDirty(DirtyFlag::Paint);
}

ImageNode& ImageNode::fallback(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight)
{
    return fallback(ImageSource(std::move(rgbaPixels), pixelWidth, pixelHeight));
}

ImageNode& ImageNode::fallback(ImageSource source)
{
    setFallback(std::move(source));
    return *this;
}

void ImageNode::setFallback(ImageSource source)
{
    fallback_ = std::move(source);
    texture_.reset();
    markDirty(DirtyFlag::Layout);
    markDirty(DirtyFlag::Paint);
}

void ImageNode::clearFallback() noexcept
{
    fallback_ = ImageSource{};
    texture_.reset();
    markDirty(DirtyFlag::Layout);
    markDirty(DirtyFlag::Paint);
}

const ImageSource ImageNode::imageSource() const noexcept { return source_; }

ImageNode& ImageNode::fit(ImageFit fit) noexcept
{
    setFit(fit);
    return *this;
}

void ImageNode::setFit(ImageFit fit) noexcept
{
    fit_ = fit;
    markDirty(DirtyFlag::Paint);
}

ImageFit ImageNode::fit() const noexcept
{
    return fit_;
}

ImageNode& ImageNode::align(float x, float y) noexcept
{
    setAlignment(x, y);
    return *this;
}

void ImageNode::setAlignment(float x, float y) noexcept
{
    alignment_ = {std::clamp(x, 0.0f, 1.0f), std::clamp(y, 0.0f, 1.0f)};
    markDirty(DirtyFlag::Paint);
}

PointF ImageNode::alignment() const noexcept
{
    return alignment_;
}

ImageNode& ImageNode::shape(ImageShape shape) noexcept { setShape(shape); return *this; }
void ImageNode::setShape(ImageShape shape) noexcept { shape_ = shape; markDirty(DirtyFlag::Paint); }
ImageShape ImageNode::shape() const noexcept { return shape_; }
ImageNode& ImageNode::bordered(bool bordered) noexcept { setBordered(bordered); return *this; }
void ImageNode::setBordered(bool bordered) noexcept { bordered_ = bordered; markDirty(DirtyFlag::Paint); }
bool ImageNode::isBordered() const noexcept { return bordered_; }
ImageNode& ImageNode::shadow(bool shadow) noexcept { setShadow(shadow); return *this; }
void ImageNode::setShadow(bool shadow) noexcept { shadow_ = shadow; markDirty(DirtyFlag::Paint); }
bool ImageNode::hasShadow() const noexcept { return shadow_; }
ImageNode& ImageNode::block(bool block) noexcept { setBlock(block); return *this; }
void ImageNode::setBlock(bool block) noexcept { block_ = block; markDirty(DirtyFlag::Layout); }
bool ImageNode::isBlock() const noexcept { return block_; }
ImageNode& ImageNode::alt(std::string description) { setAlt(std::move(description)); return *this; }
void ImageNode::setAlt(std::string description)
{
    if (alt_ != description) { alt_ = std::move(description); markDirty(DirtyFlag::Style); }
}
const std::string& ImageNode::alt() const noexcept { return alt_; }
ImageNode& ImageNode::decorative(bool decorative) noexcept { setDecorative(decorative); return *this; }
void ImageNode::setDecorative(bool decorative) noexcept
{
    if (decorative_ != decorative) { decorative_ = decorative; markDirty(DirtyFlag::Style); }
}
bool ImageNode::isDecorative() const noexcept { return decorative_; }

SizeF ImageNode::intrinsicSize() const noexcept
{
    const auto& source = effectiveSource();
    return {static_cast<float>(source.pixelWidth()), static_cast<float>(source.pixelHeight())};
}

bool ImageNode::hasSource() const noexcept
{
    return !effectiveSource().empty();
}

const ImageSource& ImageNode::effectiveSource() const noexcept
{
    return source_.empty() ? fallback_ : source_;
}

SizeF ImageNode::measure(const Constraints& constraints) const
{
    SizeF desired = intrinsicSize();
    if (block_ && desired.width > 0.0f && desired.height > 0.0f &&
        std::isfinite(constraints.maxWidth)) {
        const float ratio = desired.height / desired.width;
        desired.width = constraints.maxWidth;
        desired.height = desired.width * ratio;
        if (std::isfinite(constraints.maxHeight) && desired.height > constraints.maxHeight) {
            desired.height = constraints.maxHeight;
            desired.width = desired.height / ratio;
        }
    }
    return constraints.clamp(desired);
}

void ImageNode::prepare(PaintContext& context)
{
#ifdef WHATSUI_HAS_WHATSCANVAS
    auto* canvas = context.canvas();
    if (canvas != nullptr && hasSource()) {
        const auto& resource = effectiveSource().resource_;
        if (!texture_) texture_ = std::make_unique<detail::ImageTexture>();
        if (texture_->canvas != canvas || !texture_->image.isTextureValid()) {
            texture_->image = wsc::Image{};
            if (texture_->image.loadFromRGBA(*canvas, resource->pixels, resource->pixelWidth, resource->pixelHeight)) {
                texture_->canvas = canvas;
            }
        }
    }
#else
    (void)context;
#endif
}

void ImageNode::paint(PaintContext& context)
{
    const auto& current = theme();
    const RectF renderedBounds = context.snapRectEdges(bounds());
    RectF shapeBounds = renderedBounds;
    if (shape_ == ImageShape::Circular) {
        const float extent =
            std::min(renderedBounds.width, renderedBounds.height);
        shapeBounds = context.snapRectEdges(
            {renderedBounds.x + (renderedBounds.width - extent) * 0.5f,
             renderedBounds.y + (renderedBounds.height - extent) * 0.5f,
             extent, extent});
    }
    float radius = current.radius.none;
    if (shape_ == ImageShape::Rounded) radius = current.radius.medium;
    else if (shape_ == ImageShape::Circular) radius = current.radius.circular;
    if (shadow_) {
        const auto& elevation = current.elevation.shadow4;
        context.drawBoxShadow(shapeBounds, radius, elevation.ambient.blur, elevation.ambient.offsetX,
                              elevation.ambient.offsetY, elevation.ambient.spread, elevation.ambient.color);
        context.drawBoxShadow(shapeBounds, radius, elevation.key.blur, elevation.key.offsetX,
                              elevation.key.offsetY, elevation.key.spread, elevation.key.color);
    }
    const float border = bordered_
        ? context.snapStrokeWidth(current.stroke.thin)
        : 0.0f;
    const RectF destinationBounds = context.snapRectEdges(
        {shapeBounds.x + border, shapeBounds.y + border,
         std::max(0.0f, shapeBounds.width - border * 2.0f),
         std::max(0.0f, shapeBounds.height - border * 2.0f)});
    const float innerRadius = std::max(0.0f, radius - border);
    const int checkpoint = context.save();
    if (shape_ == ImageShape::Square) context.clipRect(destinationBounds);
    else context.clipRoundRect(destinationBounds, innerRadius);
#ifdef WHATSUI_HAS_WHATSCANVAS
    auto* canvas = context.canvas();
    const auto& resource = effectiveSource().resource_;
    if (canvas != nullptr && resource && texture_ && texture_->canvas == canvas
        && texture_->image.isTextureValid()) {
            wsc::Paint paint;
            // Images are untinted by default. WhatsCanvas modulates texture
            // samples by the paint color, whose default is not guaranteed to
            // be opaque white across backends.
            paint.setColor(wsc::Color(255, 255, 255, 255));
            paint.setImageSampling(wsc::Paint::ImageSampling::LINEAR);
            const float scale = context.canvasCoordinateScale();
            const wsc::RectF destination(destinationBounds.x * scale, destinationBounds.y * scale,
                                          destinationBounds.width * scale, destinationBounds.height * scale);
            wsc::Canvas::ImageFit canvasFit = wsc::Canvas::ImageFit::CONTAIN;
            bool drawWithFit = true;
            switch (fit_) {
            case ImageFit::Default:
            case ImageFit::Fill: canvasFit = wsc::Canvas::ImageFit::FILL; break;
            case ImageFit::Cover: canvasFit = wsc::Canvas::ImageFit::COVER; break;
            case ImageFit::Contain: break;
            case ImageFit::None:
            case ImageFit::Center: {
                const float width = static_cast<float>(resource->pixelWidth);
                const float height = static_cast<float>(resource->pixelHeight);
                const float x = fit_ == ImageFit::Center
                    ? destinationBounds.x + (destinationBounds.width - width) * 0.5f
                    : destinationBounds.x;
                const float y = fit_ == ImageFit::Center
                    ? destinationBounds.y + (destinationBounds.height - height) * 0.5f
                    : destinationBounds.y;
                canvas->drawImage(texture_->image,
                    wsc::RectF(x * scale, y * scale, width * scale, height * scale), paint);
                drawWithFit = false;
                break;
            }
            }
            if (drawWithFit) {
                canvas->drawImageFit(texture_->image, destination, canvasFit,
                                     alignment_.x, alignment_.y, paint);
            }
    }
#else
    (void)context;
#endif
    context.restoreTo(checkpoint);
    if (bordered_) {
        const float half = border * 0.5f;
        const RectF outline = context.snapRectEdges(
            {shapeBounds.x + half, shapeBounds.y + half,
             std::max(0.0f, shapeBounds.width - border),
             std::max(0.0f, shapeBounds.height - border)});
        const float outlineRadius = shape_ == ImageShape::Circular
            ? std::min(outline.width, outline.height) * 0.5f
            : std::max(0.0f, radius - half);
        context.strokeRoundRect(outline, outlineRadius, border,
                                current.colors.neutralStroke1);
    }
    clearDirty(DirtyFlag::Paint);
}

} // namespace wui
