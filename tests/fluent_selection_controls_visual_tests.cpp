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

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void savePpm(const std::string& path, const std::vector<unsigned char>& rgba, int width, int height)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot save selection visual review");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        output.put(static_cast<char>(rgba[i])); output.put(static_cast<char>(rgba[i + 1])); output.put(static_cast<char>(rgba[i + 2]));
    }
}

bool colorAt(const std::vector<unsigned char>& pixels, int width, float scale,
             float logicalX, float logicalY, wui::Color color)
{
    const int x = std::clamp(
        static_cast<int>(std::floor(logicalX * scale)), 0, width - 1);
    const int height =
        static_cast<int>(pixels.size() / (static_cast<std::size_t>(width) * 4));
    const int y = std::clamp(
        static_cast<int>(std::floor(logicalY * scale)), 0, height - 1);
    const std::size_t offset =
        (static_cast<std::size_t>(y) * width + x) * 4;
    return pixels[offset] == color.r && pixels[offset + 1] == color.g &&
           pixels[offset + 2] == color.b && pixels[offset + 3] == color.a;
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
        list.setSelectedIndex(3); list.setMaxVisibleOptions(2); list.layout({28, 98, 316, 120}); list.setScrollOffset(112); list.prepare(paint); list.paint(paint);
        paint.drawText("Dropdown", 402, 82, 14, wui::theme().colors.neutralForeground1, 600);
        wui::Dropdown dropdown("Choose a project"); dropdown.addOption({"whatsui", "WhatsUI"}).addOption({"canvas", "WhatsCanvas"}); dropdown.setMultiselect(true); dropdown.setSelectedIndices({0, 1}); draw(dropdown, paint, {402, 98, 300, 32});
        paint.drawText("Editable combobox", 402, 172, 14, wui::theme().colors.neutralForeground1, 600);
        wui::Combobox combo("Search people"); combo.text("Ada");
        combo.setVisualState(wui::ControlVisualState::Focused, true);
        draw(combo, paint, {402, 188, 300, 32});

        paint.drawText("Dropdown states", 28, 246, 14,
                       wui::theme().colors.neutralForeground1, 600);
        wui::Dropdown rest("Rest");
        draw(rest, paint, {28, 258, 128, 32});
        wui::Dropdown hover("Hover");
        hover.setVisualState(wui::ControlVisualState::Hovered, true);
        draw(hover, paint, {168, 258, 128, 32});
        wui::Dropdown pressed("Pressed");
        pressed.setVisualState(wui::ControlVisualState::Pressed, true);
        draw(pressed, paint, {308, 258, 128, 32});
        wui::Dropdown focused("Focus");
        focused.setVisualState(wui::ControlVisualState::Focused, true);
        draw(focused, paint, {448, 258, 128, 32});
        wui::Dropdown disabled("Disabled");
        disabled.setEnabled(false);
        draw(disabled, paint, {588, 258, 128, 32});

        paint.drawText("Windowed rows, multi-select summary and disabled options use Fluent semantics.", 28, 390, 13, wui::theme().colors.neutralForeground2, 400);
        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        if (pixels.size() != static_cast<std::size_t>(width * height * 4)) throw std::runtime_error("invalid software capture");
        const float dropdownStroke =
            paint.snapStrokeWidth(wui::theme().stroke.thin);
        expect(colorAt(pixels, width, scale, 552.0f,
                       98.0f + 32.0f - dropdownStroke * 0.5f,
                       wui::theme().colors.neutralStrokeAccessible),
               "Dropdown must retain its full-width accessible bottom stroke");
        expect(colorAt(pixels, width, scale, 54.0f, 184.0f,
                       wui::theme().colors.neutralBackground1.selected),
               "Selected ListBox row must use the Fluent selected surface");
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
