#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "wsc/Canvas.h"

#include "wui/accessibility.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"
#include "wui/widgets.h"

namespace {

constexpr int kWidth = 900;
constexpr int kHeight = 380;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

wui::PointerEvent pointer(wui::PointerAction action, wui::PointF position,
                          wui::MouseButton button = wui::MouseButton::None)
{
    wui::PointerEvent event;
    event.action = action;
    event.position = position;
    event.button = button;
    return event;
}

void savePpm(const std::string& path, const std::vector<unsigned char>& rgba,
             int width, int height)
{
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot create Checkbox visual capture");
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t index = 0; index + 3 < rgba.size(); index += 4) {
        out.put(static_cast<char>(rgba[index]));
        out.put(static_cast<char>(rgba[index + 1]));
        out.put(static_cast<char>(rgba[index + 2]));
    }
}

bool pixelIs(const std::vector<unsigned char>& rgba, int width, float scale,
             float logicalX, float logicalY, wui::Color color)
{
    const int x = static_cast<int>(std::lround(logicalX * scale));
    const int y = static_cast<int>(std::lround(logicalY * scale));
    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
    return offset + 3 < rgba.size() && rgba[offset] == color.r &&
        rgba[offset + 1] == color.g && rgba[offset + 2] == color.b &&
        rgba[offset + 3] == color.a;
}

bool regionContainsColor(const std::vector<unsigned char>& rgba, int width, float scale,
                         wui::RectF logicalRegion, wui::Color color)
{
    const int left = static_cast<int>(std::floor(logicalRegion.x * scale));
    const int top = static_cast<int>(std::floor(logicalRegion.y * scale));
    const int right = static_cast<int>(std::ceil((logicalRegion.x + logicalRegion.width) * scale));
    const int bottom = static_cast<int>(std::ceil((logicalRegion.y + logicalRegion.height) * scale));
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const auto offset = static_cast<std::size_t>((y * width + x) * 4);
            if (offset + 3 < rgba.size() && rgba[offset] == color.r &&
                rgba[offset + 1] == color.g && rgba[offset + 2] == color.b &&
                rgba[offset + 3] == color.a) return true;
        }
    }
    return false;
}

bool regionContainsColorNear(const std::vector<unsigned char>& rgba, int width,
                             float scale, wui::RectF logicalRegion,
                             wui::Color color, int tolerance)
{
    const int left = static_cast<int>(std::floor(logicalRegion.x * scale));
    const int top = static_cast<int>(std::floor(logicalRegion.y * scale));
    const int right = static_cast<int>(
        std::ceil((logicalRegion.x + logicalRegion.width) * scale));
    const int bottom = static_cast<int>(
        std::ceil((logicalRegion.y + logicalRegion.height) * scale));
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const auto offset = static_cast<std::size_t>((y * width + x) * 4);
            if (offset + 3 < rgba.size() &&
                std::abs(static_cast<int>(rgba[offset]) - color.r) <= tolerance &&
                std::abs(static_cast<int>(rgba[offset + 1]) - color.g) <= tolerance &&
                std::abs(static_cast<int>(rgba[offset + 2]) - color.b) <= tolerance)
                return true;
        }
    }
    return false;
}

struct VerticalPixelBounds {
    int top{0};
    int bottom{-1};
    [[nodiscard]] bool valid() const noexcept { return bottom >= top; }
    [[nodiscard]] float center() const noexcept {
        return (static_cast<float>(top) + static_cast<float>(bottom)) * 0.5f;
    }
};

VerticalPixelBounds verticalBoundsNearColor(
    const std::vector<unsigned char>& rgba, int width, float scale,
    wui::RectF logicalRegion, wui::Color color, int tolerance)
{
    const int left = static_cast<int>(std::ceil(logicalRegion.x * scale));
    const int top = static_cast<int>(std::ceil(logicalRegion.y * scale));
    const int right = static_cast<int>(
        std::floor((logicalRegion.x + logicalRegion.width) * scale));
    const int bottom = static_cast<int>(
        std::floor((logicalRegion.y + logicalRegion.height) * scale));
    VerticalPixelBounds result;
    result.top = bottom;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            const auto offset = static_cast<std::size_t>((y * width + x) * 4);
            if (offset + 3 >= rgba.size()) continue;
            if (std::abs(static_cast<int>(rgba[offset]) - color.r) <= tolerance &&
                std::abs(static_cast<int>(rgba[offset + 1]) - color.g) <= tolerance &&
                std::abs(static_cast<int>(rgba[offset + 2]) - color.b) <= tolerance) {
                result.top = std::min(result.top, y);
                result.bottom = std::max(result.bottom, y);
            }
        }
    }
    return result;
}

