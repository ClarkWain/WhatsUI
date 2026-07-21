#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"

#include "wui/paint_context.h"
#include "wui/rating.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"
#include "wui/widgets.h"

namespace {

constexpr int kLogicalWidth = 780;
constexpr int kLogicalHeight = 420;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void savePpm(const std::string& path, const std::vector<unsigned char>& rgba,
             int width, int height)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot create Rating/Image visual capture");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t index = 0; index + 3 < rgba.size(); index += 4) {
        output.put(static_cast<char>(rgba[index]));
        output.put(static_cast<char>(rgba[index + 1]));
        output.put(static_cast<char>(rgba[index + 2]));
    }
}

std::vector<unsigned char> checker(int width, int height)
{
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width * height * 4));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool alternate = ((x / 8) + (y / 8)) % 2 != 0;
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = alternate ? 15 : 71;
            pixels[offset + 1] = alternate ? 108 : 158;
            pixels[offset + 2] = alternate ? 189 : 245;
            pixels[offset + 3] = 255;
        }
    }
    return pixels;
}

bool pixelIs(const std::vector<unsigned char>& pixels, int width, float scale,
             int logicalX, int logicalY, wui::Color color)
{
    const int x = static_cast<int>(std::lround(logicalX * scale));
    const int y = static_cast<int>(std::lround(logicalY * scale));
    const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
    return offset + 3 < pixels.size() && pixels[offset] == color.r &&
        pixels[offset + 1] == color.g && pixels[offset + 2] == color.b &&
        pixels[offset + 3] == color.a;
}

