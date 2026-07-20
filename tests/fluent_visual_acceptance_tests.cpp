#include <algorithm>
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
#include "wui/text_input.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"
#include "wui/widgets.h"

namespace {

constexpr int kLogicalWidth = 600;
constexpr int kLogicalHeight = 304;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void expectNear(float actual, float expected, float tolerance, const char* rule)
{
    if (std::fabs(actual - expected) <= tolerance + 0.0001f) return;
    throw std::runtime_error(std::string(rule) + ": actual=" + std::to_string(actual)
                             + ", expected=" + std::to_string(expected)
                             + ", tolerance=" + std::to_string(tolerance));
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
    if (!output) throw std::runtime_error("cannot create Fluent visual acceptance artifact");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t index = 0; index + 3 < rgba.size(); index += 4) {
        output.put(static_cast<char>(rgba[index]));
        output.put(static_cast<char>(rgba[index + 1]));
        output.put(static_cast<char>(rgba[index + 2]));
    }
}

bool pixelIs(const std::vector<std::uint8_t>& rgba, int width, float scale,
             float logicalX, float logicalY, wui::Color color)
{
    const int x = static_cast<int>(std::lround(logicalX * scale));
    const int y = static_cast<int>(std::lround(logicalY * scale));
    if (x < 0 || y < 0 || x >= width) return false;
    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
    return offset + 3 < rgba.size() && rgba[offset] == color.r
        && rgba[offset + 1] == color.g && rgba[offset + 2] == color.b
        && rgba[offset + 3] == color.a;
}

bool pixelNear(const std::vector<std::uint8_t>& rgba, int width, float scale,
               float logicalX, float logicalY, wui::Color color, int tolerance)
{
    const int x = static_cast<int>(std::lround(logicalX * scale));
    const int y = static_cast<int>(std::lround(logicalY * scale));
    if (x < 0 || y < 0 || x >= width) return false;
    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
    const auto near = [tolerance](std::uint8_t actual, std::uint8_t expected) {
        return std::abs(static_cast<int>(actual) - static_cast<int>(expected)) <= tolerance;
    };
    return offset + 3 < rgba.size() && near(rgba[offset], color.r)
        && near(rgba[offset + 1], color.g) && near(rgba[offset + 2], color.b)
        && near(rgba[offset + 3], color.a);
}

struct InkBounds {
    int top{std::numeric_limits<int>::max()};
    int bottom{-1};
    int left{std::numeric_limits<int>::max()};

    [[nodiscard]] bool valid() const noexcept { return bottom >= top && left != std::numeric_limits<int>::max(); }
    [[nodiscard]] float centerY(float scale) const noexcept
    {
        return valid() ? static_cast<float>(top + bottom) * 0.5f / scale : 0.0f;
    }
    [[nodiscard]] float leftLogical(float scale) const noexcept
    {
        return valid() ? static_cast<float>(left) / scale : 0.0f;
    }
};

template <typename Predicate>
InkBounds findInk(const std::vector<std::uint8_t>& rgba, int width, int height, float scale,
                  wui::RectF logicalRegion, Predicate predicate)
{
    const int left = std::clamp(static_cast<int>(std::lround(logicalRegion.x * scale)), 0, width);
    const int top = std::clamp(static_cast<int>(std::lround(logicalRegion.y * scale)), 0, height);
    const int right = std::clamp(static_cast<int>(std::lround(
        (logicalRegion.x + logicalRegion.width) * scale)), 0, width);
    const int bottom = std::clamp(static_cast<int>(std::lround(
        (logicalRegion.y + logicalRegion.height) * scale)), 0, height);
    InkBounds result;
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const auto offset = static_cast<std::size_t>((y * width + x) * 4);
            if (!predicate(rgba[offset], rgba[offset + 1], rgba[offset + 2], rgba[offset + 3])) {
                continue;
            }
            result.top = std::min(result.top, y);
            result.bottom = std::max(result.bottom, y);
            result.left = std::min(result.left, x);
        }
    }
    return result;
}

