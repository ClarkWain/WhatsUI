#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "wsc/Canvas.h"
#include "wui/basic_controls.h"
#include "wui/icons.h"
#include "wui/overlays.h"
#include "wui/paint_context.h"
#include "wui/runtime.h"
#include "wui/text_input.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"
#include "wui/widgets.h"

namespace {

constexpr int kLogicalWidth = 1040;
constexpr int kLogicalHeight = 670;
constexpr std::array<float, 6> kColumns{
    28.0f, 194.0f, 360.0f, 526.0f, 692.0f, 858.0f};

enum class MatrixState : std::size_t {
    Rest = 0,
    Hover,
    Pressed,
    Focus,
    Selected,
    Disabled,
};

struct PixelBounds {
    int left{0};
    int top{0};
    int right{-1};
    int bottom{-1};

    [[nodiscard]] bool valid() const noexcept
    {
        return right >= left && bottom >= top;
    }

    [[nodiscard]] int width() const noexcept
    {
        return valid() ? right - left + 1 : 0;
    }

    [[nodiscard]] int height() const noexcept
    {
        return valid() ? bottom - top + 1 : 0;
    }

    [[nodiscard]] float centerX() const noexcept
    {
        return 0.5f * static_cast<float>(left + right);
    }

    [[nodiscard]] float centerY() const noexcept
    {
        return 0.5f * static_cast<float>(top + bottom);
    }
};

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool channelNear(std::uint8_t actual, std::uint8_t expected,
                 int tolerance) noexcept
{
    return std::abs(static_cast<int>(actual) -
                    static_cast<int>(expected)) <= tolerance;
}

bool colorNear(const std::vector<unsigned char>& pixels, int width, int height,
               int x, int y, wui::Color color, int tolerance = 3)
{
    if (x < 0 || y < 0 || x >= width || y >= height) return false;
    const auto offset =
        static_cast<std::size_t>((y * width + x) * 4);
    return offset + 3 < pixels.size()
        && channelNear(pixels[offset], color.r, tolerance)
        && channelNear(pixels[offset + 1], color.g, tolerance)
        && channelNear(pixels[offset + 2], color.b, tolerance)
        && channelNear(pixels[offset + 3], color.a, tolerance);
}

bool differentFrom(const std::vector<unsigned char>& pixels,
                   int width, int height, int x, int y,
                   wui::Color color, int tolerance = 8)
{
    return !colorNear(pixels, width, height, x, y, color, tolerance);
}

wui::Color compositeOver(wui::Color foreground,
                         wui::Color background) noexcept
{
    const int alpha = foreground.a;
    const auto blend = [alpha](std::uint8_t front, std::uint8_t back) {
        return static_cast<std::uint8_t>(
            (static_cast<int>(front) * alpha +
             static_cast<int>(back) * (255 - alpha) + 127) /
            255);
    };
    return {blend(foreground.r, background.r),
            blend(foreground.g, background.g),
            blend(foreground.b, background.b), 255};
}

int physical(float logical, float scale)
{
    return static_cast<int>(std::lround(logical * scale));
}

PixelBounds findColor(const std::vector<unsigned char>& pixels,
                      int width, int height, float scale,
                      wui::RectF region, wui::Color color,
                      int tolerance = 3)
{
    const int left = std::clamp(
        static_cast<int>(std::floor(region.x * scale)), 0, width);
    const int top = std::clamp(
        static_cast<int>(std::floor(region.y * scale)), 0, height);
    const int right = std::clamp(
        static_cast<int>(std::ceil(
            (region.x + region.width) * scale)), 0, width);
    const int bottom = std::clamp(
        static_cast<int>(std::ceil(
            (region.y + region.height) * scale)), 0, height);
    PixelBounds result{right, bottom, left - 1, top - 1};
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            if (!colorNear(pixels, width, height, x, y,
                           color, tolerance)) {
                continue;
            }
            result.left = std::min(result.left, x);
            result.top = std::min(result.top, y);
            result.right = std::max(result.right, x);
            result.bottom = std::max(result.bottom, y);
        }
    }
    return result;
}

PixelBounds findInkOutsideHorizontalBand(
    const std::vector<unsigned char>& pixels, int width, int height,
    float scale, wui::RectF region, float logicalCentreY,
    float halfBand, wui::Color background, int tolerance = 10)
{
    const int left = std::clamp(
        static_cast<int>(std::floor(region.x * scale)), 0, width);
    const int top = std::clamp(
        static_cast<int>(std::floor(region.y * scale)), 0, height);
    const int right = std::clamp(
        static_cast<int>(std::ceil(
            (region.x + region.width) * scale)), 0, width);
    const int bottom = std::clamp(
        static_cast<int>(std::ceil(
            (region.y + region.height) * scale)), 0, height);
    PixelBounds result{right, bottom, left - 1, top - 1};
    for (int y = top; y < bottom; ++y) {
        const float logicalPixelCentre =
            (static_cast<float>(y) + 0.5f) / scale;
        if (std::fabs(logicalPixelCentre - logicalCentreY) <= halfBand) {
            continue;
        }
        for (int x = left; x < right; ++x) {
            if (!differentFrom(pixels, width, height, x, y,
                               background, tolerance)) {
                continue;
            }
            result.left = std::min(result.left, x);
            result.top = std::min(result.top, y);
            result.right = std::max(result.right, x);
            result.bottom = std::max(result.bottom, y);
        }
    }
    return result;
}

