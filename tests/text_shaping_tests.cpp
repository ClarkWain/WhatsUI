#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>

#include "wsc/Canvas.h"

#include "wui/widgets.h"
#include "wui/paint_context.h"
#include "wui/whatscanvas_text.h"

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool isValidUtf8(const std::string& text)
{
    std::size_t index = 0;
    while (index < text.size()) {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        if (first < 0x80) {
            ++index;
            continue;
        }
        std::size_t count = 0;
        std::uint32_t value = 0;
        if ((first & 0xE0u) == 0xC0u) { count = 2; value = first & 0x1Fu; }
        else if ((first & 0xF0u) == 0xE0u) { count = 3; value = first & 0x0Fu; }
        else if ((first & 0xF8u) == 0xF0u) { count = 4; value = first & 0x07u; }
        else return false;
        if (index + count > text.size()) return false;
        for (std::size_t offset = 1; offset < count; ++offset) {
            const unsigned char continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xC0u) != 0x80u) return false;
            value = (value << 6u) | (continuation & 0x3Fu);
        }
        const std::uint32_t minimum = count == 2 ? 0x80u : count == 3 ? 0x800u : 0x10000u;
        if (value < minimum || value > 0x10FFFFu || (value >= 0xD800u && value <= 0xDFFFu)) return false;
        index += count;
    }
    return true;
}

std::string utf8(std::initializer_list<std::uint32_t> codepoints)
{
    std::string result;
    for (const std::uint32_t codepoint : codepoints) {
        if (codepoint <= 0x7F) result.push_back(static_cast<char>(codepoint));
        else if (codepoint <= 0x7FF) {
            result.push_back(static_cast<char>(0xC0u | (codepoint >> 6u)));
            result.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        } else if (codepoint <= 0xFFFF) {
            result.push_back(static_cast<char>(0xE0u | (codepoint >> 12u)));
            result.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
            result.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        } else {
            result.push_back(static_cast<char>(0xF0u | (codepoint >> 18u)));
            result.push_back(static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3Fu)));
            result.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
            result.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        }
    }
    return result;
}

void testMultilingualLayoutAndCache()
{
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, 640, 360);
    expect(canvas && canvas->initializeContext(), "Software canvas must initialize");

    wui::WhatsCanvasTextMeasurer text(*canvas, 1.0f);
    const auto policy = text.installWindowsFallbackPolicy();
    expect(policy.defaultFallbackChain, "Default Windows fallback chain must be installed");
#if defined(_WIN32)
    if (std::filesystem::exists("C:/Windows/Fonts/seguiemj.ttf")) {
        expect(policy.emojiFallback, "Segoe UI Emoji must participate in the fallback policy");
    }
#endif

    const std::string sample = "Plan "
        + utf8({0x4F60, 0x597D, 0xFF0C, 0x4E16, 0x754C, 0x20, 0x1F44B, 0x20, 0x2014, 0x20,
                0x0645, 0x0631, 0x062D, 0x0628, 0x0627, 0x20, 0x0628, 0x0627, 0x0644,
                0x0639, 0x0627, 0x0644, 0x0645}) + " 123";
    const auto first = text.layoutText(sample, 18.0f, 92.0f, 24.0f, 0, false);
    expect(first.size() >= 3, "CJK and Arabic sample should wrap into several lines");
    for (const auto& line : first) {
        expect(isValidUtf8(line.text), "backend line layout must preserve valid UTF-8");
        expect(line.width <= 92.1f, "backend line layout must honor the logical width");
    }

    const auto beforeHit = text.cacheStats().hits;
    const auto second = text.layoutText(sample, 18.0f, 92.0f, 24.0f, 0, false);
    expect(second.size() == first.size(), "same layout key must resolve deterministically");
    expect(text.cacheStats().hits > beforeHit, "second identical layout must hit deterministic adapter cache");

    const auto emoji = text.measureText("Status: " + utf8({0x1F44B, 0x20, 0x2705}), 18.0f);
    expect(std::isfinite(emoji.width) && emoji.width > 0.0f,
           "emoji fallback input must have finite non-zero shaped metrics");
    const auto bidi = text.measureText("ABC " + utf8({0x0645, 0x0631, 0x062D, 0x0628, 0x0627}) + " 123", 18.0f);
    expect(std::isfinite(bidi.width) && bidi.width > 0.0f,
           "mixed bidi input must have finite non-zero shaped metrics");
}

void testTextUsesBackendLineBreaking()
{
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, 640, 360);
    expect(canvas && canvas->initializeContext(), "Software canvas must initialize");
    wui::WhatsCanvasTextMeasurer textMeasurer(*canvas);
    textMeasurer.installWindowsFallbackPolicy();
    wui::setTextMeasurer(&textMeasurer);

    wui::Text label(utf8({0x4E2D, 0x6587, 0x6CA1, 0x6709, 0x7A7A, 0x683C, 0x4E5F,
                          0x5FC5, 0x987B, 0x6309, 0x7167, 0x540E, 0x7AEF, 0x89C4,
                          0x5219, 0x6362, 0x884C}));
    label.setFontSize(18.0f);
    label.setWrap(wui::TextWrap::Word);
    const auto lines = label.resolvedLines(72.0f);
    expect(lines.size() >= 2, "Text must delegate constrained word wrapping to the backend provider");
    for (const auto& line : lines) expect(isValidUtf8(line), "Text must not split UTF-8 code units");

    wui::setTextMeasurer(nullptr);
}

void testCanvasDprOwnsNativeCoordinateScale()
{
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, 240, 120);
    expect(canvas && canvas->initializeContext(), "Software canvas must initialize for DPR bridge test");

    wui::PaintContext lateBoundContext(1.5f);
    lateBoundContext.setCanvas(*canvas);
    expect(std::fabs(lateBoundContext.canvasCoordinateScale() - 1.5f) < 0.001f,
           "A scale-only PaintContext must retain its coordinate scale when a Canvas is bound later");

    canvas->setDevicePixelRatio(1.5f);
    wui::PaintContext context(*canvas, 1.5f, true);
    expect(std::fabs(context.scaleFactor() - 1.5f) < 0.001f,
           "PaintContext must retain the window DPR for framework consumers");
    expect(std::fabs(context.canvasCoordinateScale() - 1.0f) < 0.001f,
           "Canvas DPR mode must forward logical coordinates without a second scale");

    // The measure/layout adapter also uses logical font sizes when Canvas
    // owns DPR; Canvas::drawText applies the physical raster scale itself.
    wui::WhatsCanvasTextMeasurer measurer(*canvas, 1.0f);
    const auto metrics = measurer.measureText("DirectWrite DPR", 14.0f);
    expect(metrics.width > 0.0f && std::isfinite(metrics.width),
           "logical-DPR text metrics must remain usable");

    const auto entriesBeforeWeights = measurer.cacheStats().entries;
    (void)measurer.measureText("Weighted heading", 16.0f, 400);
    const auto entriesAfterRegular = measurer.cacheStats().entries;
    (void)measurer.measureText("Weighted heading", 16.0f, 700);
    const auto entriesAfterBold = measurer.cacheStats().entries;
    expect(entriesAfterRegular == entriesBeforeWeights + 1
               && entriesAfterBold == entriesAfterRegular + 1,
           "text measurement cache must distinguish font weights");
}

} // namespace

int main()
{
    try {
        testMultilingualLayoutAndCache();
        testTextUsesBackendLineBreaking();
        testCanvasDprOwnsNativeCoordinateScale();
        std::cout << "WhatsUI text shaping tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WhatsUI text shaping tests failed: " << error.what() << '\n';
        return 1;
    }
}
