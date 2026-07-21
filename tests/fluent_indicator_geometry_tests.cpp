#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"

#include "wui/basic_controls.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"
#include "wui/widgets.h"

namespace {

constexpr int kLogicalWidth = 120;
constexpr int kLogicalHeight = 112;
// Radio and Checkbox both use a 16-DIP indicator. The radio sits at the
// leading edge of its 32-DIP hit target; Checkbox centres its indicator in its
// own 32-DIP target.
constexpr float kRadioCenterX = 36.0f;
constexpr float kCheckboxCenterX = 76.0f;
constexpr float kIndicatorCenterY = 36.0f;
constexpr float kSelectedRadioCenterY = 76.0f;
constexpr float kPi = 3.14159265358979323846f;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void draw(wui::Node& node, wui::PaintContext& paint, wui::RectF bounds)
{
    node.layout(bounds);
    node.prepare(paint);
    node.paint(paint);
}

void savePpm(const std::string& path, const std::vector<std::uint8_t>& rgba, int width, int height)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot create Fluent indicator geometry artifact");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t index = 0; index + 3 < rgba.size(); index += 4) {
        output.put(static_cast<char>(rgba[index]));
        output.put(static_cast<char>(rgba[index + 1]));
        output.put(static_cast<char>(rgba[index + 2]));
    }
}

struct InkBounds {
    int left{std::numeric_limits<int>::max()};
    int top{std::numeric_limits<int>::max()};
    int right{-1};
    int bottom{-1};

    [[nodiscard]] bool valid() const noexcept { return right >= left && bottom >= top; }
    [[nodiscard]] int width() const noexcept { return valid() ? right - left + 1 : 0; }
    [[nodiscard]] int height() const noexcept { return valid() ? bottom - top + 1 : 0; }
};

bool isStrokeInk(const std::vector<std::uint8_t>& rgba, int width, int height, int x, int y)
{
    if (x < 0 || y < 0 || x >= width || y >= height) return false;
    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
    // Both unchecked indicator strokes use the accessible neutral token
    // (97,97,97).  175 preserves anti-aliased edge coverage at 150% while
    // excluding the white interior and #FAFAFA page surface.
    return rgba[offset] <= 175 && rgba[offset + 1] <= 175 && rgba[offset + 2] <= 175;
}

bool isBrandInk(const std::vector<std::uint8_t>& rgba, int width, int height,
                int x, int y)
{
    if (x < 0 || y < 0 || x >= width || y >= height) return false;
    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
    // Compound-brand AA pixels remain blue-dominant even when composited over
    // the neutral page. Keep the threshold independent from one theme shade.
    return rgba[offset + 2] >= rgba[offset] + 24 &&
        rgba[offset + 2] >= rgba[offset + 1] + 8;
}

InkBounds findInkBounds(const std::vector<std::uint8_t>& rgba, int width, int height,
                        float scale, wui::RectF logicalSearch)
{
    const int left = std::max(0, static_cast<int>(std::floor(logicalSearch.x * scale)));
    const int top = std::max(0, static_cast<int>(std::floor(logicalSearch.y * scale)));
    const int right = std::min(width - 1, static_cast<int>(std::ceil(
        (logicalSearch.x + logicalSearch.width) * scale)));
    const int bottom = std::min(height - 1, static_cast<int>(std::ceil(
        (logicalSearch.y + logicalSearch.height) * scale)));
    InkBounds result;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            if (!isStrokeInk(rgba, width, height, x, y)) continue;
            result.left = std::min(result.left, x);
            result.top = std::min(result.top, y);
            result.right = std::max(result.right, x);
            result.bottom = std::max(result.bottom, y);
        }
    }
    return result;
}

bool pixelIs(const std::vector<std::uint8_t>& rgba, int width, int height, float scale,
             float logicalX, float logicalY, wui::Color color)
{
    const int x = static_cast<int>(std::lround(logicalX * scale));
    const int y = static_cast<int>(std::lround(logicalY * scale));
    if (x < 0 || y < 0 || x >= width || y >= height) return false;
    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
    return rgba[offset] == color.r && rgba[offset + 1] == color.g
        && rgba[offset + 2] == color.b && rgba[offset + 3] == color.a;
}