int verticalColorRun(const std::vector<unsigned char>& pixels,
                     int width, int height, int x, int firstY, int lastY,
                     wui::Color color, int tolerance = 3)
{
    firstY = std::clamp(firstY, 0, height - 1);
    lastY = std::clamp(lastY, 0, height - 1);
    int longest = 0;
    int current = 0;
    for (int y = firstY; y <= lastY; ++y) {
        if (colorNear(pixels, width, height, x, y, color, tolerance)) {
            longest = std::max(longest, ++current);
        } else {
            current = 0;
        }
    }
    return longest;
}

int horizontalColorRun(const std::vector<unsigned char>& pixels,
                       int width, int height, int y, int firstX, int lastX,
                       wui::Color color, int tolerance = 3)
{
    firstX = std::clamp(firstX, 0, width - 1);
    lastX = std::clamp(lastX, 0, width - 1);
    int longest = 0;
    int current = 0;
    for (int x = firstX; x <= lastX; ++x) {
        if (colorNear(pixels, width, height, x, y, color, tolerance)) {
            longest = std::max(longest, ++current);
        } else {
            current = 0;
        }
    }
    return longest;
}

int mirroredColorMismatch(const std::vector<unsigned char>& pixels,
                          int width, int height, float scale,
                          wui::RectF region, wui::Color color,
                          int tolerance = 4)
{
    const int left = physical(region.x, scale);
    const int top = physical(region.y, scale);
    const int right = physical(region.x + region.width, scale) - 1;
    const int bottom = physical(region.y + region.height, scale) - 1;
    int mismatches = 0;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            const bool value = colorNear(
                pixels, width, height, x, y, color, tolerance);
            const bool mirrorX = colorNear(
                pixels, width, height, left + right - x, y,
                color, tolerance);
            const bool mirrorY = colorNear(
                pixels, width, height, x, top + bottom - y,
                color, tolerance);
            mismatches += value != mirrorX ? 1 : 0;
            mismatches += value != mirrorY ? 1 : 0;
        }
    }
    return mismatches;
}

void writePpm(const std::string& path,
              const std::vector<unsigned char>& rgba,
              int width, int height)
{
    std::ofstream output(path, std::ios::binary);
    expect(output.good(),
           "OpenGL Fluent control matrix output could not be opened");
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto offset =
                static_cast<std::size_t>((y * width + x) * 4);
            output.put(static_cast<char>(rgba[offset]));
            output.put(static_cast<char>(rgba[offset + 1]));
            output.put(static_cast<char>(rgba[offset + 2]));
        }
    }
}

template <typename NodeType>
void paintNode(NodeType& node, const wui::RectF& bounds,
               wui::PaintContext& paint)
{
    node.layout(bounds);
    node.prepare(paint);
    node.paint(paint);
}

void applyState(wui::ControlNode& control, MatrixState state)
{
    switch (state) {
    case MatrixState::Hover:
        control.setVisualState(wui::ControlVisualState::Hovered, true);
        break;
    case MatrixState::Pressed:
        control.setVisualState(wui::ControlVisualState::Pressed, true);
        break;
    case MatrixState::Focus:
        control.setVisualState(wui::ControlVisualState::Focused, true);
        control.setVisualState(wui::ControlVisualState::FocusVisible, true);
        break;
    case MatrixState::Disabled:
        control.setEnabled(false);
        break;
    case MatrixState::Rest:
    case MatrixState::Selected:
        break;
    }
}

void pointAtSplitRegion(wui::SplitButton& split, wui::RectF bounds,
                        bool disclosure, bool pressed)
{
    split.layout(bounds);
    const wui::PointF position{
        disclosure ? bounds.x + bounds.width - 8.0f : bounds.x + 8.0f,
        bounds.y + bounds.height * 0.5f};
    expect(split.onPointerEvent(
               {0, wui::PointerType::Mouse, wui::PointerAction::Enter,
                wui::MouseButton::None, position}),
           "SplitButton test setup must enter the requested region");
    if (pressed) {
        expect(split.onPointerEvent(
                   {0, wui::PointerType::Mouse, wui::PointerAction::Down,
                    wui::MouseButton::Left, position}),
               "SplitButton test setup must press the requested region");
    }
}

void verifyRoundColor(const std::vector<unsigned char>& pixels,
                      int width, int height, float scale,
                      wui::RectF region, wui::Color color,
                      const char* missingMessage,
                      const char* diameterMessage,
                      const char* symmetryMessage)
{
    const auto ink =
        findColor(pixels, width, height, scale, region, color, 4);
    expect(ink.valid(), missingMessage);
    expect(std::abs(ink.width() - ink.height()) <= 1,
           diameterMessage);
    const int diameter = std::max(ink.width(), ink.height());
    expect(mirroredColorMismatch(pixels, width, height, scale, region,
                                 color, 4) <=
               std::max(12, diameter * 4),
           symmetryMessage);
}

