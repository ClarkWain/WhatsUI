#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/paint_context.h"
#include "wui/table.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {
void expect(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void savePpm(const std::string& path, const std::vector<unsigned char>& pixels, int width, int height)
{
    std::ofstream stream(path, std::ios::binary); if (!stream) throw std::runtime_error("Cannot write table visual capture");
    stream << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) { stream.put(static_cast<char>(pixels[i])); stream.put(static_cast<char>(pixels[i + 1])); stream.put(static_cast<char>(pixels[i + 2])); }
}
bool hasColor(const std::vector<unsigned char>& pixels, wui::Color color)
{
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) if (pixels[i] == color.r && pixels[i + 1] == color.g && pixels[i + 2] == color.b && pixels[i + 3] == color.a) return true;
    return false;
}
bool pixelIs(const std::vector<unsigned char>& pixels, int width, float scale,
             float x, float y, wui::Color color)
{
    const int px = std::clamp(static_cast<int>(std::lround(x * scale)), 0, width - 1);
    const int py = std::max(0, static_cast<int>(std::lround(y * scale)));
    const std::size_t offset = (static_cast<std::size_t>(py) * static_cast<std::size_t>(width) +
                                static_cast<std::size_t>(px)) * 4;
    return offset + 3 < pixels.size() && pixels[offset] == color.r &&
           pixels[offset + 1] == color.g && pixels[offset + 2] == color.b &&
           pixels[offset + 3] == color.a;
}
std::vector<wui::TableColumn> columns()
{
    // Keep all three columns in each side-by-side review surface. This is a
    // visual matrix, not a truncation test; behavioural tests cover windowing.
    return {{"task", "Task", 142, 76, wui::TableColumnAlignment::Start, true}, {"state", "State", 92, 64, wui::TableColumnAlignment::Center, true}, {"owner", "Owner", 0, 64, wui::TableColumnAlignment::End, false}};
}
std::vector<wui::TableRow> rows()
{
    return {{"1", {"Audit matrix", "In progress", "Mina"}}, {"2", {"Capture review", "Ready", "Owen"}}, {"3", {"Grid docs", "Blocked", "Sam"}}, {"4", {"Run regression", "Ready", "June"}}, {"5", {"Disabled row", "Blocked", "--"}, false}};
}
void render(const std::string& path, float scale)
{
    constexpr int logicalWidth = 760, logicalHeight = 330;
    const int width = static_cast<int>(std::lround(logicalWidth * scale)), height = static_cast<int>(std::lround(logicalHeight * scale));
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height); expect(canvas && canvas->initializeContext(), "Software canvas must initialize");
    wui::WhatsCanvasTextMeasurer measurer(*canvas, scale); wui::setTextMeasurer(&measurer);
    try {
        canvas->beginFrame(); wui::PaintContext paint(*canvas, scale);
        paint.fillRect({0, 0, static_cast<float>(logicalWidth), static_cast<float>(logicalHeight)}, wui::theme().colors.neutralBackground2.rest);
        paint.drawText("Fluent Table and DataGrid", 28, 38, 20, wui::theme().colors.neutralForeground1, 600);
        wui::Table table(columns()); table.setRows(rows()).maxVisibleRows(3).accessibleLabel("Passive release work");
        table.layout({28, 64, 340, 164}); table.prepare(paint); table.paint(paint);
        paint.drawText("Table", 28, 254, 12, wui::theme().colors.neutralForeground2, 600);
        wui::DataGrid grid; grid.setColumns(columns()).setRows(rows()).maxVisibleRows(4); grid.sortBy(0);
        grid.onKeyEvent({0, wui::KeyAction::Down, 40}); grid.onKeyEvent({0, wui::KeyAction::Down, 32});
        grid.layout({402, 64, 330, 204}); grid.prepare(paint); grid.paint(paint);
        paint.drawText("DataGrid · sorted, selected, focused", 402, 294, 12, wui::theme().colors.neutralForeground2, 600);
        canvas->endFrame(); const auto pixels = canvas->readPixelsRGBA();
        expect(!pixels.empty() && hasColor(pixels, wui::theme().colors.neutralStroke1) && hasColor(pixels, wui::theme().colors.neutralBackground1.selected),
               "Table/DataGrid visual matrix must paint separators and a Fluent selected row");
        expect(pixelIs(pixels, width, scale, 590, 144,
                       wui::theme().colors.neutralBackground1.selected),
               "DataGrid selected state must fill the complete 44-DIP row outside the focused cell");
        savePpm(path, pixels, width, height);
    } catch (...) { wui::setTextMeasurer(nullptr); throw; }
    wui::setTextMeasurer(nullptr);
}
}
int main(int argc, char** argv)
{
    try { render(argc > 1 ? argv[1] : "fluent_table_review.ppm", argc > 2 ? std::stof(argv[2]) : 1.0f); return 0; }
    catch (const std::exception& error) { std::cerr << "Fluent table visual test failure: " << error.what() << '\n'; return 1; }
}
