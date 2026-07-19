#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/navigation.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {
void expect(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void savePpm(const std::string& path, const std::vector<unsigned char>& pixels, int width, int height)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot write Fluent navigation capture");
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
    constexpr int logicalWidth = 760, logicalHeight = 260;
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
        paint.drawText("Fluent navigation", 28, 36, 20, wui::theme().colors.neutralForeground1, 600);

        wui::Toolbar toolbar;
        toolbar.addItem("Cut"); toolbar.addItem("Copy"); toolbar.addItem("Paste", wui::ToolbarItemAppearance::Primary);
        toolbar.layout({28, 54, 260, 32}); toolbar.prepare(paint); toolbar.paint(paint);
        auto* primary = dynamic_cast<wui::ToolbarItem*>(toolbar.children().back().get());
        primary->onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Enter}); primary->paint(paint);
        wui::Toolbar vertical;
        vertical.setOrientation(wui::ToolbarOrientation::Vertical);
        vertical.addItem("New"); vertical.addItem("Open"); vertical.addItem("Save");
        vertical.layout({660, 42, 72, 88}); vertical.prepare(paint); vertical.paint(paint);

        wui::TabList tabs;
        tabs.addTab("overview", "Overview"); tabs.addTab("activity", "Activity"); tabs.addTab("settings", "Settings", false);
        tabs.setValue("activity"); tabs.layout({28, 108, 420, 40}); tabs.prepare(paint); tabs.paint(paint);

        wui::Link link("View all activity"); link.layout({490, 118, 120, 20}); link.paint(paint);

        wui::Breadcrumb breadcrumb;
        breadcrumb.maxVisible(3); breadcrumb.addItem("Home"); breadcrumb.addItem("Projects"); breadcrumb.addItem("WhatsUI"); breadcrumb.addItem("Navigation", true);
        breadcrumb.layout({28, 184, 520, 24}); breadcrumb.prepare(paint); breadcrumb.paint(paint);
        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        expect(!pixels.empty() && hasColor(pixels, wui::theme().colors.brandForeground1) && hasColor(pixels, wui::theme().colors.brandBackground.rest),
               "Navigation matrix must contain Fluent brand selection and primary command colors");
        savePpm(output, pixels, width, height);
    } catch (...) { wui::setTextMeasurer(nullptr); throw; }
    wui::setTextMeasurer(nullptr);
}
} // namespace

int main(int argc, char** argv)
{
    try { render(argc > 1 ? argv[1] : "fluent_navigation_review.ppm", argc > 2 ? std::stof(argv[2]) : 1.0f); return 0; }
    catch (const std::exception& error) { std::cerr << "Fluent navigation visual test failure: " << error.what() << '\n'; return 1; }
}
