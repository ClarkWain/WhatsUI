// Windows-native text regression.
//
// Ordinary Fluent visual tests intentionally use WhatsCanvas' portable
// Software text backend: they are deterministic layout/pixel checks, not a
// statement about the native desktop rasterizer. This test explicitly selects
// the DirectWrite backend and drives it through the same Canvas-DPR ownership
// contract used by GlfwPlatformWindow. It catches three classes of regression:
//   * silently falling back to the portable font backend on Windows;
//   * changing the Fluent default family away from resolvable Segoe UI; and
//   * applying the monitor DPI once in Canvas and again in PaintContext.

// Keep Win32's legacy min/max macros out of the shared geometry headers.
// This matches the GLFW backend's include policy.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wsc/Paint.h"

#include "wui/paint_context.h"
#include "wui/whatscanvas_text.h"
#include "wui/widgets.h"

// Canvas and WhatsUI both expose identifiers such as WINDING and `small`.
// Include every shared UI header before Win32 so legacy GDI/RPC macros cannot
// rewrite declarations while a header is parsed.
#include <windows.h>
#include <dwrite.h>

namespace {

constexpr float kLogicalWidth = 320.0f;
constexpr float kLogicalHeight = 100.0f;
constexpr float kTextX = 24.0f;
constexpr float kTextY = 28.0f;
constexpr float kTextSize = 14.0f;
constexpr char kFamily[] = "Segoe UI";
constexpr char kProbe[] = "Text DPI";

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
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

InkBounds findInk(const std::vector<std::uint8_t>& pixels, int width, int height)
{
    InkBounds result;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto offset = static_cast<std::size_t>((y * width + x) * 4);
            // DirectWrite grayscale antialiasing keeps all RGB channels equal;
            // use a generous luma threshold so fractional-DPI edge pixels
            // participate without sampling the white backdrop.
            const int luma = (static_cast<int>(pixels[offset]) * 30 +
                              static_cast<int>(pixels[offset + 1]) * 59 +
                              static_cast<int>(pixels[offset + 2]) * 11) / 100;
            if (luma >= 235) continue;
            result.left = std::min(result.left, x);
            result.top = std::min(result.top, y);
            result.right = std::max(result.right, x);
            result.bottom = std::max(result.bottom, y);
        }
    }
    return result;
}

void verifySegoeUiIsInstalled()
{
    IDWriteFactory* factory = nullptr;
    const HRESULT factoryResult = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&factory));
    expect(SUCCEEDED(factoryResult) && factory != nullptr,
           "Windows DirectWrite factory must initialize for native text verification");

    IDWriteFontCollection* collection = nullptr;
    const HRESULT collectionResult = factory->GetSystemFontCollection(&collection, FALSE);
    expect(SUCCEEDED(collectionResult) && collection != nullptr,
           "Windows DirectWrite system font collection must be available");
    UINT32 index = 0;
    BOOL exists = FALSE;
    const HRESULT lookupResult = collection->FindFamilyName(L"Segoe UI", &index, &exists);
    collection->Release();
    factory->Release();
    expect(SUCCEEDED(lookupResult) && exists,
           "Fluent Windows default family Segoe UI must resolve through DirectWrite");
}

struct NativeRender {
    float scale{1.0f};
    float logicalMetricWidth{0.0f};
    InkBounds ink;
};

