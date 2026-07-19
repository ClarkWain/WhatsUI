#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/selection.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {

void savePpm(const std::string& path, const std::vector<unsigned char>& rgba, int width, int height)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot save selection visual review");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        output.put(static_cast<char>(rgba[i])); output.put(static_cast<char>(rgba[i + 1])); output.put(static_cast<char>(rgba[i + 2]));
    }
}

void draw(wui::Node& node, wui::PaintContext& paint, wui::RectF bounds)
{
    node.layout(bounds); node.prepare(paint); node.paint(paint);
}

void render(const std::string& output, float scale)
{
    constexpr int logicalWidth = 760;
    constexpr int logicalHeight = 430;
    scale = std::max(1.0f, scale);
    const int width = static_cast<int>(std::lround(logicalWidth * scale));
    const int height = static_cast<int>(std::lround(logicalHeight * scale));
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
    if (!canvas || !canvas->initializeContext()) throw std::runtime_error("software canvas unavailable");
    wui::WhatsCanvasTextMeasurer text(*canvas, scale); wui::setTextMeasurer(&text);
    try {
        wui::PaintContext paint(*canvas, scale); canvas->beginFrame();
        paint.fillRect({0, 0, static_cast<float>(logicalWidth), static_cast<float>(logicalHeight)}, wui::theme().colors.neutralBackground2.rest);
        paint.drawText("Fluent selection controls", 28, 42, 24, wui::theme().colors.neutralForeground1, 600);
        paint.drawText("ListBox", 28, 82, 14, wui::theme().colors.neutralForeground1, 600);
        wui::ListBox list(std::vector<wui::Option>{
            wui::Option{"meeting", "Team meeting", std::string("Tomorrow, 10:00")},
            wui::Option{"review", "Design review", std::string("Friday, 14:30")},
            wui::Option{"archived", "Archived task", false},
            wui::Option{"planning", "Planning session", std::string("Next week")},
            wui::Option{"retro", "Retrospective", std::string("Friday")}});
        list.setSelectedIndex(3); list.setMaxVisibleOptions(2); list.layout({28, 98, 316, 114}); list.setScrollOffset(104); list.prepare(paint); list.paint(paint);
        paint.drawText("Dropdown", 402, 82, 14, wui::theme().colors.neutralForeground1, 600);
        wui::Dropdown dropdown("Choose a project"); dropdown.addOption({"whatsui", "WhatsUI"}).addOption({"canvas", "WhatsCanvas"}); dropdown.setMultiselect(true); dropdown.setSelectedIndices({0, 1}); draw(dropdown, paint, {402, 98, 300, 32});
        paint.drawText("Editable combobox", 402, 172, 14, wui::theme().colors.neutralForeground1, 600);
        wui::Combobox combo("Search people"); combo.text("Ada"); draw(combo, paint, {402, 188, 300, 32});
        paint.drawText("Windowed rows, multi-select summary and disabled options use Fluent semantics.", 28, 342, 13, wui::theme().colors.neutralForeground2, 400);
        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        if (pixels.size() != static_cast<std::size_t>(width * height * 4)) throw std::runtime_error("invalid software capture");
        savePpm(output, pixels, width, height);
    } catch (...) { wui::setTextMeasurer(nullptr); throw; }
    wui::setTextMeasurer(nullptr);
}

} // namespace

int main(int argc, char** argv)
{
    try { render(argc > 1 ? argv[1] : "fluent_selection_review.ppm", argc > 2 ? std::stof(argv[2]) : 1.0f); return 0; }
    catch (const std::exception&) { return 1; }
}