void verifyTextGeometry(const std::vector<std::uint8_t>& pixels, int width, int height, float scale)
{
    // These regions deliberately exclude borders, carets and focus indicators.
    // The acceptance band is one physical pixel at every tested DPR. This is
    // intentionally stricter than a DIP-based tolerance: at 200% a half-DIP
    // displacement is already one complete output pixel.
    const float onePhysicalPixel = 1.0f / scale;
    const auto whiteInk = [](std::uint8_t red, std::uint8_t green, std::uint8_t blue,
                             std::uint8_t /*alpha*/) {
        return red >= 245 && green >= 245 && blue >= 245;
    };
    const auto darkInk = [](std::uint8_t red, std::uint8_t green, std::uint8_t blue,
                            std::uint8_t /*alpha*/) {
        return red <= 105 && green <= 105 && blue <= 105;
    };
    const auto mutedInk = [](std::uint8_t red, std::uint8_t green, std::uint8_t blue,
                             std::uint8_t /*alpha*/) {
        return red <= 180 && green <= 180 && blue <= 180;
    };

    const InkBounds buttonInk = findInk(pixels, width, height, scale, {38, 32, 92, 24}, whiteInk);
    expect(buttonInk.valid(), "Button label must produce visible ink");
    expectNear(buttonInk.centerY(scale), 44.0f, onePhysicalPixel,
               "Button label ink must be vertically centred within one physical pixel");

    const InkBounds labelInk = findInk(pixels, width, height, scale, {292, 88, 160, 24}, darkInk);
    expect(labelInk.valid(), "Label must produce visible ink");
    expectNear(labelInk.centerY(scale), 100.0f, onePhysicalPixel,
               "Label ink must be vertically centred within one physical pixel");

    const InkBounds placeholderInk = findInk(pixels, width, height, scale, {30, 88, 220, 24}, mutedInk);
    expect(placeholderInk.valid(), "Input placeholder must produce visible ink");
    expectNear(placeholderInk.centerY(scale), 100.0f, onePhysicalPixel,
               "Input placeholder ink must be vertically centred within one physical pixel");
    // The deterministic default face has a one-DIP left side-bearing for 'P'.
    // Checking the rendered first ink pixel as well as caretRect() below catches
    // both an incorrect content inset and an accidental extra application DPR.
    const float expectedLeftInk = 24.0f + wui::theme().controls.horizontalPadding + 1.0f;
    expectNear(placeholderInk.leftLogical(scale), expectedLeftInk, onePhysicalPixel,
               "Input placeholder must preserve the Fluent inset and glyph side-bearing");
}

void verifyInputPaddingGeometry()
{
    const wui::RectF bounds{24.0f, 84.0f, 240.0f, 32.0f};
    wui::TextInput input;
    input.layout(bounds);
    const float padding = wui::theme().controls.horizontalPadding;
    expect(std::fabs(input.caretRect().x - (bounds.x + padding)) <= 0.01f,
           "Input start caret must begin at the Fluent left padding token");

    input.text("This intentionally long editable value verifies right content padding");
    input.controller().setCaret(input.controller().text().size());
    const float expectedEnd = bounds.x + bounds.width - padding - 1.0f;
    expect(std::fabs(input.caretRect().x - expectedEnd) <= 0.01f,
           "Input end caret must remain one DIP inside the right padding token");
}