void testOfficialSizesAndOptions()
{
    wui::Checkbox checkbox("Accept terms");
    expect(wui::theme().controls.checkboxSize == 16.0f,
           "Fluent medium Checkbox indicator token must be 16 DIP");
    expect(checkbox.measure({}).height == 32.0f,
           "Fluent medium Checkbox must reserve a 32 DIP click target");
    checkbox.setSize(wui::CheckboxSize::Large);
    expect(checkbox.measure({}).height == 36.0f,
           "Fluent large Checkbox must reserve a 36 DIP click target");
    checkbox.setShape(wui::CheckboxShape::Circular);
    checkbox.setLabelPosition(wui::CheckboxLabelPosition::Before);
    checkbox.setRequired(true);
    expect(checkbox.shape() == wui::CheckboxShape::Circular &&
               checkbox.labelPosition() == wui::CheckboxLabelPosition::Before &&
               checkbox.isRequired(),
           "Checkbox must preserve circular, label-before and required options");
}

void testMixedStateAndNativeKeyboardContract()
{
    wui::Checkbox checkbox("Select all");
    checkbox.setCheckState(wui::CheckboxState::Mixed);
    int boolChanges = 0;
    int stateChanges = 0;
    checkbox.onChange([&](bool checked) {
        expect(checked, "Mixed activation must report the native checked transition");
        ++boolChanges;
    });
    checkbox.onStateChange([&](wui::CheckboxState state) {
        expect(state == wui::CheckboxState::Checked,
               "Mixed activation must leave the component in checked state");
        ++stateChanges;
    });
    expect(checkbox.isMixed() && !checkbox.isChecked(),
           "Mixed state must be distinct from checked");
    expect(!checkbox.onKeyEvent({0, wui::KeyAction::Down, 13}),
           "Enter must not activate a native-semantics Checkbox");
    expect(checkbox.isMixed(), "Ignored Enter input must preserve mixed state");
    expect(checkbox.onKeyEvent({0, wui::KeyAction::Down, 32}) &&
               checkbox.state() == wui::CheckboxState::Checked &&
               boolChanges == 1 && stateChanges == 1,
           "Space must activate Checkbox and reuse both change callbacks");
}

void testPointerDisabledAndAccessibility()
{
    wui::Checkbox checkbox("Select all");
    checkbox.layout({0.0f, 0.0f, 160.0f, 32.0f});
    checkbox.setMixed();
    checkbox.setRequired();
    const auto snapshot = wui::snapshotAccessibilityTree(checkbox, &checkbox);
    expect(snapshot.size() == 1 &&
               snapshot.front().properties.role == wui::AccessibilityRole::CheckBox &&
               snapshot.front().properties.checked.has_value() &&
               !*snapshot.front().properties.checked &&
               snapshot.front().properties.mixed &&
               snapshot.front().properties.required,
           "Mixed required Checkbox must expose partially-checked and form-required accessibility states");

    expect(checkbox.onPointerEvent(pointer(wui::PointerAction::Down, {8.0f, 16.0f},
                                                   wui::MouseButton::Left)) &&
               (checkbox.visualStates() & wui::toMask(wui::ControlVisualState::Focused)) != 0 &&
               (checkbox.visualStates() & wui::toMask(wui::ControlVisualState::FocusVisible)) == 0,
           "Pointer activation must focus Checkbox without a keyboard-only outline");
    checkbox.onPointerEvent(pointer(wui::PointerAction::Cancel, {8.0f, 16.0f}));
    checkbox.setEnabled(false);
    expect(!checkbox.onKeyEvent({0, wui::KeyAction::Down, 32}) &&
               !checkbox.onPointerEvent(pointer(wui::PointerAction::Down, {8.0f, 16.0f},
                                                 wui::MouseButton::Left)) &&
               checkbox.performAccessibilityAction(wui::AccessibilityActionKind::Toggle, {}) ==
                   wui::AccessibilityActionStatus::ElementNotEnabled,
           "Disabled Checkbox must reject keyboard, pointer and automation input");
}