void render(const std::string& outputPath, float scale)
{
    scale = std::max(1.0f, scale);
    const int width = static_cast<int>(std::lround(kLogicalWidth * scale));
    const int height = static_cast<int>(std::lround(kLogicalHeight * scale));
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
    expect(canvas && canvas->initializeContext(), "Software canvas must initialize");
    wui::WhatsCanvasTextMeasurer measurer(*canvas, scale);
    wui::setTextMeasurer(&measurer);
    try {
        wui::PaintContext paint(*canvas, scale);
        canvas->beginFrame();
        paint.fillRect({0, 0, kLogicalWidth, kLogicalHeight}, wui::theme().colors.neutralBackground2.rest);
        paint.drawText("Fluent Rating", 24, 36, 20, wui::theme().colors.neutralForeground1, 600);

        wui::Rating neutral(3.0f);
        neutral.layout({24, 56, 148, 28});
        neutral.paint(paint);
        wui::Rating half(3.5f);
        half.setStep(0.5f);
        half.setColor(wui::RatingColor::Brand);
        half.setVisualState(wui::ControlVisualState::Focused, true);
        half.layout({200, 56, 148, 28});
        half.paint(paint);
        wui::Rating marigold(4.0f);
        marigold.setColor(wui::RatingColor::Marigold);
        marigold.setSize(wui::RatingSize::Large);
        marigold.layout({376, 60, 108, 20});
        marigold.paint(paint);
        wui::Rating disabled(2.0f);
        disabled.setEnabled(false);
        disabled.setSize(wui::RatingSize::Medium);
        disabled.layout({512, 62, 88, 16});
        disabled.paint(paint);

        wui::Rating circles(2.5f);
        circles.setStep(0.5f);
        circles.setShape(wui::RatingShape::Circle);
        circles.setColor(wui::RatingColor::Brand);
        circles.setSize(wui::RatingSize::Medium);
        circles.layout({24, 112, 88, 16});
        circles.paint(paint);
        wui::Rating squares(4.5f);
        squares.setStep(0.5f);
        squares.setShape(wui::RatingShape::Square);
        squares.setSize(wui::RatingSize::Small);
        squares.layout({136, 112, 68, 16});
        squares.paint(paint);

        wui::Rating hovered(0.0f);
        hovered.setSize(wui::RatingSize::Small);
        hovered.setColor(wui::RatingColor::Brand);
        hovered.layout({236, 112, 68, 16});
        wui::PointerEvent hoverEvent;
        hoverEvent.action = wui::PointerAction::Move;
        hoverEvent.position = {270.0f, 120.0f};
        hovered.onPointerEvent(hoverEvent);
        hovered.paint(paint);
        wui::Rating pressed(0.0f);
        pressed.setSize(wui::RatingSize::Small);
        pressed.setColor(wui::RatingColor::Brand);
        pressed.layout({328, 112, 68, 16});
        wui::PointerEvent pressEvent;
        pressEvent.action = wui::PointerAction::Down;
        pressEvent.button = wui::MouseButton::Left;
        pressEvent.position = {362.0f, 120.0f};
        pressed.onPointerEvent(pressEvent);
        pressed.paint(paint);
        wui::Rating readOnly(3.0f);
        readOnly.setSize(wui::RatingSize::Small);
        readOnly.setReadOnly(true);
        readOnly.layout({420, 112, 68, 16});
        readOnly.paint(paint);

        wui::RatingDisplay display(4.5f);
        display.setCount(12345);
        display.layout({24, 156, 220, 24});
        display.paint(paint);
        wui::RatingDisplay compact(3.8f);
        compact.setCount(1160);
        compact.setCompact(true);
        compact.setColor(wui::RatingColor::Marigold);
        compact.setSize(wui::RatingSize::ExtraLarge);
        compact.layout({286, 150, 200, 36});
        compact.paint(paint);

        paint.drawText("Image shape · border · shadow · fit", 24, 232, 20,
                       wui::theme().colors.neutralForeground1, 600);
        const auto source = checker(64, 40);
        const wui::ImageShape shapes[]{wui::ImageShape::Square,
                                      wui::ImageShape::Rounded,
                                      wui::ImageShape::Circular};
        for (int index = 0; index < 3; ++index) {
            wui::Image image(source, 64, 40);
            image.setShape(shapes[index]);
            image.setFit(index == 0 ? wui::ImageFit::Contain
                                    : index == 1 ? wui::ImageFit::Cover : wui::ImageFit::Center);
            image.setBordered(true);
            image.setShadow(index == 1);
            image.layout({24.0f + index * 156.0f, 256.0f, 128.0f, 120.0f});
            image.prepare(paint);
            image.paint(paint);
        }
        wui::Image fallbackImage;
        fallbackImage.fallback(source, 64, 40);
        fallbackImage.setFit(wui::ImageFit::Cover);
        fallbackImage.setShape(wui::ImageShape::Rounded);
        fallbackImage.setBordered(true);
        fallbackImage.layout({492.0f, 256.0f, 128.0f, 120.0f});
        fallbackImage.prepare(paint);
        fallbackImage.paint(paint);

        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        expect(pixels.size() == static_cast<std::size_t>(width * height * 4),
               "Rating/Image visual capture must return a complete RGBA frame");
        expect(paint.paintStats().boxShadowCalls >= 2,
               "Image review matrix must issue Fluent elevation shadows");
        expect(pixelIs(pixels, width, scale, 700, 200,
                       wui::theme().colors.neutralBackground2.rest),
               "Visual capture background must remain intact after polygon and clip batches");
        expect(pixelIs(pixels, width, scale, 350, 316,
                        wui::theme().colors.neutralBackground2.rest),
               "Circular centered Image must not fill transparent letterbox space with its border");
        expect(pixelIs(pixels, width, scale, 337, 316,
                        wui::theme().colors.neutralBackground2.rest) &&
                   pixelIs(pixels, width, scale, 463, 316,
                           wui::theme().colors.neutralBackground2.rest),
               "Circular Image must center a square viewport inside non-square bounds");
        expect(pixelIs(pixels, width, scale, 86, 120,
                        wui::theme().colors.neutralBackground2.rest),
               "Empty circle Rating items must preserve a non-white parent background through a true stroke");
        savePpm(outputPath, pixels, width, height);
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
        render(argc > 1 ? argv[1] : "fluent_rating_image_review.ppm",
               argc > 2 ? std::stof(argv[2]) : 1.0f);
        std::cout << "WhatsUI Fluent Rating/Image visual tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WhatsUI Fluent Rating/Image visual tests failed: " << error.what() << '\n';
        return 1;
    }
}