float firstRadialInk(const std::vector<std::uint8_t>& rgba, int width, int height,
                     float centerX, float centerY, float radians, float maximumRadius)
{
    // 0.1 physical-pixel sampling makes this a genuine radial profile rather
    // than an axis-only probe. It detects flattened or low-segment circles at
    // 200% while being insensitive to the antialiased edge's final alpha.
    constexpr float step = 0.1f;
    for (float radius = 0.0f; radius <= maximumRadius; radius += step) {
        const int x = static_cast<int>(std::lround(centerX + std::cos(radians) * radius));
        const int y = static_cast<int>(std::lround(centerY + std::sin(radians) * radius));
        if (isStrokeInk(rgba, width, height, x, y)) return radius;
    }
    return -1.0f;
}

void verifyRadioRing(const std::vector<std::uint8_t>& rgba, int width, int height, float scale)
{
    const float centerX = kRadioCenterX * scale;
    const float centerY = kIndicatorCenterY * scale;
    const InkBounds ink = findInkBounds(rgba, width, height, scale, {26, 24, 24, 24});
    expect(ink.valid(), "Unchecked Radio must paint a visible ring");
    expect(std::fabs((static_cast<float>(ink.left + ink.right) * 0.5f) - centerX) <= 1.0f
               && std::fabs((static_cast<float>(ink.top + ink.bottom) * 0.5f) - centerY) <= 1.0f,
           "Radio ring must remain centred at every DPI");
    expect(std::abs(ink.width() - ink.height()) <= 1,
           "Radio ring bounding box must be circular rather than elliptical");

    // Rasterized even-DIP rings can land symmetrically between two physical
    // pixels. Use their measured visual centre for the radial profile while
    // retaining the logical-centre assertion above; otherwise every opposite
    // direction inherits a half-pixel sampling bias rather than geometry.
    const float radialCenterX = static_cast<float>(ink.left + ink.right) * 0.5f;
    const float radialCenterY = static_cast<float>(ink.top + ink.bottom) * 0.5f;

    constexpr int kSamples = 24;
    std::array<float, kSamples> radii{};
    float minimum = std::numeric_limits<float>::max();
    float maximum = 0.0f;
    for (int index = 0; index < kSamples; ++index) {
        const float radians = static_cast<float>(index) * 2.0f * kPi
            / static_cast<float>(kSamples);
        radii[static_cast<std::size_t>(index)] = firstRadialInk(
            rgba, width, height, radialCenterX, radialCenterY, radians, 14.0f * scale);
        expect(radii[static_cast<std::size_t>(index)] >= 0.0f,
               "Radio ring must have ink on every radial direction");
        minimum = std::min(minimum, radii[static_cast<std::size_t>(index)]);
        maximum = std::max(maximum, radii[static_cast<std::size_t>(index)]);
    }

    // At 200% a visibly faceted 8--12 sided approximation differs from a
    // circle by multiple physical pixels. A one-pixel allowance covers the
    // deliberate stroke width and raster rounding while rejecting that shape.
    expect(maximum - minimum <= 1.25f,
           "Radio radial profile is too faceted; it must not become an obvious polygon");
    for (int index = 0; index < kSamples / 2; ++index) {
        expect(std::fabs(radii[static_cast<std::size_t>(index)]
                         - radii[static_cast<std::size_t>(index + kSamples / 2)]) <= 0.75f,
               "Radio opposite radial directions must remain symmetric");
    }
    // Explicit cardinal and diagonal pairs make the intended geometry clear
    // in failure reports instead of relying only on aggregate radial spread.
    expect(std::fabs(radii[0] - radii[12]) <= 0.75f
               && std::fabs(radii[6] - radii[18]) <= 0.75f
               && std::fabs(radii[3] - radii[15]) <= 0.75f
               && std::fabs(radii[9] - radii[21]) <= 0.75f,
           "Radio axis and diagonal ring samples must be mirror-symmetric");
}

