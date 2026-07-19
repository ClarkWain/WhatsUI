#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/accordion.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {
void expect(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void savePpm(const std::string& path, const std::vector<unsigned char>& pixels, int width, int height)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot write Fluent accordion capture");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
        output.put(static_cast<char>(pixels[i])); output.put(static_cast<char>(pixels[i + 1])); output.put(static_cast<char>(pixels[i + 2]));
    }
}
bool hasColor(const std::vector<unsigned char>& pixels, wui::Color color)
{
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4)
        if (pixels[i] == color.r && pixels[i + 1] == color.g && pixels[i + 2] == color.b && pixels[i + 3] == color.a) return true;
    return false;
}
void render(const std::string& output, float scale)
{
    constexpr int logicalWidth = 720, logicalHeight = 390;
    scale = std::max(1.0f, scale);
    const int width = static_cast<int>(std::lround(logicalWidth * scale));
    const int height = static_cast<int>(std::lround(logicalHeight * scale));
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
    expect(canvas && canvas->initializeContext(), "Software canvas must initialize");
    wui::WhatsCanvasTextMeasurer measurer(*canvas, scale); wui::setTextMeasurer(&measurer);
    try {
        wui::PaintContext paint(*canvas, scale);
        canvas->beginFrame();
        paint.fillRect({0, 0, static_cast<float>(logicalWidth), static_cast<float>(logicalHeight)}, wui::theme().colors.neutralBackground2.rest);
        paint.drawText("Fluent Accordion", 36, 44, 24, wui::theme().colors.neutralForeground1, 600);

        wui::Accordion accordion;
        accordion.accessibleLabel("Settings sections");
        auto& account = accordion.addItem("Account settings", "Manage your sign-in preferences and recovery information.");
        auto& appearance = accordion.addItem("Appearance", "Choose a theme and your preferred content density.");
        accordion.addItem("Advanced", "Diagnostics and developer-specific settings.").setEnabled(false);
        account.setExpanded(true);
        accordion.layout({36, 66, 430, 232});
        accordion.prepare(paint); accordion.paint(paint);
        appearance.onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Enter, wui::MouseButton::None, {180, 186}});
        appearance.paint(paint);

        wui::Accordion multiple;
        multiple.setExpandMode(wui::AccordionExpandMode::Multiple);
        auto& one = multiple.addItem("Notifications", "Control which status messages are surfaced.");
        auto& two = multiple.addItem("Privacy", "Review permissions available to this application.");
        one.setExpanded(true); two.setExpanded(true);
        multiple.layout({500, 66, 184, 232});
        multiple.prepare(paint); multiple.paint(paint);
        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        expect(!pixels.empty() && hasColor(pixels, wui::theme().colors.neutralForeground1) &&
                   hasColor(pixels, wui::theme().colors.neutralStroke1),
               "Accordion capture must contain Fluent text and separator primitives");
        savePpm(output, pixels, width, height);
    } catch (...) { wui::setTextMeasurer(nullptr); throw; }
    wui::setTextMeasurer(nullptr);
}
} // namespace

int main(int argc, char** argv)
{
    try { render(argc > 1 ? argv[1] : "fluent_accordion_review.ppm", argc > 2 ? std::stof(argv[2]) : 1.0f); return 0; }
    catch (const std::exception& error) { std::cerr << "Fluent accordion visual test failure: " << error.what() << '\n'; return 1; }
}