NativeRender renderNativeText(float scale)
{
    const int width = static_cast<int>(std::lround(kLogicalWidth * scale));
    const int height = static_cast<int>(std::lround(kLogicalHeight * scale));
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
    expect(canvas && canvas->initializeContext(), "Native text probe must initialize a WhatsCanvas Software surface");
    expect(canvas->setTextBackend(wsc::Canvas::TextBackend::DirectWrite,
                                  wsc::Canvas::TextRenderMode::Grayscale) &&
               canvas->textBackend() == wsc::Canvas::TextBackend::DirectWrite,
           "Windows native text probe must use an active DirectWrite backend, never a silent portable fallback");

    canvas->setDevicePixelRatio(scale);
    const auto mapped = canvas->mapPoint(wsc::PointF(kTextX, 0.0f));
    expect(std::fabs(mapped.getX() - kTextX * scale) < 0.01f,
           "Canvas DPR must map logical coordinates to exactly one physical DPI multiplier");

    wui::PaintContext context(*canvas, scale, true);
    expect(std::fabs(context.scaleFactor() - scale) < 0.0001f &&
               std::fabs(context.canvasCoordinateScale() - 1.0f) < 0.0001f,
           "Native Canvas DPR mode must retain monitor scale while forwarding logical coordinates exactly once");

    wsc::Paint nativePaint;
    nativePaint.setFontFamily(kFamily);
    nativePaint.setFontWeight(600);
    nativePaint.setTextSize(kTextSize);
    expect(nativePaint.hasFontFamily() && nativePaint.getFontFamily() == kFamily,
           "Native text probe must request the Fluent Segoe UI family explicitly");
    const float logicalMetricWidth = canvas->measureTextMetrics(kProbe, nativePaint).width;
    expect(logicalMetricWidth > 0.0f && std::isfinite(logicalMetricWidth),
           "DirectWrite must report nonzero logical Segoe UI text metrics");

    wui::WhatsCanvasTextMeasurer measurer(*canvas, 1.0f);
    wui::setTextMeasurer(&measurer);
    try {
        wui::Text text(kProbe);
        text.setFontFamily(kFamily);
        text.setFontSize(kTextSize);
        text.setFontWeight(600);
        text.setColor({0, 0, 0, 255});
        text.layout({kTextX, kTextY, 220.0f, 30.0f});

        canvas->beginFrame();
        context.fillRect({0.0f, 0.0f, kLogicalWidth, kLogicalHeight}, {255, 255, 255, 255});
        text.prepare(context);
        text.paint(context);
        canvas->endFrame();
        wui::setTextMeasurer(nullptr);
    } catch (...) {
        wui::setTextMeasurer(nullptr);
        throw;
    }

    std::vector<std::uint8_t> pixels;
    expect(canvas->readPixelsRGBA(pixels) && pixels.size() == static_cast<std::size_t>(width * height * 4),
           "Native text probe must read the physical DirectWrite output");
    const InkBounds ink = findInk(pixels, width, height);
    expect(ink.valid(), "DirectWrite Segoe UI probe must paint visible text");

    // A second scale in PaintContext would move this to roughly x * scale^2.
    // Allow a small glyph-side-bearing/hinting envelope, but not a second DPI
    // multiplier even at 125% where it previously looked deceptively close.
    expect(std::abs(static_cast<float>(ink.left) - kTextX * scale) <= 4.0f,
           "Native text ink origin must be monitor-scaled once, not double-scaled");
    return {scale, logicalMetricWidth, ink};
}

void testNativeDirectWriteDpiContract()
{
    verifySegoeUiIsInstalled();
    const float scales[] = {1.0f, 1.25f, 1.5f, 2.0f};
    std::vector<NativeRender> renders;
    renders.reserve(4);
    for (const float scale : scales) renders.push_back(renderNativeText(scale));

    const NativeRender& baseline = renders.front();
    expect(baseline.ink.width() > 0 && baseline.ink.height() > 0,
           "100% native DirectWrite reference must contain text ink");
    for (const auto& render : renders) {
        // Canvas measurement remains in logical DIPs. If the layout adapter
        // also applied DPR, this would grow with monitor scale and controls
        // would wrap earlier on 125%/150% desktops.
        expect(std::fabs(render.logicalMetricWidth - baseline.logicalMetricWidth) < 0.05f,
               "DirectWrite logical text metrics must not scale with the physical framebuffer");

        const float expectedWidth = static_cast<float>(baseline.ink.width()) * render.scale;
        const float expectedHeight = static_cast<float>(baseline.ink.height()) * render.scale;
        // Hinting changes individual edge coverage at fractional scales, so
        // compare envelopes. A second scale would exceed this tolerance by a
        // wide margin (1.25^2, 1.5^2, 2^2) while normal hinting remains inside.
        expect(std::fabs(static_cast<float>(render.ink.width()) - expectedWidth) <= 5.0f,
               "DirectWrite text width must track the physical DPI ratio exactly once");
        expect(std::fabs(static_cast<float>(render.ink.height()) - expectedHeight) <= 5.0f,
               "DirectWrite text height must track the physical DPI ratio exactly once");

        std::cout << "[WindowsNativeTextDpi] scale=" << render.scale
                  << " logicalWidth=" << render.logicalMetricWidth
                  << " ink=" << render.ink.width() << "x" << render.ink.height()
                  << " at " << render.ink.left << "," << render.ink.top << '\n';
    }
}

} // namespace

int main()
{
    try {
        testNativeDirectWriteDpiContract();
        std::cout << "Windows native DirectWrite DPI tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Windows native DirectWrite DPI test failure: " << error.what() << '\n';
        return 1;
    }
}
