#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"

#include "wui/animation.h"
#include "wui/paint_context.h"
#include "wui/text_input.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {

constexpr int kLogicalWidth = 760;
constexpr int kLogicalHeight = 430;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

struct PixelBounds {
    int left{0};
    int top{0};
    int right{-1};
    int bottom{-1};

    [[nodiscard]] bool valid() const noexcept
    {
        return right >= left && bottom >= top;
    }

    [[nodiscard]] int width() const noexcept { return right - left + 1; }
    [[nodiscard]] int height() const noexcept { return bottom - top + 1; }
    [[nodiscard]] float centerY() const noexcept
    {
        return (static_cast<float>(top) + static_cast<float>(bottom)) * 0.5f;
    }
};

PixelBounds findColor(const std::vector<unsigned char>& pixels, int width,
                      int height, float scale, wui::RectF logicalRegion,
                      wui::Color color, int tolerance = 0)
{
    const int left = std::clamp(
        static_cast<int>(std::floor(logicalRegion.x * scale)), 0, width);
    const int top = std::clamp(
        static_cast<int>(std::floor(logicalRegion.y * scale)), 0, height);
    const int right = std::clamp(
        static_cast<int>(std::ceil(
            (logicalRegion.x + logicalRegion.width) * scale)),
        0, width);
    const int bottom = std::clamp(
        static_cast<int>(std::ceil(
            (logicalRegion.y + logicalRegion.height) * scale)),
        0, height);

    PixelBounds result;
    result.left = right;
    result.top = bottom;
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const auto offset =
                static_cast<std::size_t>((y * width + x) * 4);
            if (offset + 3 >= pixels.size()) continue;
            if (std::abs(static_cast<int>(pixels[offset]) - color.r) <=
                    tolerance &&
                std::abs(static_cast<int>(pixels[offset + 1]) - color.g) <=
                    tolerance &&
                std::abs(static_cast<int>(pixels[offset + 2]) - color.b) <=
                    tolerance &&
                std::abs(static_cast<int>(pixels[offset + 3]) - color.a) <=
                    tolerance) {
                result.left = std::min(result.left, x);
                result.top = std::min(result.top, y);
                result.right = std::max(result.right, x);
                result.bottom = std::max(result.bottom, y);
            }
        }
    }
    return result;
}

void savePpm(const std::string& path,
             const std::vector<unsigned char>& pixels, int width, int height)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot create Input visual artifact");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t offset = 0; offset + 3 < pixels.size(); offset += 4) {
        output.put(static_cast<char>(pixels[offset]));
        output.put(static_cast<char>(pixels[offset + 1]));
        output.put(static_cast<char>(pixels[offset + 2]));
    }
}

void draw(wui::TextInput& input, wui::PaintContext& paint,
          const wui::RectF& bounds)
{
    input.layout(bounds);
    input.prepare(paint);
    input.paint(paint);
}