int run(const std::string& outputPath, float scale)
{
    scale = std::max(1.0f, scale);
    const int width = physical(kLogicalWidth, scale);
    const int height = physical(kLogicalHeight, scale);

    expect(glfwInit() == GLFW_TRUE,
           "SKIP: native GLFW/OpenGL surface unavailable: glfwInit failed");
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(
        width, height, "WhatsUI Fluent control matrix", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        throw std::runtime_error(
            "SKIP: native GLFW/OpenGL surface unavailable: window creation failed");
    }
    glfwMakeContextCurrent(window);

    expect(wsc::Canvas::loadOpenGL(
               reinterpret_cast<wsc::Canvas::OpenGLProcAddress>(
                   glfwGetProcAddress)),
           "OpenGL entry points could not be loaded");
    auto canvas =
        wsc::Canvas::create(wsc::Canvas::Backend::OpenGL, width, height);
    expect(canvas && canvas->initializeContext(),
           "OpenGL Canvas could not be initialized");
    canvas->setSize(width, height);
    canvas->setDevicePixelRatio(scale);
    expect(canvas->setOutputTarget(
               wsc::OutputTarget::GLFramebuffer(0, width, height, true)),
           "Hidden-window OpenGL framebuffer could not be bound");
    const auto iconStatus = wui::registerDefaultIconFonts(*canvas);
    expect(iconStatus.complete(),
           "Bundled Fluent icon fonts must be available in OpenGL");

    wui::WhatsCanvasTextMeasurer measurer(*canvas, 1.0f);
    wui::setTextMeasurer(&measurer);
    const auto& current = wui::theme();
    const auto& colors = current.colors;
    wui::PaintContext paint(*canvas, scale, true);

    std::array<wui::Button, 4> actionButtons{
        wui::Button("Rest"), wui::Button("Hover"),
        wui::Button("Pressed"), wui::Button("Focus")};
    for (std::size_t index = 0; index < actionButtons.size(); ++index) {
        actionButtons[index].setAppearance(wui::ButtonAppearance::Primary);
        applyState(actionButtons[index],
                   static_cast<MatrixState>(index));
    }
    wui::ToggleButton selectedAction("Selected", true);
    wui::Button disabledAction("Disabled");
    disabledAction.setAppearance(wui::ButtonAppearance::Primary);
    applyState(disabledAction, MatrixState::Disabled);

    std::array<wui::IconButton, 6> iconButtons{
        wui::IconButton(wui::IconName::Star, "Rest"),
        wui::IconButton(wui::IconName::Star, "Hover"),
        wui::IconButton(wui::IconName::Delete, "Pressed"),
        wui::IconButton(wui::IconName::Star, "Focus"),
        wui::IconButton(wui::IconName::Star, "Selected"),
        wui::IconButton(wui::IconName::Delete, "Disabled")};
    for (std::size_t index = 0; index < iconButtons.size(); ++index) {
        applyState(iconButtons[index], static_cast<MatrixState>(index));
    }
    iconButtons[static_cast<std::size_t>(MatrixState::Selected)]
        .setChecked(true);

    std::array<wui::TextInput, 6> inputs{
        wui::TextInput("Rest"), wui::TextInput("Hover"),
        wui::TextInput("Pressed"), wui::TextInput("Focus"),
        wui::TextInput("Selected"), wui::TextInput("Disabled")};
    for (std::size_t index = 0; index < inputs.size(); ++index) {
        inputs[index].setMotionEnabled(false);
        applyState(inputs[index], static_cast<MatrixState>(index));
    }
    inputs[static_cast<std::size_t>(MatrixState::Selected)]
        .controller().setText("Selected");
    inputs[static_cast<std::size_t>(MatrixState::Selected)]
        .controller().selectAll();
    inputs[static_cast<std::size_t>(MatrixState::Selected)]
        .setVisualState(wui::ControlVisualState::Focused, true);

    std::array<wui::TextArea, 6> textAreas{
        wui::TextArea("Rest"), wui::TextArea("Hover"),
        wui::TextArea("Pressed"), wui::TextArea("Focus"),
        wui::TextArea("Selected"), wui::TextArea("Disabled")};
    for (std::size_t index = 0; index < textAreas.size(); ++index) {
        textAreas[index].setMotionEnabled(false);
        applyState(textAreas[index], static_cast<MatrixState>(index));
    }
    textAreas[static_cast<std::size_t>(MatrixState::Selected)]
        .controller().setText("Selected\ntext");
    textAreas[static_cast<std::size_t>(MatrixState::Selected)]
        .controller().selectAll();
    textAreas[static_cast<std::size_t>(MatrixState::Selected)]
        .setVisualState(wui::ControlVisualState::Focused, true);

    std::array<wui::Checkbox, 6> checkboxes{
        wui::Checkbox("", false), wui::Checkbox("", false),
        wui::Checkbox("", false), wui::Checkbox("", false),
        wui::Checkbox("", true), wui::Checkbox("", true)};
    for (std::size_t index = 0; index < checkboxes.size(); ++index) {
        applyState(checkboxes[index], static_cast<MatrixState>(index));
    }

    std::array<wui::Radio, 6> radios{
        wui::Radio("", false), wui::Radio("", false),
        wui::Radio("", false), wui::Radio("", false),
        wui::Radio("", true), wui::Radio("", true)};
    for (std::size_t index = 0; index < radios.size(); ++index) {
        applyState(radios[index], static_cast<MatrixState>(index));
    }

    std::array<wui::Switch, 6> switches{
        wui::Switch("", false), wui::Switch("", false),
        wui::Switch("", false), wui::Switch("", false),
        wui::Switch("", true), wui::Switch("", true)};
    for (std::size_t index = 0; index < switches.size(); ++index) {
        applyState(switches[index], static_cast<MatrixState>(index));
    }

    std::array<wui::Slider, 6> sliders{
        wui::Slider(0, 100, 35), wui::Slider(0, 100, 35),
        wui::Slider(0, 100, 35), wui::Slider(0, 100, 35),
        wui::Slider(0, 100, 70), wui::Slider(0, 100, 70)};
    for (std::size_t index = 0; index < sliders.size(); ++index) {
        applyState(sliders[index], static_cast<MatrixState>(index));
    }

    std::array<wui::CompoundButton, 6> compoundButtons{
        wui::CompoundButton("Action", "Rest"),
        wui::CompoundButton("Action", "Hover"),
        wui::CompoundButton("Action", "Pressed"),
        wui::CompoundButton("Action", "Focus"),
        wui::CompoundButton("Action", "Selected"),
        wui::CompoundButton("Action", "Disabled")};
    for (std::size_t index = 0; index < compoundButtons.size(); ++index) {
        compoundButtons[index].setAppearance(
            wui::ButtonAppearance::Primary);
        applyState(compoundButtons[index],
                   static_cast<MatrixState>(index));
    }

    std::array<wui::MenuButton, 6> menuButtons{
        wui::MenuButton("Menu"), wui::MenuButton("Menu"),
        wui::MenuButton("Menu"), wui::MenuButton("Menu"),
        wui::MenuButton("Menu"), wui::MenuButton("Menu")};
    for (std::size_t index = 0; index < menuButtons.size(); ++index) {
        applyState(menuButtons[index], static_cast<MatrixState>(index));
    }
    wui::OverlayHost openMenuHost;
    menuButtons[static_cast<std::size_t>(MatrixState::Selected)]
        .bindOverlayHost(openMenuHost)
        .addItem({"Open item", {}, true, {}});
    menuButtons[static_cast<std::size_t>(MatrixState::Selected)]
        .layout({kColumns[4], 478, 132, 32});
    expect(menuButtons[static_cast<std::size_t>(MatrixState::Selected)]
               .performAccessibilityAction(
                   wui::AccessibilityActionKind::Expand, {}) ==
               wui::AccessibilityActionStatus::Succeeded,
           "MenuButton matrix setup must enter its retained open state");

    std::array<wui::SplitButton, 6> splitButtons{
        wui::SplitButton("Split"), wui::SplitButton("Split"),
        wui::SplitButton("Split"), wui::SplitButton("Split"),
        wui::SplitButton("Split"), wui::SplitButton("Split")};
    constexpr float kSplitY = 548.0f;
    for (std::size_t index = 0; index < splitButtons.size(); ++index) {
        splitButtons[index].layout(
            {kColumns[index], kSplitY, 132, 32});
    }
    pointAtSplitRegion(
        splitButtons[1], {kColumns[1], kSplitY, 132, 32}, false, false);
    pointAtSplitRegion(
        splitButtons[2], {kColumns[2], kSplitY, 132, 32}, false, true);
    pointAtSplitRegion(
        splitButtons[3], {kColumns[3], kSplitY, 132, 32}, true, false);
    pointAtSplitRegion(
        splitButtons[4], {kColumns[4], kSplitY, 132, 32}, true, true);
    splitButtons[5].setEnabled(false);

    wui::OverlayHost openSplitHost;
    wui::SplitButton openSplitButton("Split open");
    openSplitButton
        .bindOverlayHost(openSplitHost)
        .addItem({"Open item", {}, true, {}});
    constexpr float kOpenSplitY = 616.0f;
    openSplitButton.layout({kColumns[0], kOpenSplitY, 132, 32});
    expect(openSplitButton.performAccessibilityAction(
               wui::AccessibilityActionKind::Expand, {}) ==
               wui::AccessibilityActionStatus::Succeeded,
           "SplitButton matrix setup must enter its retained open state");

    canvas->beginFrame();
    paint.fillRect({0, 0, static_cast<float>(kLogicalWidth),
                    static_cast<float>(kLogicalHeight)},
                   colors.neutralBackground2.rest);
    const std::array<const char*, 6> headings{
        "REST", "HOVER", "PRESSED", "FOCUS",
        "SELECTED", "DISABLED"};
    for (std::size_t index = 0; index < headings.size(); ++index) {
        paint.drawText(headings[index], kColumns[index], 25.0f, 11.0f,
                       colors.neutralForeground3, 600);
    }
    const std::array<const char*, 6> splitHeadings{
        "REST", "PRIMARY HOVER", "PRIMARY DOWN",
        "DISCLOSURE HOVER", "DISCLOSURE DOWN", "DISABLED"};
    for (std::size_t index = 0; index < splitHeadings.size(); ++index) {
        paint.drawText(splitHeadings[index], kColumns[index], 537.0f,
                       9.0f, colors.neutralForeground3, 600);
    }
    paint.drawText("OPEN DISCLOSURE", kColumns[0], 605.0f, 9.0f,
                   colors.neutralForeground3, 600);

    for (std::size_t index = 0; index < kColumns.size(); ++index) {
        if (index < actionButtons.size()) {
            paintNode(actionButtons[index],
                      {kColumns[index], 42, 132, 32}, paint);
        } else if (index == static_cast<std::size_t>(
                                MatrixState::Selected)) {
            paintNode(selectedAction,
                      {kColumns[index], 42, 132, 32}, paint);
        } else {
            paintNode(disabledAction,
                      {kColumns[index], 42, 132, 32}, paint);
        }
        paintNode(iconButtons[index],
                  {kColumns[index] + 50, 82, 32, 32}, paint);
        paintNode(inputs[index],
                  {kColumns[index], 124, 132, 32}, paint);
        paintNode(textAreas[index],
                  {kColumns[index], 166, 132, 58}, paint);
        paintNode(checkboxes[index],
                  {kColumns[index] + 50, 238, 32, 32}, paint);
        // Use the real 32-DIP interactive target. The 16-DIP indicator stays
        // left-aligned inside it, while keyboard focus gets a balanced square
        // face instead of an artificial 16x32 diagnostic sliver.
        paintNode(radios[index],
                  {kColumns[index] + 50, 282, 32, 32}, paint);
        // Match the Fluent Switch root exactly.  The 40x20 track lives at an
        // 8-DIP inset inside this 56x36 focus/hit target; constraining the
        // root to 40x32 makes the track protrude through the focus outline.
        paintNode(switches[index],
                  {kColumns[index] + 38, 324, 56, 36}, paint);
        paintNode(sliders[index],
                  {kColumns[index], 370, 132, 32}, paint);
        paintNode(compoundButtons[index],
                  {kColumns[index], 414, 132, 52}, paint);
        paintNode(menuButtons[index],
                  {kColumns[index], 478, 132, 32}, paint);
        paintNode(splitButtons[index],
                  {kColumns[index], kSplitY, 132, 32}, paint);
    }
    paintNode(openSplitButton,
              {kColumns[0], kOpenSplitY, 132, 32}, paint);
    canvas->endFrame();

    const auto pixels = canvas->readPixelsRGBA();
    expect(pixels.size()
               == static_cast<std::size_t>(width * height * 4),
           "OpenGL Fluent control matrix readback must contain a complete frame");
    // Preserve the failed frame as a diagnostic artifact as well as the
    // passing golden candidate. Structural assertions below deliberately run
    // after the write so CI failures remain visually inspectable.
    writePpm(outputPath, pixels, width, height);

    // Action family: resolve the full primary state ramp and retain a real
    // selected surface for the toggle action.
    expect(colorNear(pixels, width, height,
                     physical(kColumns[0] + 6, scale),
                     physical(58, scale),
                     colors.brandBackground.rest, 3),
           "Primary Button rest state must use brandBackground");
    expect(colorNear(pixels, width, height,
                     physical(kColumns[1] + 6, scale),
                     physical(58, scale),
                     colors.brandBackground.hover, 3),
           "Primary Button hover state must use brandBackgroundHover");
    expect(colorNear(pixels, width, height,
                     physical(kColumns[2] + 6, scale),
                     physical(58, scale),
                     colors.brandBackground.pressed, 3),
           "Primary Button pressed state must use brandBackgroundPressed");
    expect(findColor(pixels, width, height, scale,
                     {kColumns[4], 42, 132, 32},
                     colors.neutralBackground1.selected, 3).valid(),
           "ToggleButton selected state must expose the selected surface");
    expect(findColor(pixels, width, height, scale,
                     {kColumns[5], 42, 132, 32},
                     colors.neutralBackgroundDisabled, 3).valid(),
           "Disabled Button must expose the disabled surface");

    // CompoundButton shares the primary state ramp with Button while keeping
    // both text lines inside one 52-DIP command surface.
    expect(colorNear(pixels, width, height,
                     physical(kColumns[1] + 6, scale),
                     physical(440, scale),
                     colors.brandBackground.hover, 3),
           "Primary CompoundButton hover must use brandBackgroundHover");
    expect(findColor(pixels, width, height, scale,
                     {kColumns[5], 414, 132, 52},
                     colors.neutralBackgroundDisabled, 3).valid(),
           "Disabled CompoundButton must expose the disabled surface");

    // MenuButton's disclosure is a Fluent icon in the same centred content
    // group as its label. Measure the real label width so this remains a
    // geometry assertion rather than an assumed trailing-edge coordinate.
    auto menuTextStyle = current.typography.body1Strong;
    if (!current.typography.familyControls.empty()) {
        menuTextStyle.family = current.typography.familyControls;
    }
    const float menuTextWidth =
        measurer.measureText("Menu", menuTextStyle.size,
                             menuTextStyle.weight,
                             menuTextStyle.family).width;
    constexpr float kMenuGap = 6.0f;
    constexpr float kMenuIconSize = 20.0f;
    const float menuContentWidth =
        menuTextWidth + kMenuGap + kMenuIconSize;
    const float menuIconX =
        kColumns[0] + (132.0f - menuContentWidth) * 0.5f +
        menuTextWidth + kMenuGap;
    const wui::RectF menuIconSlot{
        menuIconX, 484.0f, kMenuIconSize, kMenuIconSize};
    const auto menuChevron = findColor(
        pixels, width, height, scale, menuIconSlot,
        colors.neutralForeground1, 70);
    expect(menuChevron.valid(),
           "MenuButton must paint a visible trailing disclosure chevron");
    const float menuIconCentreX =
        0.5f * static_cast<float>(
            physical(menuIconSlot.x, scale) +
            physical(menuIconSlot.x + menuIconSlot.width, scale) - 1);
    const float menuIconCentreY =
        0.5f * static_cast<float>(
            physical(menuIconSlot.y, scale) +
            physical(menuIconSlot.y + menuIconSlot.height, scale) - 1);
    expect(std::fabs(menuChevron.centerX() - menuIconCentreX) <= 1.0f &&
               std::fabs(menuChevron.centerY() - menuIconCentreY) <=
                   std::ceil(scale),
           "MenuButton trailing chevron must stay centred in its 20-DIP slot");

    // SplitButton is two commands sharing one outer surface. Hover/press/open
    // may affect only the operated region; the sibling region must retain its
    // rest color.
    constexpr float kSplitPrimarySampleX = 10.0f;
    // Sample the disclosure plate clear of both the separator and chevron.
    constexpr float kSplitDisclosureSampleX = 104.0f;
    constexpr float kSplitSampleY = kSplitY + 16.0f;
    const auto expectSplitSamples =
        [&](std::size_t index, wui::Color primary,
            wui::Color disclosure, const char* primaryMessage,
            const char* disclosureMessage) {
            expect(colorNear(
                       pixels, width, height,
                       physical(kColumns[index] +
                                    kSplitPrimarySampleX,
                                scale),
                       physical(kSplitSampleY, scale),
                       primary, 3),
                   primaryMessage);
            expect(colorNear(
                       pixels, width, height,
                       physical(kColumns[index] +
                                    kSplitDisclosureSampleX,
                                scale),
                       physical(kSplitSampleY, scale),
                       disclosure, 3),
                   disclosureMessage);
        };
    expectSplitSamples(
        1, colors.brandBackground.hover, colors.brandBackground.rest,
        "SplitButton primary hover must affect only the primary region",
        "SplitButton primary hover must preserve the disclosure rest surface");
    expectSplitSamples(
        2, colors.brandBackground.pressed, colors.brandBackground.rest,
        "SplitButton primary press must affect only the primary region",
        "SplitButton primary press must preserve the disclosure rest surface");
    expectSplitSamples(
        3, colors.brandBackground.rest, colors.brandBackground.hover,
        "SplitButton disclosure hover must preserve the primary rest surface",
        "SplitButton disclosure hover must affect only the disclosure region");
    expectSplitSamples(
        4, colors.brandBackground.rest, colors.brandBackground.pressed,
        "SplitButton disclosure press must preserve the primary rest surface",
        "SplitButton disclosure press must affect only the disclosure region");
    expectSplitSamples(
        5, colors.neutralBackgroundDisabled,
        colors.neutralBackgroundDisabled,
        "Disabled SplitButton must replace the primary brand surface",
        "Disabled SplitButton must replace the disclosure brand surface");
    expect(colorNear(
               pixels, width, height,
               physical(kColumns[0] + kSplitPrimarySampleX, scale),
               physical(kOpenSplitY + 16.0f, scale),
               colors.brandBackground.rest, 3),
           "Open SplitButton must preserve the primary rest surface");
    expect(colorNear(
               pixels, width, height,
               physical(kColumns[0] + kSplitDisclosureSampleX, scale),
               physical(kOpenSplitY + 16.0f, scale),
               colors.brandBackground.pressed, 3),
           "Open SplitButton must retain the disclosure pressed surface");
    const int expectedSplitSeparator =
        std::max(1, physical(current.stroke.thin, scale));
    for (std::size_t index = 0; index + 1 < splitButtons.size(); ++index) {
        const int dividerX = physical(kColumns[index] + 100.0f, scale);
        expect(horizontalColorRun(
                   pixels, width, height,
                   physical(kSplitY + 6.0f, scale),
                   dividerX - expectedSplitSeparator - 2,
                   dividerX + expectedSplitSeparator + 2,
                   colors.onBrand, 3) == expectedSplitSeparator,
               "SplitButton separator must be exactly one snapped DIP");
    }
    {
        const int dividerX = physical(kColumns[5] + 100.0f, scale);
        expect(horizontalColorRun(
                   pixels, width, height,
                   physical(kSplitY + 6.0f, scale),
                   dividerX - expectedSplitSeparator - 2,
                   dividerX + expectedSplitSeparator + 2,
                   colors.neutralStrokeDisabled, 3) ==
                   expectedSplitSeparator,
               "Disabled SplitButton separator must be exactly one snapped DIP");
    }
    {
        const int dividerX = physical(kColumns[0] + 100.0f, scale);
        expect(horizontalColorRun(
                   pixels, width, height,
                   physical(kOpenSplitY + 6.0f, scale),
                   dividerX - expectedSplitSeparator - 2,
                   dividerX + expectedSplitSeparator + 2,
                   colors.onBrand, 3) == expectedSplitSeparator,
               "Open SplitButton separator must be exactly one snapped DIP");
    }

    // The visible glyph, rather than the font's em box, must remain centred
    // in every native OpenGL icon button.
    for (std::size_t index = 0; index < iconButtons.size(); ++index) {
        const wui::RectF box{kColumns[index] + 50, 82, 32, 32};
        const auto ink = findColor(
            pixels, width, height, scale,
            {box.x + 4, box.y + 4, 24, 24},
            index == static_cast<std::size_t>(MatrixState::Disabled)
                ? colors.neutralForegroundDisabled
                : colors.neutralForeground1,
            index == static_cast<std::size_t>(MatrixState::Disabled)
                ? 40
                : 70);
        expect(ink.valid(),
               "IconButton must paint visible Fluent icon ink");
        const float centreX =
            0.5f * static_cast<float>(
                physical(box.x, scale) +
                physical(box.x + box.width, scale) - 1);
        const float centreY =
            0.5f * static_cast<float>(
                physical(box.y, scale) +
                physical(box.y + box.height, scale) - 1);
        expect(std::fabs(ink.centerX() - centreX) <= 1.0f &&
                   std::fabs(ink.centerY() - centreY) <= 1.0f,
               "IconButton visible ink must stay within one physical pixel of the face centre");
    }

    // Input family: the neutral 1-DIP baseline and focused 2-DIP brand
    // indicator must snap to integral physical pixels at every supported DPR.
    const int restInputX = physical(kColumns[0] + 66, scale);
    const int focusInputX = physical(kColumns[3] + 66, scale);
    const int inputTop = physical(124, scale);
    const int inputBottom = physical(156, scale) - 1;
    const int expectedThin = std::max(1, physical(1.0f, scale));
    const int expectedThick = std::max(1, physical(2.0f, scale));
    expect(verticalColorRun(pixels, width, height, restInputX,
                            inputTop, inputBottom,
                            colors.neutralStrokeAccessible, 4)
               == expectedThin,
           "Input neutral bottom border must be exactly one snapped DIP");
    expect(verticalColorRun(pixels, width, height, focusInputX,
                            inputTop, inputBottom,
                            colors.compoundBrandStroke.rest, 4)
               == expectedThick,
           "Focused Input brand indicator must be exactly two snapped DIPs");
    expect(findColor(pixels, width, height, scale,
                     {kColumns[5], 124, 132, 32},
                     colors.neutralStrokeDisabled, 4).valid(),
           "Disabled Input must use the disabled stroke");
    const auto selectionFill = compositeOver(
        wui::Color{colors.brandForeground1.r,
                   colors.brandForeground1.g,
                   colors.brandForeground1.b, 72},
        colors.neutralBackground1.rest);
    expect(findColor(pixels, width, height, scale,
                     {kColumns[4], 124, 132, 32},
                     selectionFill, 8).valid(),
           "Selected Input text must retain a visible selection fill");
    expect(verticalColorRun(
               pixels, width, height,
               physical(kColumns[0] + 66, scale),
               physical(166, scale), physical(224, scale) - 1,
               colors.neutralStrokeAccessible, 4) == expectedThin,
           "TextArea neutral bottom border must be exactly one snapped DIP");
    expect(verticalColorRun(
               pixels, width, height,
               physical(kColumns[3] + 66, scale),
               physical(166, scale), physical(224, scale) - 1,
               colors.compoundBrandStroke.rest, 4) == expectedThick,
           "Focused TextArea brand indicator must be exactly two snapped DIPs");

    // Checkbox mark ink is tested independently of its 32-DIP hit target.
    // It must remain optically centred in the 16-DIP indicator.
    const wui::RectF checkedIndicator{
        kColumns[4] + 58, 246, 16, 16};
    const auto checkInk = findColor(
        pixels, width, height, scale, checkedIndicator,
        colors.onBrand, 60);
    expect(checkInk.valid(),
           "Checked Checkbox must paint visible checkmark ink");
    const float checkboxCentreX =
        0.5f * static_cast<float>(
            physical(checkedIndicator.x, scale) +
            physical(checkedIndicator.x + checkedIndicator.width, scale) - 1);
    const float checkboxCentreY =
        0.5f * static_cast<float>(
            physical(checkedIndicator.y, scale) +
            physical(checkedIndicator.y + checkedIndicator.height, scale) - 1);
    expect(std::fabs(checkInk.centerX() - checkboxCentreX) <= 1.0f &&
               std::fabs(checkInk.centerY() - checkboxCentreY) <= 1.0f,
           "Checkbox checkmark ink must remain optically centred");

    // Circle controls must remain round and symmetric after fractional-DPR
    // rasterization. Tight regions exclude adjacent labels and tracks.
    verifyRoundColor(
        pixels, width, height, scale,
        {kColumns[4] + 58, 290, 16, 16},
        colors.compoundBrandStroke.rest,
        "Selected Radio must paint visible circular ink",
        "Radio outer circle must retain equal physical diameters",
        "Radio outer circle must not be clipped or visibly asymmetric");
    // Switch owns an 8-DIP horizontal hit inset around its 40x20 track.
    // Reproduce its snapped geometry so fractional-DPR rounding does not make
    // the inspection region itself crop the 18-DIP circle.
    const auto snap = [scale](float logical) {
        return std::round(logical * scale) / scale;
    };
    const float switchTrackLeft = snap(kColumns[4] + 46.0f);
    const float switchTrackTop = snap(332.0f);
    const float switchTrackRight = snap(kColumns[4] + 86.0f);
    const float switchTrackBottom = snap(352.0f);
    const float switchThumbSize = snap(18.0f);
    const float switchThumbInset =
        ((switchTrackBottom - switchTrackTop) - switchThumbSize) * 0.5f;
    const float switchThumbCentreX = snap(
        switchTrackRight - switchThumbSize - switchThumbInset +
        switchThumbSize * 0.5f);
    const float switchThumbCentreY =
        snap((switchTrackTop + switchTrackBottom) * 0.5f);
    const auto switchThumb = findColor(
        pixels, width, height, scale,
        {switchThumbCentreX - 11.0f, switchThumbCentreY - 11.0f,
         22.0f, 22.0f},
        colors.onBrand, 4);
    expect(switchThumb.valid(),
           "Selected Switch must paint a visible round thumb");
    expect(std::abs(switchThumb.width() - switchThumb.height()) <= 1,
           "Switch thumb must retain equal physical diameters");
    expect(std::fabs(
               switchThumb.centerX() -
               (switchThumbCentreX * scale - 0.5f)) <= 1.0f &&
               std::fabs(
                   switchThumb.centerY() -
                   (switchThumbCentreY * scale - 0.5f)) <= 1.0f,
           "Switch thumb must remain centred and unclipped in its track");

    constexpr float kFocusSwitchX = kColumns[3] + 38.0f;
    constexpr float kFocusSwitchY = 324.0f;
    expect(colorNear(
               pixels, width, height,
               physical(kFocusSwitchX - 1.0f, scale),
               physical(kFocusSwitchY + 18.0f, scale),
               colors.neutralBackground2.rest, 4),
           "Switch focus strokes must stay inside the exact 56x36 root");
    expect(findColor(
               pixels, width, height, scale,
               {kFocusSwitchX, kFocusSwitchY + 4.0f, 4.0f, 28.0f},
               colors.strokeFocusOuter, 4).valid(),
           "Focused Switch must retain the inner 2-DIP black focus stroke");

    // Slider's track crosses the thumb centre, so inspect the complete
    // 20-DIP thumb silhouette in a tight 21-DIP region. The track contributes
    // only the middle rows and cannot inflate the measured vertical diameter.
    const float selectedSliderThumbCentre =
        kColumns[4] + 10.0f + (132.0f - 20.0f) * 0.70f;
    constexpr float kSelectedSliderThumbCentreY = 386.0f;
    const wui::RectF sliderThumbRegion{
        selectedSliderThumbCentre - 11.0f,
        kSelectedSliderThumbCentreY - 11.0f, 22, 22};
    const auto sliderThumb = findInkOutsideHorizontalBand(
        pixels, width, height, scale, sliderThumbRegion,
        kSelectedSliderThumbCentreY, 2.5f,
        colors.neutralBackground2.rest);
    expect(sliderThumb.valid(),
           "Slider thumb must paint a visible round silhouette");
    const int expectedSliderDiameter = physical(20.0f, scale);
    expect(std::abs(sliderThumb.width() - expectedSliderDiameter) <= 1 &&
               std::abs(sliderThumb.height() - expectedSliderDiameter) <= 1,
           "Slider thumb must retain its 20-DIP physical diameter");
    expect(std::fabs(
               sliderThumb.centerX() -
               (selectedSliderThumbCentre * scale - 0.5f)) <= 1.0f &&
               std::fabs(
                   sliderThumb.centerY() -
                   (kSelectedSliderThumbCentreY * scale - 0.5f)) <= 1.0f,
           "Slider thumb must remain centred and unclipped around its track");

    // No selection indicator may escape its own allocated hit rectangle.
    const auto background = colors.neutralBackground2.rest;
    for (std::size_t index = 0; index < kColumns.size(); ++index) {
        expect(colorNear(
                   pixels, width, height,
                   physical(kColumns[index] + 49, scale),
                   physical(254, scale), background, 8),
               "Checkbox indicator must not paint outside its hit rectangle");
        expect(colorNear(
                   pixels, width, height,
                   physical(kColumns[index] + 49, scale),
                   physical(298, scale), background, 8),
               "Radio indicator must not paint outside its allocated rectangle");
    }

    wui::setTextMeasurer(nullptr);
    canvas.reset();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::string output =
            argc > 1 ? argv[1]
                     : "fluent_control_opengl_matrix_150dpi.ppm";
        const float scale =
            argc > 2 ? std::stof(argv[2]) : 1.5f;
        return run(output, scale);
    } catch (const std::exception& error) {
        wui::setTextMeasurer(nullptr);
        const std::string_view message{error.what()};
        if (message.find("SKIP:") == 0) {
            std::fprintf(stderr, "%s\n", error.what());
            return 77;
        }
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
