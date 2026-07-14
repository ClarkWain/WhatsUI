#include "wui/widgets.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#ifdef WHATSUI_HAS_WHATSCANVAS
#include "wsc/Canvas.h"
#include "wsc/Image.h"
#include "wsc/Paint.h"
#endif

namespace wui {

namespace detail {
class ImageResource {
public:
#ifdef WHATSUI_HAS_WHATSCANVAS
    wsc::Image image;
    wsc::Canvas* canvas{nullptr};
#endif
    std::vector<unsigned char> pixels;
    int pixelWidth{0};
    int pixelHeight{0};
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
        throw std::invalid_argument("Image dimensions must be positive");
    }
    const auto expected = static_cast<std::size_t>(pixelWidth) * static_cast<std::size_t>(pixelHeight) * 4u;
    if (rgbaPixels.size() != expected) {
        throw std::invalid_argument("Image RGBA data size does not match its dimensions");
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

Image::Image() = default;

Image::Image(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight)
{
    setSource(std::move(rgbaPixels), pixelWidth, pixelHeight);
}

Image::Image(ImageSource source)
    : source_(std::move(source))
{
}

Image::~Image() = default;

Image& Image::source(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight)
{
    setSource(std::move(rgbaPixels), pixelWidth, pixelHeight);
    return *this;
}

Image& Image::source(ImageSource source)
{
    setSource(std::move(source));
    return *this;
}

void Image::setSource(std::vector<unsigned char> rgbaPixels, int pixelWidth, int pixelHeight)
{
    setSource(ImageSource(std::move(rgbaPixels), pixelWidth, pixelHeight));
}

void Image::setSource(ImageSource source)
{
    source_ = std::move(source);
    markDirty(DirtyFlag::Layout);
    markDirty(DirtyFlag::Paint);
}

void Image::clearSource() noexcept
{
    source_ = ImageSource{};
    markDirty(DirtyFlag::Layout);
    markDirty(DirtyFlag::Paint);
}

const ImageSource Image::imageSource() const noexcept { return source_; }

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
    return {static_cast<float>(source_.pixelWidth()), static_cast<float>(source_.pixelHeight())};
}

bool Image::hasSource() const noexcept
{
    return !source_.empty();
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
        auto& resource = source_.resource_;
        if (resource->canvas != canvas || !resource->image.isTextureValid()) {
            resource->image = wsc::Image{};
            if (resource->image.loadFromRGBA(*canvas, resource->pixels, resource->pixelWidth, resource->pixelHeight)) {
                resource->canvas = canvas;
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
    const auto& resource = source_.resource_;
    if (canvas != nullptr && resource && resource->canvas == canvas
        && resource->image.isTextureValid()) {
            wsc::Paint paint;
            // Images are untinted by default. WhatsCanvas modulates texture
            // samples by the paint color, whose default is not guaranteed to
            // be opaque white across backends.
            paint.setColor(wsc::Color(255, 255, 255, 255));
            paint.setImageSampling(wsc::Paint::ImageSampling::LINEAR);
            const float scale = context.canvasCoordinateScale();
            const wsc::RectF destination(bounds().x * scale, bounds().y * scale,
                                          bounds().width * scale, bounds().height * scale);
            wsc::Canvas::ImageFit canvasFit = wsc::Canvas::ImageFit::CONTAIN;
            switch (fit_) {
            case ImageFit::Fill: canvasFit = wsc::Canvas::ImageFit::FILL; break;
            case ImageFit::Cover: canvasFit = wsc::Canvas::ImageFit::COVER; break;
            case ImageFit::Contain: break;
            }
            canvas->drawImageFit(resource->image, destination, canvasFit,
                                 alignment_.x, alignment_.y, paint);
    }
#else
    (void)context;
#endif
    clearDirty(DirtyFlag::Paint);
}

} // namespace wui
