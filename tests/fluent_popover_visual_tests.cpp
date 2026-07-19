#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/paint_context.h"
#include "wui/popover.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {
void ppm(const std::string& file, const std::vector<std::uint8_t>& pixels, int width, int height)
{
    std::ofstream out(file, std::ios::binary);
    if (!out) throw std::runtime_error("failed to write visual review PPM");
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (int i = 0; i < width * height; ++i) out.write(reinterpret_cast<const char*>(&pixels[static_cast<std::size_t>(i) * 4]), 3);
}

bool isBackground(const std::vector<std::uint8_t>& pixels, int width, float scale, float x, float y)
{
    const int px = static_cast<int>(std::lround(x * scale));
    const int py = static_cast<int>(std::lround(y * scale));
    const std::size_t index = static_cast<std::size_t>((py * width + px) * 4);
    const auto background = wui::theme().colors.neutralBackground2.rest;
    return index + 3 < pixels.size() && pixels[index] == background.r && pixels[index + 1] == background.g && pixels[index + 2] == background.b;
}

void render(float scale, const std::string& file)
{
    constexpr int logicalWidth = 1040, logicalHeight = 360;
    const int width = static_cast<int>(logicalWidth * scale), height = static_cast<int>(logicalHeight * scale);
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
    if (!canvas || !canvas->initializeContext()) throw std::runtime_error("popover software canvas must initialize");
    wui::WhatsCanvasTextMeasurer measurer(*canvas, scale); wui::setTextMeasurer(&measurer);
    wui::PaintContext context(*canvas, scale);
    canvas->beginFrame();
    context.fillRect({0, 0, static_cast<float>(logicalWidth), static_cast<float>(logicalHeight)}, wui::theme().colors.neutralBackground2.rest);

    wui::Popover regular("Share this project", "Members can view activity.");
    regular.showArrow().anchor({84, 48, 124, 32}).placement(wui::PopupPlacement::BelowStart);
    regular.layout({0, 0, static_cast<float>(logicalWidth), static_cast<float>(logicalHeight)});
    regular.paint(context);

    wui::Popover inverse("Keyboard shortcut", "Press Ctrl+K to open command search.");
    inverse.appearance(wui::PopoverAppearance::Inverted).showArrow().anchor({84, 290, 118, 32}).placement(wui::PopupPlacement::AboveStart);
    inverse.layout({0, 0, static_cast<float>(logicalWidth), static_cast<float>(logicalHeight)});
    inverse.paint(context);

    wui::TeachingPopover teaching("Welcome to WhatsUI", "Create a task, then use filters to focus on what matters.");
    teaching.stepText("Step 1 of 3").primaryAction("Next").secondaryAction("Back").anchor({580, 52, 120, 32});
    teaching.layout({0, 0, static_cast<float>(logicalWidth), static_cast<float>(logicalHeight)});
    teaching.paint(context);
    canvas->endFrame();
    const auto pixels = canvas->readPixelsRGBA();
    if (pixels.size() != static_cast<std::size_t>(width * height * 4)) throw std::runtime_error("popover visual has incomplete pixels");
    // Validate that both optional arrows produce real non-background geometry.
    // The visual review covers the intentionally seam-free base; sampling the
    // anti-aliased join itself is not stable at every fractional DPI scale.
    const auto regularPanel = regular.panelBounds();
    const float regularCenter = regular.anchor().x + regular.anchor().width * .5f;
    if (isBackground(pixels, width, scale, regularCenter, regularPanel.y - 3.0f))
        throw std::runtime_error("below Popover must paint its optional anchor arrow");
    const auto inversePanel = inverse.panelBounds();
    const float inverseCenter = inverse.anchor().x + inverse.anchor().width * .5f;
    if (isBackground(pixels, width, scale, inverseCenter, inversePanel.y + inversePanel.height + 3.0f))
        throw std::runtime_error("above Popover must paint its optional anchor arrow");
    ppm(file, pixels, width, height);
    wui::setTextMeasurer(nullptr);
}
} // namespace

int main(int argc, char** argv)
{
    try {
        const std::string base = argc > 1 ? argv[1] : "fluent_popover";
        render(1.0f, base + "_100.ppm");
        render(1.5f, base + "_150.ppm");
        return EXIT_SUCCESS;
    } catch (...) { return EXIT_FAILURE; }
}
