#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"
#include "wui/overlays.h"
#include "wui/paint_context.h"
#include "wui/runtime.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {

constexpr int width = 760;
constexpr int height = 500;

void savePpm(const std::string& path, const std::vector<unsigned char>& rgba)
{
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot create MenuButton visual review");
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        out.put(static_cast<char>(rgba[i]));
        out.put(static_cast<char>(rgba[i + 1]));
        out.put(static_cast<char>(rgba[i + 2]));
    }
}

void caption(wui::PaintContext& paint, const std::string& value, float x, float y)
{
    paint.drawText(value, x, y, 12.0f, wui::theme().colors.neutralForeground3, 600);
}

template <typename T>
void draw(T& control, wui::PaintContext& paint, const wui::RectF& bounds)
{
    control.layout(bounds);
    control.prepare(paint);
    control.paint(paint);
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::string output = argc > 1 ? argv[1] : "fluent_menu_button_review.ppm";
        auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
        if (!canvas || !canvas->initializeContext()) return 2;
        wui::WhatsCanvasTextMeasurer measurer(*canvas, 1.0f);
        wui::setTextMeasurer(&measurer);
        wui::PaintContext paint(*canvas);

        canvas->beginFrame();
        paint.fillRect({0, 0, static_cast<float>(width), static_cast<float>(height)},
                       wui::theme().colors.neutralBackground2.rest);
        paint.drawText("Fluent menu command states", 28, 42, 24.0f,
                       wui::theme().colors.neutralForeground1, 600);

        caption(paint, "REST", 28, 72);
        caption(paint, "HOVER", 150, 72);
        caption(paint, "PRESSED", 272, 72);
        caption(paint, "FOCUSED", 394, 72);
        caption(paint, "DISABLED", 516, 72);
        wui::MenuButton rest("More"), hover("More"), pressed("More"), focused("More"), disabled("More");
        hover.setVisualState(wui::ControlVisualState::Hovered, true);
        pressed.setVisualState(wui::ControlVisualState::Pressed, true);
        focused.setVisualState(wui::ControlVisualState::Focused, true);
        disabled.setEnabled(false);
        draw(rest, paint, {28, 84, 104, 32});
        draw(hover, paint, {150, 84, 104, 32});
        draw(pressed, paint, {272, 84, 104, 32});
        draw(focused, paint, {394, 84, 104, 32});
        draw(disabled, paint, {516, 84, 104, 32});

        caption(paint, "MENUBUTTON OPEN / FIRST ENABLED ITEM FOCUSED", 28, 154);
        wui::OverlayHost menuHost;
        wui::MenuButton menuButton("More options");
        menuButton.bindOverlayHost(menuHost)
            .addItem({"Unavailable", "", false, {}})
            .addItem({"Rename", "F2", true, {}})
            .addItem({"Delete", "Del", true, {}});
        draw(menuButton, paint, {28, 168, 150, 32});
        if (menuButton.performAccessibilityAction(wui::AccessibilityActionKind::Expand, {})
            != wui::AccessibilityActionStatus::Succeeded) return 3;
        // Repaint the owner after Expand so its retained pressed/open surface
        // and the anchored popup are captured in the same Software frame.
        menuButton.paint(paint);
        menuHost.layout({0, 0, static_cast<float>(width), static_cast<float>(height)});
        menuHost.prepare(paint);
        menuHost.paint(paint);

        caption(paint, "SPLITBUTTON OPEN / DISCLOSURE IS INDEPENDENT", 390, 154);
        wui::OverlayHost splitHost;
        wui::SplitButton split("Save");
        split.bindOverlayHost(splitHost)
            .onClick([] {})
            .addItem({"Save as", "Ctrl+Shift+S", true, {}})
            .addItem({"Save a copy", "", true, {}});
        draw(split, paint, {390, 168, 174, 32});
        if (split.performAccessibilityAction(wui::AccessibilityActionKind::Expand, {})
            != wui::AccessibilityActionStatus::Succeeded) return 4;
        split.paint(paint);
        splitHost.layout({0, 0, static_cast<float>(width), static_cast<float>(height)});
        splitHost.prepare(paint);
        splitHost.paint(paint);

        caption(paint, "The open owner remains pressed; Escape/item invoke restores owner focus.",
                28, 462);
        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        if (pixels.size() != static_cast<std::size_t>(width * height * 4)) return 5;
        savePpm(output, pixels);
        wui::setTextMeasurer(nullptr);
        return 0;
    } catch (...) {
        wui::setTextMeasurer(nullptr);
        return 1;
    }
}
