#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/drawer.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {
void expect(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void savePpm(const std::string& path, const std::vector<unsigned char>& pixels, int width, int height)
{
    std::ofstream output(path, std::ios::binary); if (!output) throw std::runtime_error("cannot write drawer capture");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) { output.put(static_cast<char>(pixels[i])); output.put(static_cast<char>(pixels[i + 1])); output.put(static_cast<char>(pixels[i + 2])); }
}
bool hasColor(const std::vector<unsigned char>& pixels, wui::Color color)
{
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) if (pixels[i] == color.r && pixels[i + 1] == color.g && pixels[i + 2] == color.b && pixels[i + 3] == color.a) return true;
    return false;
}
bool differsAt(const std::vector<unsigned char>& pixels, int width, float scale, int logicalX, int logicalY, wui::Color color)
{
    const int x = static_cast<int>(std::lround(logicalX * scale));
    const int y = static_cast<int>(std::lround(logicalY * scale));
    const std::size_t index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4;
    return index + 3 < pixels.size() && (pixels[index] != color.r || pixels[index + 1] != color.g || pixels[index + 2] != color.b);
}
void render(const std::string& output, float scale)
{
    constexpr int logicalWidth = 900, logicalHeight = 560; scale = std::max(1.0f, scale);
    const int width = static_cast<int>(std::lround(logicalWidth * scale)), height = static_cast<int>(std::lround(logicalHeight * scale));
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height); expect(canvas && canvas->initializeContext(), "software canvas must initialize");
    wui::WhatsCanvasTextMeasurer measurer(*canvas, scale); wui::setTextMeasurer(&measurer);
    try {
        wui::PaintContext paint(*canvas, scale); canvas->beginFrame();
        paint.fillRect({0, 0, static_cast<float>(logicalWidth), static_cast<float>(logicalHeight)}, wui::theme().colors.neutralBackground2.rest);
        paint.drawText("Fluent Drawer", 40, 56, 26, wui::theme().colors.neutralForeground1, 600);
        paint.drawText("Overlay and inline layouts", 40, 84, 15, wui::theme().colors.neutralForeground2, 400);
        wui::Drawer overlay("Edit profile", "Changes are saved to your workspace");
        overlay.size(wui::DrawerSize::Small).primaryAction("Save").secondaryAction("Cancel"); overlay.layout({0, 0, static_cast<float>(logicalWidth), static_cast<float>(logicalHeight)}); overlay.prepare(paint); overlay.paint(paint);
        // An independent inline surface at the left demonstrates that it does
        // not require a scrim and remains visually distinct from the overlay.
        wui::Drawer inlineDrawer("Navigation", "Pinned workspace sections");
        inlineDrawer.type(wui::DrawerType::Inline).position(wui::DrawerPosition::Start).width(252).modal(false).layout({40, 130, 252, 330}); inlineDrawer.prepare(paint); inlineDrawer.paint(paint);
        canvas->endFrame(); const auto pixels = canvas->readPixelsRGBA();
        const auto background = wui::theme().colors.neutralBackground2.rest;
        // Scrim is alpha-composited, so assert its observable effect rather
        // than looking for an impossible pre-composite source color.
        savePpm(output, pixels, width, height);
        expect(!pixels.empty() && hasColor(pixels, wui::theme().colors.brandBackground.rest) &&
                   differsAt(pixels, width, scale, 560, 100, background),
               "Drawer visual must include modal dimming and primary action colors");
    } catch (...) { wui::setTextMeasurer(nullptr); throw; }
    wui::setTextMeasurer(nullptr);
}
} // namespace
int main(int argc, char** argv)
{
    try { render(argc > 1 ? argv[1] : "fluent_drawer_review.ppm", argc > 2 ? std::stof(argv[2]) : 1.0f); return 0; }
    catch (const std::exception& error) { std::cerr << "Fluent drawer visual failure: " << error.what() << '\n'; return 1; }
}
