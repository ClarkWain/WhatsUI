#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"

#include "wui/basic_controls.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {

constexpr int kLogicalWidth = 900;
constexpr int kLogicalHeight = 196;
constexpr float kUncheckedY = 54.0f;
constexpr float kCheckedY = 126.0f;
constexpr std::array<float, 6> kColumns{
    88.0f, 222.0f, 356.0f, 490.0f, 624.0f, 758.0f};

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void savePpm(const std::string& path, const std::vector<std::uint8_t>& rgba,
             int width, int height)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot create Fluent Radio state artifact");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t index = 0; index + 3 < rgba.size(); index += 4) {
        output.put(static_cast<char>(rgba[index]));
        output.put(static_cast<char>(rgba[index + 1]));
        output.put(static_cast<char>(rgba[index + 2]));
    }
}

std::size_t pixelOffset(int width, float scale, float logicalX, float logicalY)
{
    const int x = static_cast<int>(std::lround(logicalX * scale));
    const int y = static_cast<int>(std::lround(logicalY * scale));
    return static_cast<std::size_t>((y * width + x) * 4);
}

bool pixelIs(const std::vector<std::uint8_t>& pixels, int width, float scale,
             float logicalX, float logicalY, wui::Color expected)
{
    const std::size_t offset = pixelOffset(width, scale, logicalX, logicalY);
    return offset + 3 < pixels.size() && pixels[offset] == expected.r
        && pixels[offset + 1] == expected.g && pixels[offset + 2] == expected.b
        && pixels[offset + 3] == expected.a;
}

bool annularGapContains(const std::vector<std::uint8_t>& pixels, int width, int height,
                        float scale, float logicalCenterX, float logicalCenterY,
                        wui::Color background)
{
    const int centerX = static_cast<int>(std::lround(logicalCenterX * scale));
    const int centerY = static_cast<int>(std::lround(logicalCenterY * scale));
    const int radius = static_cast<int>(std::ceil(7.0f * scale));
    std::array<bool, 4> quadrantHasGap{};
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const float logicalRadius =
                std::sqrt(static_cast<float>(dx * dx + dy * dy)) / scale;
            if (logicalRadius < 5.25f || logicalRadius > 6.75f) continue;
            const int x = centerX + dx;
            const int y = centerY + dy;
            if (x < 0 || y < 0 || x >= width || y >= height) continue;
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            if (pixels[offset] != background.r || pixels[offset + 1] != background.g
                || pixels[offset + 2] != background.b || pixels[offset + 3] != background.a) {
                continue;
            }
            const std::size_t quadrant =
                static_cast<std::size_t>((dy >= 0 ? 2 : 0) + (dx >= 0 ? 1 : 0));
            quadrantHasGap[quadrant] = true;
        }
    }
    return std::all_of(quadrantHasGap.begin(), quadrantHasGap.end(),
                       [](bool hasGap) { return hasGap; });
}

bool annularStrokeContains(const std::vector<std::uint8_t>& pixels, int width, int height,
                           float scale, float logicalCenterX, float logicalCenterY,
                           wui::Color expected)
{
    const int centerX = static_cast<int>(std::lround(logicalCenterX * scale));
    const int centerY = static_cast<int>(std::lround(logicalCenterY * scale));
    const int radius = static_cast<int>(std::ceil(8.5f * scale));
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const float logicalRadius =
                std::sqrt(static_cast<float>(dx * dx + dy * dy)) / scale;
            if (logicalRadius < 7.0f || logicalRadius > 8.5f) continue;
            const int x = centerX + dx;
            const int y = centerY + dy;
            if (x < 0 || y < 0 || x >= width || y >= height) continue;
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            const auto near = [actual = &pixels[offset]](wui::Color target) {
                constexpr int kTolerance = 24;
                return std::abs(static_cast<int>(actual[0]) - target.r) <= kTolerance
                    && std::abs(static_cast<int>(actual[1]) - target.g) <= kTolerance
                    && std::abs(static_cast<int>(actual[2]) - target.b) <= kTolerance
                    && std::abs(static_cast<int>(actual[3]) - target.a) <= kTolerance;
            };
            if (near(expected)) return true;
        }
    }
    return false;
}

void configureState(wui::Radio& radio, std::size_t column)
{
    switch (column) {
    case 1:
        radio.setVisualState(wui::ControlVisualState::Hovered, true);
        break;
    case 2:
        radio.setVisualState(wui::ControlVisualState::Hovered, true);
        radio.setVisualState(wui::ControlVisualState::Pressed, true);
        break;
    case 3:
        radio.setVisualState(wui::ControlVisualState::Focused, true);
        break;
    case 4:
        radio.setVisualState(wui::ControlVisualState::Focused, true);
        radio.setVisualState(wui::ControlVisualState::FocusVisible, true);
        break;
    case 5:
        radio.setEnabled(false);
        break;
    default:
        break;
    }
}

void paintRadio(wui::PaintContext& paint, float x, float y,
                const char* label, bool checked, std::size_t column)
{
    wui::Radio radio(label, checked);
    configureState(radio, column);
    radio.layout({x, y, 118.0f, 32.0f});
    radio.prepare(paint);
    radio.paint(paint);
}