void verifySelectedRadioDot(const std::vector<std::uint8_t>& rgba, int width,
                            int height, float scale)
{
    const float centerX = kRadioCenterX * scale;
    const float centerY = kSelectedRadioCenterY * scale;
    const int left = static_cast<int>(std::floor(26.0f * scale));
    const int top = static_cast<int>(std::floor(66.0f * scale));
    const int right = static_cast<int>(std::ceil(46.0f * scale));
    const int bottom = static_cast<int>(std::ceil(86.0f * scale));
    const int seedX = static_cast<int>(std::lround(centerX));
    const int seedY = static_cast<int>(std::lround(centerY));
    const int searchWidth = right - left + 1;
    const int searchHeight = bottom - top + 1;
    std::vector<bool> visited(
        static_cast<std::size_t>(searchWidth * searchHeight), false);
    std::vector<std::pair<int, int>> pending{{seedX, seedY}};
    InkBounds ink;
    while (!pending.empty()) {
        const auto [x, y] = pending.back();
        pending.pop_back();
        if (x < left || x > right || y < top || y > bottom) continue;
        const auto visitedIndex = static_cast<std::size_t>(
            (y - top) * searchWidth + (x - left));
        if (visited[visitedIndex]) continue;
        visited[visitedIndex] = true;
        if (!isBrandInk(rgba, width, height, x, y)) continue;
        ink.left = std::min(ink.left, x);
        ink.top = std::min(ink.top, y);
        ink.right = std::max(ink.right, x);
        ink.bottom = std::max(ink.bottom, y);
        constexpr std::array<std::pair<int, int>, 4> neighbours{
            std::pair{-1, 0}, std::pair{1, 0},
            std::pair{0, -1}, std::pair{0, 1}};
        for (const auto [dx, dy] : neighbours) {
            pending.emplace_back(x + dx, y + dy);
        }
    }
    expect(ink.valid(), "Selected Radio must paint its compound-brand dot");
    expect(std::fabs((static_cast<float>(ink.left + ink.right) * 0.5f) -
                     centerX) <= 1.0f &&
               std::fabs((static_cast<float>(ink.top + ink.bottom) * 0.5f) -
                         centerY) <= 1.0f,
           "Selected Radio dot must remain centred at every DPI");
    expect(std::abs(ink.width() - ink.height()) <= 1,
           "Selected Radio dot must remain circular at every DPI");
    const int expectedDiameter =
        static_cast<int>(std::lround(10.0f * scale));
    expect(std::abs(ink.width() - expectedDiameter) <= 2,
           "Selected Radio dot must retain Fluent's 0.625 scale");
}

std::size_t sideInkMass(const std::vector<std::uint8_t>& rgba, int width, int height,
                        const InkBounds& bounds, int side)
{
    const int band = std::max(1, std::min(bounds.width(), bounds.height()) / 8);
    const int insetX = std::max(1, bounds.width() / 4);
    const int insetY = std::max(1, bounds.height() / 4);
    int left = bounds.left;
    int right = bounds.right;
    int top = bounds.top;
    int bottom = bounds.bottom;
    switch (side) {
    case 0: // top
        left += insetX; right -= insetX; bottom = std::min(bottom, top + band - 1); break;
    case 1: // right
        top += insetY; bottom -= insetY; left = std::max(left, right - band + 1); break;
    case 2: // bottom
        left += insetX; right -= insetX; top = std::max(top, bottom - band + 1); break;
    default: // left
        top += insetY; bottom -= insetY; right = std::min(right, left + band - 1); break;
    }
    std::size_t result = 0;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            if (isStrokeInk(rgba, width, height, x, y)) ++result;
        }
    }
    return result;
}

