// Pixel-level contract for Windows IME pre-edit rendering.  This deliberately
// uses the real WhatsCanvas Software backend: a model-only assertion cannot
// prove that the composition range is visible or that it stops rendering once
// the native composition session ends.

#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "wsc/Canvas.h"

#include "wui/paint_context.h"
#include "wui/text_input.h"
#include "wui/theme.h"

namespace {

constexpr int kWidth = 160;
constexpr int kHeight = 48;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool isColor(const std::vector<unsigned char>& pixels, int x, int y, wui::Color color)
{
    const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
    return offset + 3 < pixels.size()
        && pixels[offset] == color.r
        && pixels[offset + 1] == color.g
        && pixels[offset + 2] == color.b
        && pixels[offset + 3] == color.a;
}

std::vector<unsigned char> render(wui::TextInput& input)
{
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, kWidth, kHeight);
    expect(canvas && canvas->initializeContext(), "Software canvas must initialize for IME visual rendering");

    wui::PaintContext paint(*canvas);
    input.prepare(paint);
    canvas->beginFrame();
    input.paint(paint);
    canvas->endFrame();

    auto pixels = canvas->readPixelsRGBA();
    expect(pixels.size() == static_cast<std::size_t>(kWidth * kHeight * 4),
           "Software capture must return a complete RGBA frame");
    return pixels;
}

bool hasColorInRect(const std::vector<unsigned char>& pixels, int left, int top, int right, int bottom, wui::Color color)
{
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            if (isColor(pixels, x, y, color)) return true;
        }
    }
    return false;
}

bool isTransparentInRect(const std::vector<unsigned char>& pixels, int left, int top, int right, int bottom)
{
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const auto offset = static_cast<std::size_t>((y * kWidth + x) * 4);
            if (offset + 3 < pixels.size() && pixels[offset + 3] != 0) return false;
        }
    }
    return true;
}

void testCompositionUsesUnderlineAndClearsOnEnd()
{
    wui::TextInput input;
    input.text("abcd");
    input.layout({4.0f, 4.0f, 144.0f, 32.0f});
    input.setVisualState(wui::ControlVisualState::Focused, true);
    input.controller().setCaret(1);

    expect(input.onCompositionInput({0, "xy", wui::CompositionInputEvent::Phase::Start}),
           "Composition start must update the focused text input");
    const auto& composition = input.controller().composition();
    expect(composition.start == 1 && composition.end == 3,
           "Composition update must expose the exact visual underline range");

    const auto active = render(input);
    const auto& colors = wui::theme().colors;
    // TextInput places its baseline at y=25 for these fixed bounds and the
    // composition marker at baseline + focusInset.  The pre-edit span starts
    // after one 14px Fluent body glyph (x~24) and ends after two more (x~39).
    expect(hasColorInRect(active, 22, 26, 39, 30, colors.focus),
           "Active composition must paint a focused underline under its pre-edit span");
    // The upper text band is intentionally outside the underline. A normal
    // selection fill would paint the whole control height here; pre-edit must
    // not masquerade as selection highlight.
    expect(!hasColorInRect(active, 22, 8, 39, 20, colors.focus),
           "Composition must not use a selection-style focus highlight");

    expect(input.onCompositionInput({0, "", wui::CompositionInputEvent::Phase::End}),
           "Composition end must route to the text input");
    expect(input.controller().composition().empty(),
           "Composition end must clear the active pre-edit range");
    expect(input.controller().selection().empty() && input.controller().selection().end == 3,
           "Composition end must collapse the transient pre-edit selection at its caret");
    const auto ended = render(input);
    // Exclude the collapsed caret at the pre-edit end; only the former span
    // itself is relevant to verifying underline cleanup.
    expect(!hasColorInRect(ended, 22, 26, 39, 30, colors.focus),
           "Composition underline must clear after the pre-edit session ends");
    expect(!hasColorInRect(ended, 22, 8, 39, 20, colors.focus),
           "Ended composition must not leave a selection-style pre-edit highlight behind");
}

void testLongTextClipsAndKeepsCaretVisible()
{
    wui::TextInput input;
    input.text("A long task title that must stay inside this compact field");
    input.layout({4.0f, 4.0f, 72.0f, 32.0f});
    input.setVisualState(wui::ControlVisualState::Focused, true);
    input.controller().moveToEnd();

    const auto pixels = render(input);
    // The control ends at x=76. Any text/caret escaping its clipped inner
    // viewport would leave non-transparent pixels in this untouched canvas.
    expect(isTransparentInRect(pixels, 76, 4, kWidth, 36),
           "Long TextInput content must not paint beyond its outer bounds");
    const auto& colors = wui::theme().colors;
    expect(hasColorInRect(pixels, 56, 6, 65, 34, colors.focus),
           "Long TextInput must horizontally reveal the active caret near the viewport edge");

    const auto caret = input.caretRect();
    expect(caret.x >= 16.0f && caret.x <= 64.0f,
           "IME caret rectangle must use the same clipped horizontal viewport as painting");
}

} // namespace

int main()
{
    try {
        testCompositionUsesUnderlineAndClearsOnEnd();
        testLongTextClipsAndKeepsCaretVisible();
        std::cout << "WhatsUI TextInput composition visual tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WhatsUI TextInput composition visual tests failed: " << error.what() << '\n';
        return 1;
    }
}
