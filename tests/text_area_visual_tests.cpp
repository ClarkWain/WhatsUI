#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"

#include "wui/paint_context.h"
#include "wui/text_input.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"

namespace {

constexpr int kWidth = 520;
constexpr int kHeight = 740;
constexpr float kLeft = 32.0f;
constexpr float kControlWidth = 456.0f;
constexpr float kControlHeight = 64.0f;
constexpr float kPlaceholderY = 70.0f;
constexpr float kLongTextY = 166.0f;
constexpr float kSelectionY = 262.0f;
constexpr float kCompositionY = 358.0f;
constexpr float kFocusedY = 454.0f;
constexpr float kInvalidY = 550.0f;
constexpr float kDisabledY = 646.0f;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool isColor(const std::vector<unsigned char>& pixels, int x, int y, wui::Color color)
{
    const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
    return offset + 3 < pixels.size()
        && pixels[offset] == color.r && pixels[offset + 1] == color.g
        && pixels[offset + 2] == color.b && pixels[offset + 3] == color.a;
}

bool hasColor(const std::vector<unsigned char>& pixels, int left, int top,
              int right, int bottom, wui::Color color)
{
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            if (isColor(pixels, x, y, color)) return true;
        }
    }
    return false;
}

int blueSelectionBands(const std::vector<unsigned char>& pixels, int left, int top,
                       int right, int bottom)
{
    int bands = 0;
    bool previousRow = false;
    for (int y = top; y < bottom; ++y) {
        bool row = false;
        for (int x = left; x < right; ++x) {
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            if (pixels[offset + 2] > pixels[offset] + 20
                && pixels[offset + 2] > pixels[offset + 1] + 8) {
                row = true;
                break;
            }
        }
        if (row && !previousRow) ++bands;
        previousRow = row;
    }
    return bands;
}

void drawLabel(wui::PaintContext& paint, const std::string& label, float y)
{
    paint.drawText(label, kLeft, y - 5.0f, 12.0f,
                   wui::theme().colors.neutralForeground3, 600);
}

void drawArea(wui::TextArea& area, wui::PaintContext& paint, float y)
{
    area.layout({kLeft, y, kControlWidth, kControlHeight});
    area.prepare(paint);
    area.paint(paint);
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::string output = argc > 1 ? argv[1] : "textarea_fluent_states.ppm";
        auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, kWidth, kHeight);
        expect(canvas && canvas->initializeContext(), "Software Canvas must initialize");
        wui::WhatsCanvasTextMeasurer measurer(*canvas, 1.0f);
        wui::setTextMeasurer(&measurer);
        wui::PaintContext paint(*canvas);

        canvas->beginFrame();
        paint.fillRect({0.0f, 0.0f, static_cast<float>(kWidth), static_cast<float>(kHeight)},
                       wui::theme().colors.neutralBackground2.rest);
        paint.drawText("Fluent TextArea state review", kLeft, 32.0f, 20.0f,
                       wui::theme().colors.neutralForeground1, 600);

        drawLabel(paint, "PLACEHOLDER", kPlaceholderY);
        wui::TextArea placeholder("Describe the task across multiple lines");
        drawArea(placeholder, paint, kPlaceholderY);

        drawLabel(paint, "LONG TEXT + INTERNAL SCROLL", kLongTextY);
        wui::TextArea longText;
        longText.text("Line one\nLine two\nLine three\nLine four\nLine five\nLine six\nLine seven");
        longText.controller().moveToEnd();
        longText.setVisualState(wui::ControlVisualState::Focused, true);
        drawArea(longText, paint, kLongTextY);
        expect(longText.verticalScrollOffset() > 0.0f,
               "Long Software TextArea must scroll to reveal its caret");

        drawLabel(paint, "MULTI-LINE SELECTION", kSelectionY);
        wui::TextArea selection;
        selection.text("Select the first visual line and continue into the second visual line.");
        selection.controller().setSelection({7, 62});
        selection.setVisualState(wui::ControlVisualState::Focused, true);
        drawArea(selection, paint, kSelectionY);

        drawLabel(paint, "IME COMPOSITION", kCompositionY);
        wui::TextArea composition;
        composition.setVisualState(wui::ControlVisualState::Focused, true);
        (void)composition.onCompositionInput({0,
            "Composing text continues across the visual line and wraps into another visible line.",
            wui::CompositionInputEvent::Phase::Start});
        drawArea(composition, paint, kCompositionY);

        drawLabel(paint, "FOCUSED", kFocusedY);
        wui::TextArea focused;
        focused.text("Focused notes");
        focused.setVisualState(wui::ControlVisualState::Focused, true);
        drawArea(focused, paint, kFocusedY);

        drawLabel(paint, "INVALID", kInvalidY);
        wui::TextArea invalid("Required notes");
        invalid.setInvalid(true);
        drawArea(invalid, paint, kInvalidY);

        drawLabel(paint, "DISABLED", kDisabledY);
        wui::TextArea disabled;
        disabled.text("Read-only disabled notes");
        disabled.setEnabled(false);
        disabled.setVisualState(wui::ControlVisualState::Focused, true);
        drawArea(disabled, paint, kDisabledY);

        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        expect(pixels.size() == static_cast<std::size_t>(kWidth * kHeight * 4),
               "Software TextArea capture must contain a complete frame");
        expect(canvas->savePixelsPPM(output), "Software TextArea state artifact must be saved");

        const auto& colors = wui::theme().colors;
        expect(blueSelectionBands(pixels, 38, static_cast<int>(kSelectionY + 4.0f),
                                  480, static_cast<int>(kSelectionY + 62.0f)) >= 2,
               "Selection fill must be visible on multiple wrapped visual lines");
        expect(hasColor(pixels, 38, static_cast<int>(kCompositionY + 4.0f),
                        480, static_cast<int>(kCompositionY + 58.0f), colors.brandForeground1),
               "IME composition must paint its Fluent underline");
        expect(hasColor(pixels, 34, static_cast<int>(kFocusedY + 58.0f),
                        486, static_cast<int>(kFocusedY + 64.0f), colors.brandBackground.rest),
               "Focused TextArea must paint the two-DIP Fluent focus indicator");
        expect(hasColor(pixels, 31, static_cast<int>(kInvalidY),
                        35, static_cast<int>(kInvalidY + 64.0f), colors.statusDanger),
               "Invalid TextArea must paint its semantic danger stroke");
        expect(hasColor(pixels, 38, static_cast<int>(kDisabledY + 6.0f),
                        482, static_cast<int>(kDisabledY + 56.0f), colors.disabled),
               "Disabled TextArea must use the semantic disabled surface");
        expect(!hasColor(pixels, 34, static_cast<int>(kDisabledY + 58.0f),
                         486, static_cast<int>(kDisabledY + 64.0f), colors.brandBackground.rest),
               "Disabled TextArea must suppress a stale focus indicator");
        expect(!hasColor(pixels, 34, static_cast<int>(kDisabledY + 2.0f),
                         486, static_cast<int>(kDisabledY + 58.0f), colors.brandForeground1),
               "Disabled TextArea must suppress a stale caret and composition affordance");

        wui::setTextMeasurer(nullptr);
        std::cout << "WhatsUI TextArea Software visual tests passed: " << output << '\n';
        return 0;
    } catch (const std::exception& error) {
        wui::setTextMeasurer(nullptr);
        std::cerr << "WhatsUI TextArea Software visual tests failed: " << error.what() << '\n';
        return 1;
    }
}