void verifyShapeAndStateTokens(const std::vector<std::uint8_t>& pixels, int width, float scale)
{
    const auto& colors = wui::theme().colors;
    const auto background = colors.neutralBackground2.rest;

    // Circle corners must remain transparent/page-coloured. Axis/centre samples
    // ensure this is a real circular fill or ring, not an absent drawing.
    expect(pixelIs(pixels, width, scale, 168, 28, background)
               // Sample above the '+' glyph so the fill token is not blended
               // with foreground icon ink.
               && pixelIs(pixels, width, scale, 184, 35, colors.brandBackground.rest)
               && pixelIs(pixels, width, scale, 200, 44, background),
           "Circular Button must preserve corner transparency and a filled centre");
    // A one-DIP ring has no guaranteed fully covered sample below 200% DPR.
    // Accept its deliberately antialiased edge near the token; requiring an
    // exact full token at the mathematical edge would reward duplicate overdraw.
    expect(pixelIs(pixels, width, scale, 24, 216, background)
               && pixelNear(pixels, width, scale, 24, 226,
                            colors.neutralStrokeAccessible, 28)
               && pixelIs(pixels, width, scale, 34, 226, background),
           "Unselected Radio must render as a hollow circular stroke");

    // Blank interior/axis samples make the state checks independent from glyph
    // rasterization and continue to work identically at 100% and 150% scale.
    expect(pixelIs(pixels, width, scale, 36, 174, colors.neutralBackground1.rest)
               && pixelIs(pixels, width, scale, 196, 174, colors.neutralBackground1.hover)
               && pixelIs(pixels, width, scale, 356, 174, colors.neutralBackground1.pressed),
           "Compound Button rest, hover and pressed surfaces must be distinct");
    // At fractional DPR the mathematical edge is deliberately a partial-
    // coverage pixel. Keep the tolerance below the distance to adjacent state
    // tokens while allowing the expected 125% analytic-AA blend.
    expect(pixelNear(pixels, width, scale, 24, 226, colors.neutralStrokeAccessible, 28)
               && pixelNear(pixels, width, scale, 150, 226,
                            colors.neutralStrokeAccessibleHover, 28)
               && pixelNear(pixels, width, scale, 276, 226,
                            colors.neutralStrokeAccessiblePressed, 28)
               && pixelNear(pixels, width, scale, 402, 226,
                            colors.compoundBrandStroke.rest, 28)
               && pixelIs(pixels, width, scale, 404, 226, colors.neutralBackground2.rest)
               && pixelIs(pixels, width, scale, 410, 226,
                          colors.compoundBrandForeground1.rest),
           "Radio rest, hover, pressed and selected state tokens must be distinct");
    expect(pixelIs(pixels, width, scale, 100, 276, colors.brandBackground.rest)
               && pixelIs(pixels, width, scale, 400, 276, colors.neutralStroke1),
           "ProgressBar filled and track regions must remain visually distinct");
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::string output = argc > 1 ? argv[1] : "fluent_visual_acceptance.ppm";
        const float scale = argc > 2 ? std::max(1.0f, std::stof(argv[2])) : 1.0f;
        const int width = static_cast<int>(std::lround(kLogicalWidth * scale));
        const int height = static_cast<int>(std::lround(kLogicalHeight * scale));
        auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
        expect(canvas && canvas->initializeContext(), "Software canvas must initialize");
        wui::WhatsCanvasTextMeasurer measurer(*canvas, scale);
        wui::setTextMeasurer(&measurer);
        try {
            wui::PaintContext paint(*canvas, scale);
            canvas->beginFrame();
            paint.fillRect({0, 0, static_cast<float>(kLogicalWidth), static_cast<float>(kLogicalHeight)},
                           wui::theme().colors.neutralBackground2.rest);

            wui::Button button("Action");
            button.setAppearance(wui::ButtonAppearance::Primary);
            draw(button, paint, {24, 28, 120, 32});
            wui::Button circular("+");
            circular.setAppearance(wui::ButtonAppearance::Primary);
            circular.setShape(wui::ButtonShape::Circular);
            draw(circular, paint, {168, 28, 32, 32});

            wui::TextInput input("Placeholder");
            draw(input, paint, {24, 84, 240, 32});
            wui::Label label("Task label");
            draw(label, paint, {292, 90, 160, 20});

            wui::CompoundButton rest("Rest", "Description");
            wui::CompoundButton hover("Hover", "Description");
            wui::CompoundButton pressed("Pressed", "Description");
            hover.setVisualState(wui::ControlVisualState::Hovered, true);
            pressed.setVisualState(wui::ControlVisualState::Pressed, true);
            draw(rest, paint, {24, 132, 140, 52});
            draw(hover, paint, {184, 132, 140, 52});
            draw(pressed, paint, {344, 132, 140, 52});

            wui::Radio radioRest("Rest", false), radioHover("Hover", false),
                radioPressed("Pressed", false), radioSelected("Selected", true);
            radioHover.setVisualState(wui::ControlVisualState::Hovered, true);
            radioPressed.setVisualState(wui::ControlVisualState::Pressed, true);
            draw(radioRest, paint, {24, 210, 100, 32});
            draw(radioHover, paint, {150, 210, 100, 32});
            draw(radioPressed, paint, {276, 210, 100, 32});
            draw(radioSelected, paint, {402, 210, 120, 32});

            wui::ProgressBar progress(0.0f, 1.0f, 0.25f);
            draw(progress, paint, {24, 270, 540, 12});
            canvas->endFrame();

            const auto pixels = canvas->readPixelsRGBA();
            expect(pixels.size() == static_cast<std::size_t>(width * height * 4),
                   "Visual acceptance probe must capture a complete RGBA frame");
            // Persist the artifact before checking it, so a failed gate remains
            // directly inspectable in CI and local CTest working directories.
            savePpm(output, pixels, width, height);
            verifyTextGeometry(pixels, width, height, scale);
            verifyInputPaddingGeometry();
            verifyShapeAndStateTokens(pixels, width, scale);
        } catch (...) {
            wui::setTextMeasurer(nullptr);
            throw;
        }
        wui::setTextMeasurer(nullptr);
        return 0;
    } catch (const std::exception& error) {
        wui::setTextMeasurer(nullptr);
        std::cerr << "Fluent visual acceptance failure: " << error.what() << '\n';
        return 1;
    }
}
