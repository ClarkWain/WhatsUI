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
void render(const std::string& output, float scale)
{
    constexpr int logicalWidth = 760, logicalHeight = 420;
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
        toolbar.addItem("Cut");
        toolbar.addItem("Copy").setVisualState(wui::ControlVisualState::Hovered, true);
        toolbar.addItem("Paste", wui::ToolbarItemAppearance::Primary);
        toolbar.layout({28, 54, 260, 40}); toolbar.prepare(paint); toolbar.paint(paint);
        auto* primary = dynamic_cast<wui::ToolbarItem*>(toolbar.children().back().get());
        primary->onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Enter}); primary->paint(paint);
        wui::Toolbar vertical;
        vertical.setOrientation(wui::ToolbarOrientation::Vertical);
        vertical.addItem("New"); vertical.addItem("Open"); vertical.addItem("Save");
        vertical.layout({660, 42, 72, 112}); vertical.prepare(paint); vertical.paint(paint);

        wui::TabList tabs;
        tabs.addTab("overview", "Overview"); tabs.addTab("activity", "Activity"); tabs.addTab("settings", "Settings", false);
        tabs.setValue("activity");
        dynamic_cast<wui::Tab*>(tabs.children().front().get())
            ->setVisualState(wui::ControlVisualState::Hovered, true);
        tabs.layout({28, 108, 420, 44}); tabs.prepare(paint); tabs.paint(paint);

        wui::Link link("View all activity");
        link.setVisualState(wui::ControlVisualState::Hovered, true);
        link.layout({490, 118, 120, 20}); link.paint(paint);

        wui::Breadcrumb breadcrumb;
        breadcrumb.maxVisible(3); breadcrumb.addItem("Home"); breadcrumb.addItem("Projects"); breadcrumb.addItem("WhatsUI"); breadcrumb.addItem("Navigation", true);
        dynamic_cast<wui::BreadcrumbItem*>(
            breadcrumb.children().front().get())
            ->setVisualState(wui::ControlVisualState::Hovered, true);
        breadcrumb.layout({28, 184, 520, 32}); breadcrumb.prepare(paint); breadcrumb.paint(paint);

        paint.drawText("Tab states", 28, 262, 14,
                       wui::theme().colors.neutralForeground1, 600);
        wui::TabList stateTabs;
        stateTabs.addTab("rest", "Rest");
        stateTabs.addTab("hover", "Hover");
        stateTabs.addTab("pressed", "Pressed");
        stateTabs.addTab("focus", "Focus");
        stateTabs.addTab("disabled", "Disabled", false);
        stateTabs.setValue("focus");
        dynamic_cast<wui::Tab*>(stateTabs.children()[1].get())
            ->setVisualState(wui::ControlVisualState::Hovered, true);
        dynamic_cast<wui::Tab*>(stateTabs.children()[2].get())
            ->setVisualState(wui::ControlVisualState::Pressed, true);
        dynamic_cast<wui::Tab*>(stateTabs.children()[3].get())
            ->setVisualState(wui::ControlVisualState::Focused, true);
        stateTabs.layout({28, 272, 520, 44});
        stateTabs.prepare(paint);
        stateTabs.paint(paint);

        paint.drawText("Link states", 28, 354, 14,
                       wui::theme().colors.neutralForeground1, 600);
        wui::Link restLink("Rest");
        restLink.layout({28, 364, 52, 20}); restLink.paint(paint);
        wui::Link hoverLink("Hover");
        hoverLink.setVisualState(wui::ControlVisualState::Hovered, true);
        hoverLink.layout({106, 364, 52, 20}); hoverLink.paint(paint);
        wui::Link pressedLink("Pressed");
        pressedLink.setVisualState(wui::ControlVisualState::Pressed, true);
        pressedLink.layout({184, 364, 60, 20}); pressedLink.paint(paint);
        wui::Link focusLink("Focus");
        focusLink.setVisualState(wui::ControlVisualState::Focused, true);
        focusLink.layout({270, 364, 52, 20}); focusLink.paint(paint);
        wui::Link disabledLink("Disabled");
        disabledLink.setEnabled(false);
        disabledLink.layout({348, 364, 68, 20}); disabledLink.paint(paint);
        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        expect(!pixels.empty() && hasColor(pixels, wui::theme().colors.brandForeground1) && hasColor(pixels, wui::theme().colors.brandBackground.rest),
               "Navigation matrix must contain Fluent brand selection and primary command colors");
        const auto selectedBounds = tabs.children()[1]->bounds();
        expect(colorAt(pixels, width, scale,
                       selectedBounds.x + selectedBounds.width * 0.5f,
                       selectedBounds.y + selectedBounds.height - 1.5f,
                       wui::theme().colors.compoundBrandStroke.rest),
               "Selected Tab must paint the exact 3-DIP brand indicator");
        const auto hoveredBounds = tabs.children()[0]->bounds();
        expect(colorAt(pixels, width, scale,
                       hoveredBounds.x + hoveredBounds.width * 0.5f,
                       hoveredBounds.y + hoveredBounds.height - 1.5f,
                       wui::theme().colors.neutralStroke1Hover),
               "Hovered unselected Tab must paint the neutral 3-DIP indicator");
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
