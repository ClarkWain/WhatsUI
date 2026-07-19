#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/icons.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {

constexpr int kWidth = 760;
constexpr int kHeight = 430;

struct GalleryIcon {
    wui::IconName name;
    const char* label;
};

constexpr std::array kGallery{
    GalleryIcon{wui::IconName::Add, "Add"},
    GalleryIcon{wui::IconName::Delete, "Delete"},
    GalleryIcon{wui::IconName::Dismiss, "Dismiss"},
    GalleryIcon{wui::IconName::Edit, "Edit"},
    GalleryIcon{wui::IconName::Star, "Star"},
    GalleryIcon{wui::IconName::Checkmark, "Check"},
    GalleryIcon{wui::IconName::Info, "Info"},
    GalleryIcon{wui::IconName::Warning, "Warning"},
    GalleryIcon{wui::IconName::ErrorCircle, "Error"},
    GalleryIcon{wui::IconName::Search, "Search"},
    GalleryIcon{wui::IconName::MoreHorizontal, "More"},
    GalleryIcon{wui::IconName::Calendar, "Calendar"},
};

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

void savePpm(const std::string& path,
             const std::vector<unsigned char>& pixels, int width, int height)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot save Fluent icon review");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t index = 0; index + 3 < pixels.size(); index += 4) {
        output.put(static_cast<char>(pixels[index]));
        output.put(static_cast<char>(pixels[index + 1]));
        output.put(static_cast<char>(pixels[index + 2]));
    }
}

std::size_t coloredPixels(const std::vector<unsigned char>& pixels, int width,
                          int height, float scale, const wui::RectF& bounds)
{
    const int left = std::max(0, static_cast<int>(std::floor(bounds.x * scale)));
    const int top = std::max(0, static_cast<int>(std::floor(bounds.y * scale)));
    const int right =
        std::min(width, static_cast<int>(
                            std::ceil((bounds.x + bounds.width) * scale)));
    const int bottom =
        std::min(height, static_cast<int>(
                             std::ceil((bounds.y + bounds.height) * scale)));
    std::size_t count = 0;
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const auto at = static_cast<std::size_t>((y * width + x) * 4);
            const int red = pixels[at];
            const int green = pixels[at + 1];
            const int blue = pixels[at + 2];
            if (blue > red + 24 && blue > green + 8 && red < 100) ++count;
        }
    }
    return count;
}

void render(const std::string& outputPath, float scale)
{
    scale = std::max(1.0f, scale);
    const int width = static_cast<int>(std::lround(kWidth * scale));
    const int height = static_cast<int>(std::lround(kHeight * scale));
    auto canvas =
        wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
    expect(canvas && canvas->initializeContext(),
           "Fluent icon gallery software canvas must initialize");

    wui::WhatsCanvasTextMeasurer measurer(*canvas, scale);
    const auto status = measurer.policyStatus();
    expect(status.regularIconFont && status.filledIconFont,
           "Both bundled Fluent icon font faces must register");
    wui::setTextMeasurer(&measurer);

    try {
        wui::PaintContext paint(*canvas, scale);
        const auto& current = wui::theme();
        const auto background = current.colors.neutralBackground2.rest;
        const auto card = current.colors.neutralBackground1.rest;
        const auto foreground = current.colors.neutralForeground1;
        const auto brand = current.colors.brandForeground1;

        canvas->beginFrame();
        paint.fillRect({0, 0, static_cast<float>(kWidth),
                        static_cast<float>(kHeight)}, background);
        paint.drawText("Fluent System Icons", 28.0f, 44.0f, 24.0f,
                       foreground, 600, current.typography.body1.family);
        paint.drawText("Regular", 28.0f, 82.0f, 14.0f, foreground, 600,
                       current.typography.body1.family);
        paint.drawText("Filled", 394.0f, 82.0f, 14.0f, foreground, 600,
                       current.typography.body1.family);

        std::vector<wui::RectF> glyphBounds;
        glyphBounds.reserve(kGallery.size() * 2 + 6);
        for (std::size_t index = 0; index < kGallery.size(); ++index) {
            const int row = static_cast<int>(index / 3);
            const int column = static_cast<int>(index % 3);
            for (int styleIndex = 0; styleIndex < 2; ++styleIndex) {
                const float sectionX = styleIndex == 0 ? 28.0f : 394.0f;
                const float x = sectionX + column * 112.0f;
                const float y = 96.0f + row * 68.0f;
                const wui::RectF tile{x, y, 100.0f, 56.0f};
                paint.fillRoundRect(tile, current.radius.medium, card);
                const wui::RectF glyph{x + 8.0f, y + 8.0f, 24.0f, 24.0f};
                glyphBounds.push_back(glyph);
                wui::drawIcon(
                    paint, kGallery[index].name, glyph, brand,
                    wui::IconSize::Size20,
                    styleIndex == 0 ? wui::IconStyle::Regular
                                    : wui::IconStyle::Filled);
                paint.drawText(kGallery[index].label, x + 8.0f, y + 49.0f,
                               11.0f, foreground, 400,
                               current.typography.caption1.family);
            }
        }

        paint.drawText("Optical sizes", 28.0f, 390.0f, 14.0f, foreground,
                       600, current.typography.body1.family);
        float sizeX = 146.0f;
        for (const auto size : {wui::IconSize::Size16, wui::IconSize::Size20,
                                wui::IconSize::Size24}) {
            const float extent = static_cast<float>(size);
            const wui::RectF glyph{sizeX, 368.0f, 32.0f, 32.0f};
            glyphBounds.push_back(glyph);
            wui::drawIcon(paint, wui::IconName::TaskList, glyph, brand, size,
                          wui::IconStyle::Regular);
            paint.drawText(std::to_string(static_cast<int>(extent)), sizeX,
                           418.0f, 11.0f, foreground, 400,
                           current.typography.caption1.family);
            sizeX += 72.0f;
        }

        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        expect(pixels.size() ==
                   static_cast<std::size_t>(width * height * 4),
               "Fluent icon gallery must return a complete RGBA frame");
        for (const auto& glyph : glyphBounds) {
            expect(coloredPixels(pixels, width, height, scale, glyph) >= 3,
                   "Every gallery glyph must produce visible brand pixels");
        }
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
        render(argc > 1 ? argv[1] : "fluent_icons_review.ppm",
               argc > 2 ? std::stof(argv[2]) : 1.0f);
        return 0;
    } catch (...) {
        return 1;
    }
}
