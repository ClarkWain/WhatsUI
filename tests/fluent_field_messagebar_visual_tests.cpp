#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/form_feedback.h"
#include "wui/paint_context.h"
#include "wui/text_input.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {
void savePpm(const std::string& path, const std::vector<unsigned char>& rgba, int width, int height)
{
    std::ofstream out(path, std::ios::binary); if (!out) throw std::runtime_error("cannot save Fluent Field review");
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) { out.put(static_cast<char>(rgba[i])); out.put(static_cast<char>(rgba[i+1])); out.put(static_cast<char>(rgba[i+2])); }
}
void draw(wui::Node& n, wui::PaintContext& p, wui::RectF b) { n.layout(b); n.prepare(p); n.paint(p); }
}
int main(int argc, char** argv)
{
    try {
        const float scale = argc > 2 ? std::max(1.f, std::stof(argv[2])) : 1.f;
        constexpr int logicalWidth = 760, logicalHeight = 500;
        const int width = static_cast<int>(std::lround(logicalWidth * scale)); const int height = static_cast<int>(std::lround(logicalHeight * scale));
        auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height); if (!canvas || !canvas->initializeContext()) return 2;
        wui::WhatsCanvasTextMeasurer text(*canvas, scale); wui::setTextMeasurer(&text); wui::PaintContext paint(*canvas, scale);
        canvas->beginFrame(); paint.fillRect({0,0,float(logicalWidth),float(logicalHeight)}, wui::theme().colors.neutralBackground2.rest);
        paint.drawText("Fluent Field and MessageBar", 28, 42, 24, wui::theme().colors.neutralForeground1, 600);
        wui::Field normal("Task title"); normal.setHint("Keep it short and actionable"); normal.setRequired(true); normal.setControl(std::make_unique<wui::TextInput>("Add a title")); draw(normal, paint, {28,74,330,96});
        wui::Field invalid("Project name"); invalid.setOrientation(wui::FieldOrientation::Horizontal); invalid.setValidationState(wui::FieldValidationState::Error); invalid.setValidationMessage("This field is required"); invalid.setControl(std::make_unique<wui::TextInput>("Required")); draw(invalid, paint, {400,74,330,88});
        wui::MessageBar info("Your changes are saved automatically."); info.setTitle("Tip"); info.setDismissible(true); draw(info, paint, {28,210,704,64});
        wui::MessageBar warning("This action cannot be undone. Review the selected tasks before continuing."); warning.setTitle("Review changes"); warning.setIntent(wui::MessageBarIntent::Warning); warning.setMultiline(true); warning.addAction({"Undo", {}}); warning.setDismissible(true); draw(warning, paint, {28,302,704,112});
        canvas->endFrame(); const auto pixels = canvas->readPixelsRGBA(); if (pixels.size()!=static_cast<std::size_t>(width*height*4)) return 3;
        const int sampleX=static_cast<int>(std::lround(30*scale)), sampleY=static_cast<int>(std::lround(230*scale)); const auto o=static_cast<std::size_t>((sampleY*width+sampleX)*4);
        if (o+3>=pixels.size() || pixels[o+3]!=255) return 4;
        savePpm(argc>1?argv[1]:"fluent_field_messagebar.ppm",pixels,width,height); wui::setTextMeasurer(nullptr); return 0;
    } catch (...) { wui::setTextMeasurer(nullptr); return 1; }
}