void renderAndVerify(const std::string& outputPath, float scale)
{
    wui::Ticker::instance().cancelAll();
    const int width =
        static_cast<int>(std::lround(kLogicalWidth * scale));
    const int height =
        static_cast<int>(std::lround(kLogicalHeight * scale));
    auto canvas =
        wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
    expect(canvas && canvas->initializeContext(),
           "Software Canvas must initialize for Input review");
    wui::WhatsCanvasTextMeasurer measurer(*canvas, scale);
    wui::setTextMeasurer(&measurer);

    try {
        wui::PaintContext paint(*canvas, scale);
        const auto& current = wui::theme();
        canvas->beginFrame();
        paint.fillRect(
            {0, 0, static_cast<float>(kLogicalWidth),
             static_cast<float>(kLogicalHeight)},
            current.colors.neutralBackground2.rest);
        paint.drawText("Fluent Input and Textarea", 32, 38, 22,
                       current.colors.neutralForeground1, 600);

        wui::TextInput rest("Rest");
        draw(rest, paint, {32, 64, 300, 32});

        wui::TextInput hover("Hover");
        hover.setVisualState(wui::ControlVisualState::Hovered, true);
        draw(hover, paint, {380, 64, 300, 32});

        wui::TextInput focusIn("Focus animation");
        focusIn.setVisualState(wui::ControlVisualState::Focused, true);
        draw(focusIn, paint, {32, 128, 648, 32});
        // 50 ms is one quarter of Fluent durationNormal. The decelerating
        // scaleX animation must already cover the centre, not the ends.
        wui::Ticker::instance().tick(0.05f);
        draw(focusIn, paint, {32, 128, 648, 32});

        wui::TextInput focused;
        focused.text("Search");
        focused.controller().setCaret(0);
        focused.setMotionEnabled(false);
        focused.setVisualState(wui::ControlVisualState::Focused, true);
        draw(focused, paint, {32, 192, 648, 32});

        wui::TextArea areaRest("Textarea placeholder");
        draw(areaRest, paint, {32, 256, 300, 96});

        wui::TextArea areaFocused;
        areaFocused.text("Notes");
        areaFocused.controller().setCaret(0);
        areaFocused.setMotionEnabled(false);
        areaFocused.setVisualState(wui::ControlVisualState::Focused, true);
        draw(areaFocused, paint, {380, 256, 300, 96});

        wui::TextInput disabled("Disabled");
        disabled.setEnabled(false);
        draw(disabled, paint, {32, 384, 300, 32});

        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        expect(pixels.size() ==
                   static_cast<std::size_t>(width * height * 4),
               "Input visual capture must return complete RGBA");
        savePpm(outputPath, pixels, width, height);

        const auto restTop = findColor(
            pixels, width, height, scale, {40, 63.5f, 284, 3},
            current.colors.neutralStroke1, 3);
        const auto restBottom = findColor(
            pixels, width, height, scale, {40, 92, 284, 5},
            current.colors.neutralStrokeAccessible, 3);
        expect(restTop.valid() && restBottom.valid(),
               "Resting outline Input must use a grey frame and accessible dark bottom stroke");
        expect(restBottom.height() ==
                   std::max(1, static_cast<int>(std::lround(scale))),
               "Resting bottom stroke must occupy an integer physical-pixel thickness");

        const auto hoverTop = findColor(
            pixels, width, height, scale, {388, 63.5f, 284, 3},
            current.colors.neutralStroke1Hover, 3);
        const auto hoverBottom = findColor(
            pixels, width, height, scale, {388, 92, 284, 5},
            current.colors.neutralStrokeAccessibleHover, 3);
        expect(hoverTop.valid() && hoverBottom.valid(),
               "Hovered Input must resolve both Fluent neutral stroke aliases");

        const auto midBrand = findColor(
            pixels, width, height, scale, {32, 156, 648, 4},
            current.colors.compoundBrandStroke.rest, 3);
        const auto midBaseLeft = findColor(
            pixels, width, height, scale, {34, 157, 100, 3},
            current.colors.neutralStrokeAccessiblePressed, 3);
        const int fullPhysicalWidth =
            static_cast<int>(std::lround(648.0f * scale));
        expect(midBrand.valid() && midBaseLeft.valid() &&
                   midBrand.width() > fullPhysicalWidth * 45 / 100 &&
                   midBrand.width() < fullPhysicalWidth * 75 / 100,
               "Focus indicator must expand from the centre during its 200 ms transition");

        const auto fullBrand = findColor(
            pixels, width, height, scale, {32, 220, 648, 4},
            current.colors.compoundBrandStroke.rest, 3);
        expect(fullBrand.valid() &&
                   fullBrand.width() > fullPhysicalWidth * 90 / 100,
               "Focused Input must finish with a full-width 2-DIP brand bottom stroke");
        expect(fullBrand.height() ==
                   std::max(1, static_cast<int>(
                                   std::lround(2.0f * scale))),
               "Focused Input bottom stroke must occupy a crisp integer physical-pixel thickness");

        const auto inputCaret = findColor(
            pixels, width, height, scale, {42, 195, 6, 26},
            current.colors.brandForeground1, 1);
        expect(inputCaret.valid() && inputCaret.width() == 1,
               "Input caret must snap to exactly one physical pixel");
        expect(inputCaret.height() ==
                   static_cast<int>(std::lround(
                       current.typography.body1.lineHeight * scale)),
               "Input caret height must match the 20-DIP text line, not the complete field");
        const float expectedInputCentre =
            (192.0f + 16.0f) * scale - 0.5f;
        expect(std::abs(inputCaret.centerY() - expectedInputCentre) <=
                   0.5f,
               "Input caret must be vertically centred with the text line");

        const auto areaCaret = findColor(
            pixels, width, height, scale, {390, 259, 8, 28},
            current.colors.brandForeground1, 1);
        expect(areaCaret.valid() && areaCaret.width() == 1 &&
                   areaCaret.height() ==
                       static_cast<int>(std::lround(
                           current.typography.body1.lineHeight * scale)),
               "Textarea caret must use one physical pixel and one text-line height");
        const float expectedAreaTop = (256.0f +
            current.spacing.vertical.sNudge) * scale;
        expect(std::abs(static_cast<float>(areaCaret.top) -
                        expectedAreaTop) <= 0.5f,
               "Medium Textarea content and caret must use the 6-DIP Fluent top padding");

        expect(std::abs(focused.caretRect().height -
                        current.typography.body1.lineHeight) <= 0.01f &&
                   std::abs(areaFocused.caretRect().height -
                            current.typography.body1.lineHeight) <= 0.01f,
               "Public IME caret rectangles must match painted line-height carets");
    } catch (...) {
        wui::Ticker::instance().cancelAll();
        wui::setTextMeasurer(nullptr);
        throw;
    }
    wui::Ticker::instance().cancelAll();
    wui::setTextMeasurer(nullptr);
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::string output =
            argc > 1 ? argv[1] : "fluent_input_textarea_review.ppm";
        const float scale =
            argc > 2 ? std::max(1.0f, std::stof(argv[2])) : 1.0f;
        renderAndVerify(output, scale);
        std::cout << "Fluent Input/Textarea visual tests passed at "
                  << scale << "x: " << output << '\n';
        return 0;
    } catch (const std::exception& error) {
        wui::Ticker::instance().cancelAll();
        wui::setTextMeasurer(nullptr);
        std::cerr << "Fluent Input/Textarea visual tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