void testBindingReplacementUnsubscribesOldState()
{
    wui::State<bool> oldState{false};
    wui::State<bool> newState{false};
    wui::Checkbox checkbox("Bound");
    checkbox.bind(oldState);
    checkbox.bind(newState);
    checkbox.layout({0.0f, 0.0f, 120.0f, 32.0f});
    wui::PaintContext paint;
    checkbox.paint(paint);
    expect(!checkbox.isDirty(wui::DirtyFlag::Paint),
           "Binding fixture must begin from a clean paint state");

    oldState.set(true);
    expect(!checkbox.isChecked() && !checkbox.isDirty(wui::DirtyFlag::Paint),
           "Replacing a Checkbox binding must immediately unsubscribe the old State");
    newState.set(true);
    expect(checkbox.isChecked() && checkbox.isDirty(wui::DirtyFlag::Paint),
           "The replacement Checkbox binding must remain subscribed and invalidate paint");
}

void writeVisualMatrix(const std::string& output, float scale)
{
    const int width = static_cast<int>(std::lround(static_cast<float>(kWidth) * scale));
    const int height = static_cast<int>(std::lround(static_cast<float>(kHeight) * scale));
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, width, height);
    expect(canvas && canvas->initializeContext(),
           "Software canvas must initialize for Checkbox visual review");
    wui::WhatsCanvasTextMeasurer measurer(*canvas, scale);
    wui::setTextMeasurer(&measurer);
    try {
        wui::PaintContext paint(*canvas, scale);
        canvas->beginFrame();
        paint.fillRect({0.0f, 0.0f, static_cast<float>(kWidth), static_cast<float>(kHeight)},
                       wui::theme().colors.neutralBackground2.rest);
        paint.drawText("Fluent Checkbox state matrix", 24.0f, 38.0f, 20.0f,
                       wui::theme().colors.neutralForeground1, 600);

        std::vector<std::unique_ptr<wui::Checkbox>> controls;
        controls.push_back(std::make_unique<wui::Checkbox>("Unchecked"));
        controls.push_back(std::make_unique<wui::Checkbox>("Unchecked hover"));
        controls.back()->setVisualState(wui::ControlVisualState::Hovered, true);
        controls.push_back(std::make_unique<wui::Checkbox>("Unchecked pressed"));
        controls.back()->setVisualState(wui::ControlVisualState::Hovered, true);
        controls.back()->setVisualState(wui::ControlVisualState::Pressed, true);
        controls.push_back(std::make_unique<wui::Checkbox>("Unchecked disabled"));
        controls.back()->setEnabled(false);

        controls.push_back(std::make_unique<wui::Checkbox>("Checked", true));
        controls.push_back(std::make_unique<wui::Checkbox>("Checked hover", true));
        controls.back()->setVisualState(wui::ControlVisualState::Hovered, true);
        controls.push_back(std::make_unique<wui::Checkbox>("Checked pressed", true));
        controls.back()->setVisualState(wui::ControlVisualState::Hovered, true);
        controls.back()->setVisualState(wui::ControlVisualState::Pressed, true);
        controls.push_back(std::make_unique<wui::Checkbox>("Checked disabled", true));
        controls.back()->setEnabled(false);

        for (const char* label : {"Mixed", "Mixed hover", "Mixed pressed",
                                  "Mixed disabled"}) {
            controls.push_back(std::make_unique<wui::Checkbox>(label));
            controls.back()->setMixed();
        }
        controls[9]->setVisualState(wui::ControlVisualState::Hovered, true);
        controls[10]->setVisualState(wui::ControlVisualState::Hovered, true);
        controls[10]->setVisualState(wui::ControlVisualState::Pressed, true);
        controls[11]->setEnabled(false);

        controls.push_back(std::make_unique<wui::Checkbox>("Pointer focus", true));
        controls.back()->setVisualState(wui::ControlVisualState::Focused, true);
        controls.push_back(std::make_unique<wui::Checkbox>("Keyboard focus", true));
        controls.back()->setVisualState(wui::ControlVisualState::Focused, true);
        controls.back()->setVisualState(wui::ControlVisualState::FocusVisible, true);
        controls.push_back(std::make_unique<wui::Checkbox>("Large circular task", true));
        controls.back()->setSize(wui::CheckboxSize::Large);
        controls.back()->setShape(wui::CheckboxShape::Circular);
        controls.push_back(std::make_unique<wui::Checkbox>("Circular mixed"));
        controls.back()->setShape(wui::CheckboxShape::Circular);
        controls.back()->setMixed();
        controls.push_back(std::make_unique<wui::Checkbox>("Required before"));
        controls.back()->setLabelPosition(wui::CheckboxLabelPosition::Before);
        controls.back()->setRequired();
        controls.push_back(std::make_unique<wui::Checkbox>(
            "A wrapping label keeps its indicator aligned with the first line"));
        expect(controls.back()->measure({0.0f, 210.0f}).height >= 40.0f,
               "A narrow Checkbox label must wrap to multiple visual lines");

        for (std::size_t index = 0; index < controls.size(); ++index) {
            const float x = 24.0f + static_cast<float>(index % 4) * 215.0f;
            const float y = 62.0f + static_cast<float>(index / 4) * 64.0f;
            const float measuredHeight = controls[index]->measure({0.0f, 195.0f}).height;
            controls[index]->layout({x, y, 195.0f, measuredHeight});
            controls[index]->prepare(paint);
            controls[index]->paint(paint);
        }
        canvas->endFrame();
        const auto pixels = canvas->readPixelsRGBA();
        expect(pixels.size() == static_cast<std::size_t>(width * height * 4),
               "Checkbox visual capture must return a complete RGBA frame");
        // Keep the rendered state matrix available even when a later pixel
        // contract fails at a fractional DPR.
        savePpm(output, pixels, width, height);
        const auto background = wui::theme().colors.neutralBackground2.rest;
        // Index 14 is the selected circular task checkbox at logical bounds
        // {454, 254, 195, 36}. Its 20-DIP indicator starts at {462,262}; the
        // geometric corner must remain background rather than becoming a
        // clipped square at either 1x or fractional DPR.
        expect(pixelIs(pixels, width, scale, 462.0f, 262.0f, background),
               "Circular Checkbox must leave the indicator corner unfilled");
        // Index 12 has logical pointer focus at {24,254,195,32}. It must not
        // paint the keyboard-only root focus rectangle.
        expect(pixelIs(pixels, width, scale, 22.0f, 270.0f, background),
               "Pointer-focused Checkbox must not paint a persistent black frame");
        // Index 13 has keyboard focus at {239,254,195,32}; the root outline
        // remains available for keyboard navigation and accessibility.
        expect(!pixelIs(pixels, width, scale, 237.0f, 270.0f, background),
               "Keyboard-focused Checkbox must retain its focus-visible outline");
        expect(regionContainsColor(pixels, width, scale, {31.0f, 75.0f, 3.0f, 7.0f},
                                   wui::theme().colors.neutralStrokeAccessible) &&
                   regionContainsColor(pixels, width, scale, {246.0f, 75.0f, 3.0f, 7.0f},
                                       wui::theme().colors.neutralStrokeAccessibleHover),
               "Unchecked Checkbox hover must resolve the official accessible-stroke hover token");
        expect(pixelIs(pixels, width, scale, 34.0f, 136.0f,
                       wui::theme().colors.compoundBrandBackground.rest) &&
                   pixelIs(pixels, width, scale, 249.0f, 136.0f,
                           wui::theme().colors.compoundBrandBackground.hover) &&
                   pixelIs(pixels, width, scale, 464.0f, 136.0f,
                           wui::theme().colors.compoundBrandBackground.pressed),
               "Checked Checkbox surfaces must use compound-brand interaction tokens");
        // Index 17 wraps to multiple lines at {239,318}. The indicator's left
        // stroke must occur beside the first line, rather than
        // drifting to the vertical center of the complete label block.
        expect(regionContainsColor(pixels, width, scale, {246.0f, 320.0f, 4.0f, 9.0f},
                                   wui::theme().colors.neutralStrokeAccessible),
               "Wrapped Checkbox indicator must align with the first label line");
        // The checked control at index 4 must contain visible on-brand pixels
        // on both sides of the Fluent checkmark. Keep the samples broad enough
        // for the font's distinct 1x and fractional-DPI hinted outlines.
        expect(regionContainsColorNear(
                   pixels, width, scale, {33.0f, 136.0f, 7.0f, 11.0f},
                   wui::theme().colors.onBrand, 128) &&
                   regionContainsColorNear(
                       pixels, width, scale, {39.0f, 134.0f, 8.0f, 14.0f},
                       wui::theme().colors.onBrand, 128),
               "Checked Checkbox must paint a recognizable two-arm checkmark");
        // Ignore the rounded face corners and compare the visible white ink
        // against the physical-pixel centre of the complete 16-DIP indicator.
        // This catches baseline-centred icon fonts whose actual checkmark is
        // one DIP too high (1.5 px at the common Windows 150% scale).
        const auto checkmarkInk = verticalBoundsNearColor(
            pixels, width, scale, {34.5f, 136.5f, 11.0f, 11.0f},
            wui::theme().colors.onBrand, 72);
        // Checkbox snaps the indicator's two outer edges before drawing.
        // Compare against that rendered pixel box (not the unsnapped logical
        // interval, whose centre differs by half a pixel at 125% DPR).
        const int indicatorTop =
            static_cast<int>(std::lround(134.0f * scale));
        const int indicatorBottom =
            static_cast<int>(std::lround(150.0f * scale)) - 1;
        const float indicatorCenter =
            (static_cast<float>(indicatorTop) +
             static_cast<float>(indicatorBottom)) *
            0.5f;
        expect(checkmarkInk.valid() &&
                   std::abs(checkmarkInk.center() - indicatorCenter) <= 0.75f,
               "Checked Checkbox mark ink must be vertically centred within the indicator");
        // Mixed is an outlined compound-brand state with a same-color inner
        // mark. It must not regress to the checked state's blue face and
        // inverse white mark.
        expect(pixelIs(pixels, width, scale, 40.0f, 206.0f,
                       wui::theme().colors.compoundBrandForeground1.rest) &&
                   !pixelIs(pixels, width, scale, 34.0f, 200.0f,
                            wui::theme().colors.compoundBrandBackground.rest),
               "Square mixed Checkbox must use a transparent face and compound-brand mark");
        // Index 15 is circular mixed at {669,254}; its centre mark is brand
        // while the bounding-square corner remains transparent.
        expect(pixelIs(pixels, width, scale, 685.0f, 270.0f,
                       wui::theme().colors.compoundBrandForeground1.rest) &&
                   pixelIs(pixels, width, scale, 677.0f, 262.0f,
                           background),
               "Circular mixed Checkbox must retain its circular outer silhouette");
    } catch (...) {
        wui::setTextMeasurer(nullptr);
        throw;
    }
    wui::setTextMeasurer(nullptr);
}

} // namespace

int main(int argc, char** argv)
{
    try {
        testOfficialSizesAndOptions();
        testMixedStateAndNativeKeyboardContract();
        testPointerDisabledAndAccessibility();
        testBindingReplacementUnsubscribesOldState();
        const float scale = argc > 2 ? std::max(1.0f, std::stof(argv[2])) : 1.0f;
        writeVisualMatrix(argc > 1 ? argv[1] : "fluent_checkbox_review.ppm", scale);
        std::cout << "WhatsUI Fluent Checkbox completion tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WhatsUI Fluent Checkbox completion tests failed: " << error.what() << '\n';
        return 1;
    }
}
