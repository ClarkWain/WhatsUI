#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/tree.h"
#include "wui/whatscanvas_text.h"

namespace {
constexpr int kWidth = 680, kHeight = 330;
void expect(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void savePpm(const std::string& path, const std::vector<unsigned char>& pixels, int width, int height)
{
    std::ofstream out(path, std::ios::binary); if (!out) throw std::runtime_error("cannot save Tree capture");
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) { out.put(static_cast<char>(pixels[i])); out.put(static_cast<char>(pixels[i + 1])); out.put(static_cast<char>(pixels[i + 2])); }
}
bool hasColor(const std::vector<unsigned char>& pixels, wui::Color color)
{
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) if (pixels[i] == color.r && pixels[i + 1] == color.g && pixels[i + 2] == color.b && pixels[i + 3] == color.a) return true;
    return false;
}
void render(const std::string& output, float scale)
{
    scale = std::max(1.0f, scale); const int width = static_cast<int>(std::lround(kWidth * scale)), height = static_cast<int>(std::lround(kHeight * scale));
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height); expect(canvas && canvas->initializeContext(), "Tree software Canvas must initialize");
    wui::WhatsCanvasTextMeasurer measurer(*canvas, scale); wui::setTextMeasurer(&measurer);
    try {
        wui::PaintContext paint(*canvas, scale); canvas->beginFrame(); const auto& theme = wui::theme();
        paint.fillRect({0, 0, static_cast<float>(kWidth), static_cast<float>(kHeight)}, theme.colors.neutralBackground2.rest);
        paint.drawText("Project explorer", 28, 38, 22, theme.colors.neutralForeground1, 600);
        paint.drawText("Keyboard tree navigation with selected and disabled items", 28, 61, 13, theme.colors.neutralForeground2, 400);
        wui::Tree tree; tree.setMaxVisibleItems(6);
        auto& project = tree.addItem("project", "WhatsUI");
        project.addItem("readme", "README.md"); auto& src = project.addItem("src", "src"); src.addItem("widgets", "widgets"); src.addItem("tree", "tree.cpp");
        auto& packages = tree.addItem("packages", "packages"); packages.setExpanded(false);
        auto& generated = tree.addItem("generated", "Generated files"); generated.setEnabled(false);
        tree.addItem("settings", "Settings"); tree.layout({28, 88, 300, 200}); tree.select("tree"); tree.paint(paint);
        paint.drawText("Collapsed branch", 390, 112, 14, theme.colors.neutralForeground2, 600);
        wui::Tree compact; auto& archive = compact.addItem("archive", "Archive"); archive.addItem("2025", "2025"); archive.setExpanded(false); compact.addItem("notes", "Notes");
        compact.layout({390, 132, 230, 96}); compact.paint(paint);
        canvas->endFrame(); const auto pixels = canvas->readPixelsRGBA();
        expect(pixels.size() == static_cast<std::size_t>(width * height * 4), "Tree capture must have complete RGBA pixels");
        expect(hasColor(pixels, theme.colors.neutralBackground1.selected), "Tree visual must show selected row treatment");
        savePpm(output, pixels, width, height);
    } catch (...) { wui::setTextMeasurer(nullptr); throw; }
    wui::setTextMeasurer(nullptr);
}
} // namespace
int main(int argc, char** argv)
{
    try { render(argc > 1 ? argv[1] : "fluent_tree_review.ppm", argc > 2 ? std::stof(argv[2]) : 1.0f); return 0; }
    catch (const std::exception& error) { std::cerr << "WhatsUI Fluent Tree visual failure: " << error.what() << '\n'; return 1; }
}