void verifyCheckboxFrame(const std::vector<std::uint8_t>& rgba, int width, int height, float scale)
{
    const float centerX = kCheckboxCenterX * scale;
    const float centerY = kIndicatorCenterY * scale;
    const InkBounds ink = findInkBounds(rgba, width, height, scale, {66, 26, 20, 20});
    expect(ink.valid(), "Unchecked Checkbox must paint a visible 16-DIP frame");
    expect(std::fabs((static_cast<float>(ink.left + ink.right) * 0.5f) - centerX) <= 1.0f
               && std::fabs((static_cast<float>(ink.top + ink.bottom) * 0.5f) - centerY) <= 1.0f,
           "Checkbox frame must remain centred at every DPI");
    expect(std::abs(ink.width() - ink.height()) <= 1,
           "Checkbox frame must retain equal logical width and height");

    const std::array<std::size_t, 4> sideMasses{
        sideInkMass(rgba, width, height, ink, 0), sideInkMass(rgba, width, height, ink, 1),
        sideInkMass(rgba, width, height, ink, 2), sideInkMass(rgba, width, height, ink, 3)};
    const auto [minimum, maximum] = std::minmax_element(sideMasses.begin(), sideMasses.end());
    expect(*minimum > 0 && *maximum - *minimum <= std::max<std::size_t>(2, *maximum / 4),
           "Checkbox frame must paint comparable ink on all four sides");

    std::size_t comparisons = 0;
    std::size_t horizontalMismatch = 0;
    std::size_t verticalMismatch = 0;
    const int mirrorX = static_cast<int>(std::lround(centerX * 2.0f));
    const int mirrorY = static_cast<int>(std::lround(centerY * 2.0f));
    for (int y = ink.top; y <= ink.bottom; ++y) {
        for (int x = ink.left; x <= ink.right; ++x) {
            const bool value = isStrokeInk(rgba, width, height, x, y);
            horizontalMismatch += value != isStrokeInk(rgba, width, height, mirrorX - x, y);
            verticalMismatch += value != isStrokeInk(rgba, width, height, x, mirrorY - y);
            ++comparisons;
        }
    }
    expect(comparisons > 0 && horizontalMismatch * 5 <= comparisons
               && verticalMismatch * 5 <= comparisons,
           "Checkbox frame corners and edges must be mirror-symmetric");

    // The immediate exterior must retain the page surface. This catches a
    // clipped frame that grows past its 16-DIP indicator box at fractional DPI.
    const auto background = wui::theme().colors.neutralBackground2.rest;
    expect(pixelIs(rgba, width, height, scale, 66, 26, background)
               && pixelIs(rgba, width, height, scale, 86, 46, background)
               && pixelIs(rgba, width, height, scale, 64, 36, background)
               && pixelIs(rgba, width, height, scale, 88, 36, background),
           "Checkbox frame must not clip or leak outside its 16-DIP indicator bounds");
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::string output = argc > 1 ? argv[1] : "fluent_indicator_geometry.ppm";
        const float scale = argc > 2 ? std::stof(argv[2]) : 1.0f;
        expect(scale >= 1.0f && std::isfinite(scale), "DPI scale must be finite and at least 1");
        const int width = static_cast<int>(std::lround(kLogicalWidth * scale));
        const int height = static_cast<int>(std::lround(kLogicalHeight * scale));
        auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
        expect(canvas && canvas->initializeContext(), "Software Canvas must initialize");
        // Match the native GLFW path: logical coordinates reach Canvas
        // unchanged and the Canvas root transform owns DPR. This is essential
        // because curve tessellation happens before that transform.
        canvas->setDevicePixelRatio(scale);
        wui::WhatsCanvasTextMeasurer measurer(*canvas, scale);
        wui::setTextMeasurer(&measurer);
        try {
            wui::PaintContext paint(*canvas, scale, true);
            canvas->beginFrame();
            paint.fillRect({0, 0, static_cast<float>(kLogicalWidth), static_cast<float>(kLogicalHeight)},
                           wui::theme().colors.neutralBackground2.rest);
            wui::Radio radio({}, false);
            wui::Radio selectedRadio({}, true);
            wui::Checkbox checkbox({}, false);
            draw(radio, paint, {20, 20, 32, 32});
            draw(checkbox, paint, {60, 20, 32, 32});
            draw(selectedRadio, paint, {20, 60, 32, 32});
            canvas->endFrame();

            const auto pixels = canvas->readPixelsRGBA();
            expect(pixels.size() == static_cast<std::size_t>(width * height * 4),
                   "Indicator geometry probe must capture a complete RGBA frame");
            // Retain artifacts on both success and failure for automatic visual
            // review; this happens before the assertions by design.
            savePpm(output, pixels, width, height);
            verifyRadioRing(pixels, width, height, scale);
            verifySelectedRadioDot(pixels, width, height, scale);
            verifyCheckboxFrame(pixels, width, height, scale);
        } catch (...) {
            wui::setTextMeasurer(nullptr);
            throw;
        }
        wui::setTextMeasurer(nullptr);
        return 0;
    } catch (const std::exception& error) {
        wui::setTextMeasurer(nullptr);
        std::cerr << "Fluent indicator geometry failure: " << error.what() << '\n';
        return 1;
    }
}