void verifyStatePixels(const std::vector<std::uint8_t>& pixels,
                       int width, int height, float scale)
{
    const auto& colors = wui::theme().colors;
    const std::array<wui::Color, 6> uncheckedBorders{
        colors.neutralStrokeAccessible,
        colors.neutralStrokeAccessibleHover,
        colors.neutralStrokeAccessiblePressed,
        colors.neutralStrokeAccessible,
        colors.neutralStrokeAccessible,
        colors.neutralStrokeDisabled};
    const std::array<wui::Color, 6> checkedDots{
        colors.compoundBrandForeground1.rest,
        colors.compoundBrandForeground1.hover,
        colors.compoundBrandForeground1.pressed,
        colors.compoundBrandForeground1.rest,
        colors.compoundBrandForeground1.rest,
        colors.neutralForegroundDisabled};
    const std::array<wui::Color, 6> checkedBorders{
        colors.compoundBrandStroke.rest,
        colors.compoundBrandStroke.hover,
        colors.compoundBrandStroke.pressed,
        colors.compoundBrandStroke.rest,
        colors.compoundBrandStroke.rest,
        colors.neutralStrokeDisabled};

    for (std::size_t column = 0; column < kColumns.size(); ++column) {
        const float centerX = kColumns[column] + 16.0f;
        const float uncheckedCenterY = kUncheckedY + 16.0f;
        const float checkedCenterY = kCheckedY + 16.0f;

        // Unchecked radios are transparent inside and use only the accessible
        // neutral outline for the current interaction state.
        expect(pixelIs(pixels, width, scale, centerX, uncheckedCenterY,
                       colors.neutralBackground2.rest),
               "Unchecked Radio centre must remain transparent");
        expect(annularStrokeContains(pixels, width, height, scale,
                                     centerX, uncheckedCenterY, uncheckedBorders[column]),
               "Unchecked Radio border must use its Fluent interaction token");

        // Checked radios use a 10-DIP compound-brand centre dot, a transparent
        // annular gap, and a separate 1-DIP compound-brand outer stroke.
        expect(pixelIs(pixels, width, scale, centerX, checkedCenterY,
                       checkedDots[column]),
               "Checked Radio centre must use the compound-brand foreground token");
        expect(annularGapContains(pixels, width, height, scale, centerX, checkedCenterY,
                                  colors.neutralBackground2.rest),
               "Checked Radio must retain a transparent gap between dot and border");
        expect(annularStrokeContains(pixels, width, height, scale,
                                     centerX, checkedCenterY, checkedBorders[column]),
               "Checked Radio border must use the compound-brand stroke token");
    }

    const float pointerFocusEdgeX = kColumns[3] - 2.0f;
    const float keyboardFocusEdgeX = kColumns[4] - 2.0f;
    expect(pixelIs(pixels, width, scale, pointerFocusEdgeX, kUncheckedY + 16.0f,
                   colors.neutralBackground2.rest)
               && pixelIs(pixels, width, scale, pointerFocusEdgeX, kCheckedY + 16.0f,
                          colors.neutralBackground2.rest),
           "Pointer-focused Radio must not paint the keyboard focus rectangle");
    expect(!pixelIs(pixels, width, scale, keyboardFocusEdgeX, kUncheckedY + 16.0f,
                    colors.neutralBackground2.rest)
               && !pixelIs(pixels, width, scale, keyboardFocusEdgeX, kCheckedY + 16.0f,
                           colors.neutralBackground2.rest),
           "Keyboard-focused Radio must retain the Fluent focus-visible rectangle");
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::string output = argc > 1 ? argv[1] : "fluent_radio_states.ppm";
        const float scale = argc > 2 ? std::max(1.0f, std::stof(argv[2])) : 1.0f;
        const int width = static_cast<int>(std::lround(kLogicalWidth * scale));
        const int height = static_cast<int>(std::lround(kLogicalHeight * scale));
        auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
        expect(canvas && canvas->initializeContext(), "Software Canvas must initialize");
        wui::WhatsCanvasTextMeasurer measurer(*canvas, scale);
        wui::setTextMeasurer(&measurer);
        try {
            wui::PaintContext paint(*canvas, scale);
            canvas->beginFrame();
            paint.fillRect({0, 0, static_cast<float>(kLogicalWidth),
                            static_cast<float>(kLogicalHeight)},
                           wui::theme().colors.neutralBackground2.rest);
            paint.drawText("Unchecked", 20, 74, 14,
                           wui::theme().colors.neutralForeground1, 600);
            paint.drawText("Checked", 20, 146, 14,
                           wui::theme().colors.neutralForeground1, 600);
            const std::array<const char*, 6> labels{
                "Rest", "Hover", "Pressed", "Pointer focus", "Keyboard focus", "Disabled"};
            for (std::size_t column = 0; column < kColumns.size(); ++column) {
                paintRadio(paint, kColumns[column], kUncheckedY,
                           labels[column], false, column);
                paintRadio(paint, kColumns[column], kCheckedY,
                           labels[column], true, column);
            }
            canvas->endFrame();
            const auto pixels = canvas->readPixelsRGBA();
            expect(pixels.size() == static_cast<std::size_t>(width * height * 4),
                   "Radio state capture must contain a complete RGBA frame");
            savePpm(output, pixels, width, height);
            verifyStatePixels(pixels, width, height, scale);
        } catch (...) {
            wui::setTextMeasurer(nullptr);
            throw;
        }
        wui::setTextMeasurer(nullptr);
        return 0;
    } catch (const std::exception& error) {
        wui::setTextMeasurer(nullptr);
        std::cerr << "Fluent Radio visual failure: " << error.what() << '\n';
        return 1;
    }
}
