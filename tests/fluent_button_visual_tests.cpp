#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"

#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"
#include "wui/widgets.h"

namespace {

constexpr int kLogicalWidth = 760;
constexpr int kLogicalHeight = 480;

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

    [[nodiscard]] float centerX() const noexcept
    {
        return (static_cast<float>(left) + static_cast<float>(right)) * 0.5f;
    }

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
    if (!output) throw std::runtime_error("cannot create Button artifact");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t offset = 0; offset + 3 < pixels.size(); offset += 4) {
        output.put(static_cast<char>(pixels[offset]));
        output.put(static_cast<char>(pixels[offset + 1]));
        output.put(static_cast<char>(pixels[offset + 2]));
    }
}

void draw(wui::Node& button, wui::PaintContext& paint,
          const wui::RectF& bounds)
{
    button.layout(bounds);
    button.prepare(paint);
    button.paint(paint);
}

void renderAndVerify(const std::string& outputPath, float scale)
{
    const int width =
        static_cast<int>(std::lround(kLogicalWidth * scale));
    const int height =
        static_cast<int>(std::lround(kLogicalHeight * scale));
    auto canvas =
        wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
    expect(canvas && canvas->initializeContext(),
           "Software Canvas must initialize for Button review");
    const auto iconFonts = wui::registerDefaultIconFonts(*canvas);
    expect(iconFonts.complete(),
           "Bundled Fluent icon fonts must be available for Button review");
    wui::WhatsCanvasTextMeasurer measurer(*canvas, scale);
    wui::setTextMeasurer(&measurer);

    try {
        const auto& current = wui::theme();
        wui::PaintContext paint(*canvas, scale);
        canvas->beginFrame();
        paint.fillRect(
            {0, 0, static_cast<float>(kLogicalWidth),
             static_cast<float>(kLogicalHeight)},
            current.colors.neutralBackground2.rest);
        paint.drawText("Fluent Button", 32, 38, 22,
                       current.colors.neutralForeground1, 600);

        wui::Button secondaryRest("Example");
        wui::Button secondaryHover("Example");
        wui::Button secondaryPressed("Example");
        wui::Button secondaryDisabled("Example");
        secondaryHover.setVisualState(
            wui::ControlVisualState::Hovered, true);
        secondaryPressed.setVisualState(
            wui::ControlVisualState::Pressed, true);
        secondaryDisabled.setEnabled(false);
        draw(secondaryRest, paint, {32, 64, 96, 32});
        draw(secondaryHover, paint, {152, 64, 96, 32});
        draw(secondaryPressed, paint, {272, 64, 96, 32});
        draw(secondaryDisabled, paint, {392, 64, 96, 32});

        wui::Button primaryRest("Add");
        wui::Button primaryHover("Add");
        wui::Button primaryPressed("Add");
        wui::Button pointerFocus("Add");
        wui::Button keyboardFocus("Add");
        for (auto* button : {&primaryRest, &primaryHover, &primaryPressed,
                             &pointerFocus, &keyboardFocus}) {
            button->setAppearance(wui::ButtonAppearance::Primary);
        }
        primaryHover.setVisualState(
            wui::ControlVisualState::Hovered, true);
        primaryPressed.setVisualState(
            wui::ControlVisualState::Pressed, true);
        pointerFocus.setVisualState(
            wui::ControlVisualState::Focused, true);
        keyboardFocus.setVisualState(
            wui::ControlVisualState::Focused, true);
        keyboardFocus.setVisualState(
            wui::ControlVisualState::FocusVisible, true);
        draw(primaryRest, paint, {32, 136, 96, 32});
        draw(primaryHover, paint, {152, 136, 96, 32});
        draw(primaryPressed, paint, {272, 136, 96, 32});
        draw(pointerFocus, paint, {392, 136, 96, 32});
        draw(keyboardFocus, paint, {512, 136, 96, 32});

        wui::Button outlineRest("Text");
        wui::Button outlineHover("Text");
        wui::Button outlinePressed("Text");
        wui::Button subtleRest("Text");
        wui::Button subtleHover("Text");
        wui::Button subtlePressed("Text");
        wui::Button transparentHover("Text");
        outlineRest.setAppearance(wui::ButtonAppearance::Outline);
        outlineHover.setAppearance(wui::ButtonAppearance::Outline);
        outlinePressed.setAppearance(wui::ButtonAppearance::Outline);
        subtleRest.setAppearance(wui::ButtonAppearance::Subtle);
        subtleHover.setAppearance(wui::ButtonAppearance::Subtle);
        subtlePressed.setAppearance(wui::ButtonAppearance::Subtle);
        transparentHover.setAppearance(wui::ButtonAppearance::Transparent);
        outlineHover.setVisualState(wui::ControlVisualState::Hovered, true);
        outlinePressed.setVisualState(wui::ControlVisualState::Pressed, true);
        subtleHover.setVisualState(wui::ControlVisualState::Hovered, true);
        subtlePressed.setVisualState(wui::ControlVisualState::Pressed, true);
        transparentHover.setVisualState(wui::ControlVisualState::Hovered, true);
        draw(outlineRest, paint, {32, 208, 80, 32});
        draw(outlineHover, paint, {132, 208, 80, 32});
        draw(outlinePressed, paint, {232, 208, 80, 32});
        draw(subtleRest, paint, {332, 208, 80, 32});
        draw(subtleHover, paint, {432, 208, 80, 32});
        draw(subtlePressed, paint, {532, 208, 80, 32});
        draw(transparentHover, paint, {632, 208, 80, 32});

        wui::Button small("Small");
        wui::Button medium("Medium");
        wui::Button large("Large");
        small.setSize(wui::ButtonSize::Small);
        large.setSize(wui::ButtonSize::Large);
        const auto smallSize = small.measure({});
        const auto mediumSize = medium.measure({});
        const auto largeSize = large.measure({});
        draw(small, paint, {32, 312, smallSize.width, smallSize.height});
        draw(medium, paint, {128, 308, mediumSize.width,
                             mediumSize.height});
        draw(large, paint, {256, 304, largeSize.width, largeSize.height});

        wui::Button iconText("Text");
        iconText.setAppearance(wui::ButtonAppearance::Primary);
        iconText.setIcon(wui::IconName::Circle);
        wui::Button iconOnly("Add");
        iconOnly.setAppearance(wui::ButtonAppearance::Primary);
        iconOnly.setIcon(wui::IconName::Add);
        iconOnly.setIconOnly(true);
        wui::ToggleButton secondarySelected("Text", true);
        secondarySelected.setIcon(wui::IconName::Circle);
        wui::ToggleButton outlineSelected("Text", true);
        outlineSelected.setAppearance(wui::ButtonAppearance::Outline);
        outlineSelected.setIcon(wui::IconName::Circle);
        wui::ToggleButton primaryIconSelected("Selected", true);
        primaryIconSelected.setAppearance(wui::ButtonAppearance::Primary);
        primaryIconSelected.setIcon(wui::IconName::Circle);
        primaryIconSelected.setIconOnly(true);
        const auto iconTextSize = iconText.measure({});
        const auto iconOnlySize = iconOnly.measure({});
        draw(iconText, paint,
             {32, 384, iconTextSize.width, iconTextSize.height});
        draw(iconOnly, paint,
             {152, 384, iconOnlySize.width, iconOnlySize.height});
        draw(secondarySelected, paint, {232, 384, 80, 32});
        draw(outlineSelected, paint, {352, 384, 80, 32});
        draw(primaryIconSelected, paint, {472, 384, 32, 32});

        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        expect(pixels.size() ==
                   static_cast<std::size_t>(width * height * 4),
               "Button capture must return complete RGBA");
        savePpm(outputPath, pixels, width, height);

        expect(secondaryRest.appearance() ==
                   wui::ButtonAppearance::Secondary &&
                   secondaryRest.variant() ==
                       wui::ButtonVariant::Secondary,
               "Fluent Button default appearance must be Secondary");
        const float expectedSmallWidth =
            measurer.measureText("Small", 12.0f, 400, "Segoe UI").width +
            16.0f;
        const float expectedMediumWidth =
            measurer.measureText("Medium", 14.0f, 600, "Segoe UI").width +
            24.0f;
        const float expectedLargeWidth =
            measurer.measureText("Large", 16.0f, 600, "Segoe UI").width +
            32.0f;
        expect(std::abs(smallSize.width - expectedSmallWidth) <= 0.01f &&
                   std::abs(smallSize.height - 24.0f) <= 0.01f &&
                   std::abs(mediumSize.width - expectedMediumWidth) <= 0.01f &&
                   std::abs(mediumSize.height - 32.0f) <= 0.01f &&
                   std::abs(largeSize.width - expectedLargeWidth) <= 0.01f &&
                   std::abs(largeSize.height - 40.0f) <= 0.01f,
               "Button sizes must hug measured Segoe UI content with 8/12/16-DIP padding and 24/32/40-DIP heights");
        const float iconTextExpected =
            measurer.measureText("Text", 14.0f, 600, "Segoe UI").width +
            20.0f + 6.0f + 24.0f;
        expect(std::abs(iconTextSize.width - iconTextExpected) <= 0.01f &&
                   std::abs(iconTextSize.height - 32.0f) <= 0.01f &&
                   std::abs(iconOnlySize.width - 32.0f) <= 0.01f &&
                   std::abs(iconOnlySize.height - 32.0f) <= 0.01f,
               "Medium icon+text and icon-only Buttons must preserve the Figma 20-DIP icon, 6-DIP gap, and 32-DIP square geometry");

        expect(findColor(pixels, width, height, scale,
                         {40, 70, 80, 20},
                         current.colors.neutralBackground1.rest, 1)
                   .valid() &&
                   findColor(pixels, width, height, scale,
                             {160, 70, 80, 20},
                             current.colors.neutralBackground1.hover, 1)
                       .valid() &&
                   findColor(pixels, width, height, scale,
                             {280, 70, 80, 20},
                             current.colors.neutralBackground1.pressed, 1)
                       .valid() &&
                   findColor(pixels, width, height, scale,
                             {400, 70, 80, 20},
                             current.colors.neutralBackgroundDisabled, 1)
                       .valid(),
               "Secondary Button surfaces must resolve rest, hover, pressed, and disabled aliases");

        expect(findColor(pixels, width, height, scale,
                         {40, 142, 80, 20},
                         current.colors.brandBackground.rest, 1)
                   .valid() &&
                   findColor(pixels, width, height, scale,
                             {160, 142, 80, 20},
                             current.colors.brandBackground.hover, 1)
                       .valid() &&
                   findColor(pixels, width, height, scale,
                             {280, 142, 80, 20},
                             current.colors.brandBackground.pressed, 1)
                       .valid(),
               "Primary Button surfaces must resolve the complete brand state ramp");

        expect(findColor(pixels, width, height, scale,
                         {32, 208, 80, 32},
                         current.colors.neutralStroke1, 2)
                   .valid() &&
                   findColor(pixels, width, height, scale,
                             {132, 208, 80, 32},
                             current.colors.neutralStroke1Hover, 2)
                       .valid() &&
                   findColor(pixels, width, height, scale,
                             {232, 208, 80, 32},
                             current.colors.neutralStroke1Pressed, 2)
                       .valid(),
               "Outline Button must stay transparent while its stroke follows the rest/hover/pressed ramp");
        expect(findColor(pixels, width, height, scale,
                         {440, 214, 64, 20},
                         current.colors.neutralBackground1.hover, 2)
                   .valid() &&
                   findColor(pixels, width, height, scale,
                             {540, 214, 64, 20},
                             current.colors.neutralBackground1.pressed, 2)
                       .valid(),
               "Subtle Button must use the exact Fluent hover and pressed surfaces");
        expect(findColor(pixels, width, height, scale,
                         {648, 212, 48, 24},
                         current.colors.brandForeground1, 32)
                   .valid(),
               "Transparent Button hover must use the Fluent brand foreground");

        const auto pointerRing = findColor(
            pixels, width, height, scale, {389, 133, 102, 38},
            current.colors.strokeFocusInner, 8);
        const auto keyboardRing = findColor(
            pixels, width, height, scale, {509, 133, 102, 38},
            current.colors.strokeFocusInner, 8);
        expect(!pointerRing.valid() && keyboardRing.valid(),
               "Pointer focus must not leave a black frame; keyboard focus-visible must retain it");

        const auto addInk = findColor(
            pixels, width, height, scale, {56, 140, 48, 24},
            current.colors.onBrand, 96);
        const float expectedCenter = (32.0f + 48.0f) * scale - 0.5f;
        expect(addInk.valid() &&
                   std::abs(addInk.centerX() - expectedCenter) <=
                       std::max(2.0f, 2.0f * scale),
               "Button label ink must remain horizontally centred in the 96-DIP face");
        expect(addInk.centerY() < 152.0f * scale,
               "Button label must retain the Figma optical upward offset");
        expect(findColor(pixels, width, height, scale,
                         {240, 390, 64, 20},
                         current.colors.neutralBackground1.selected, 2)
                   .valid() &&
                   findColor(pixels, width, height, scale,
                             {352, 384, 80, 32},
                             current.colors.neutralStroke1Selected, 2)
                       .valid() &&
                   findColor(pixels, width, height, scale,
                             {476, 388, 24, 24},
                             current.colors.brandBackground.selected, 2)
                       .valid(),
               "ToggleButton selected states must use the Figma Secondary, Outline, and Primary aliases");
    } catch (...) {
        wui::setTextMeasurer(nullptr);
        throw;
    }
    wui::setTextMeasurer(nullptr);
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::string output =
            argc > 1 ? argv[1] : "fluent_button_review.ppm";
        const float scale =
            argc > 2 ? std::max(1.0f, std::stof(argv[2])) : 1.0f;
        renderAndVerify(output, scale);
        std::cout << "Fluent Button visual tests passed at " << scale
                  << "x: " << output << '\n';
        return 0;
    } catch (const std::exception& error) {
        wui::setTextMeasurer(nullptr);
        std::cerr << "Fluent Button visual tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
