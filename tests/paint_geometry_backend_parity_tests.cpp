#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "wsc/Canvas.h"

#include "wui/basic_controls.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/widgets.h"

namespace {

constexpr int kLogicalWidth = 192;
constexpr int kLogicalHeight = 172;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool near(std::uint8_t actual, std::uint8_t expected, int tolerance = 2)
{
    return std::abs(static_cast<int>(actual) -
                    static_cast<int>(expected)) <= tolerance;
}

bool isBackground(const std::vector<std::uint8_t>& pixels, int width,
                  int x, int y, wui::Color background)
{
    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
    return offset + 3 < pixels.size() &&
        near(pixels[offset], background.r) &&
        near(pixels[offset + 1], background.g) &&
        near(pixels[offset + 2], background.b) &&
        near(pixels[offset + 3], background.a);
}

struct Bounds {
    int left{std::numeric_limits<int>::max()};
    int top{std::numeric_limits<int>::max()};
    int right{-1};
    int bottom{-1};

    [[nodiscard]] bool valid() const noexcept
    {
        return right >= left && bottom >= top;
    }
    [[nodiscard]] int width() const noexcept { return right - left + 1; }
    [[nodiscard]] int height() const noexcept { return bottom - top + 1; }
};

Bounds inkBounds(const std::vector<std::uint8_t>& pixels, int width, int height,
                 float scale, wui::RectF logicalRegion, wui::Color background)
{
    const int left = std::clamp(
        static_cast<int>(std::floor(logicalRegion.x * scale)), 0, width);
    const int top = std::clamp(
        static_cast<int>(std::floor(logicalRegion.y * scale)), 0, height);
    const int right = std::clamp(
        static_cast<int>(std::ceil(
            (logicalRegion.x + logicalRegion.width) * scale)),
        0, width);
    const int bottom = std::clamp(
        static_cast<int>(std::ceil(
            (logicalRegion.y + logicalRegion.height) * scale)),
        0, height);
    Bounds result;
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            if (isBackground(pixels, width, x, y, background)) continue;
            result.left = std::min(result.left, x);
            result.top = std::min(result.top, y);
            result.right = std::max(result.right, x);
            result.bottom = std::max(result.bottom, y);
        }
    }
    return result;
}

int mirroredMismatch(const std::vector<std::uint8_t>& pixels, int width,
                     const Bounds& bounds, wui::Color background)
{
    int mismatch = 0;
    for (int y = bounds.top; y <= bounds.bottom; ++y) {
        for (int x = bounds.left; x <= bounds.right; ++x) {
            const int reflectedX = bounds.left + bounds.right - x;
            const int reflectedY = bounds.top + bounds.bottom - y;
            const bool ink =
                !isBackground(pixels, width, x, y, background);
            mismatch += ink !=
                !isBackground(pixels, width, reflectedX, y, background);
            mismatch += ink !=
                !isBackground(pixels, width, x, reflectedY, background);
        }
    }
    return mismatch / 2;
}

int verticalInkRun(const std::vector<std::uint8_t>& pixels, int width,
                   int height, int x, int top, int bottom,
                   wui::Color background)
{
    int longest = 0;
    int current = 0;
    for (int y = std::max(0, top); y <= std::min(height - 1, bottom); ++y) {
        if (!isBackground(pixels, width, x, y, background)) {
            longest = std::max(longest, ++current);
        } else {
            current = 0;
        }
    }
    return longest;
}

template <typename NodeType>
void paintNode(NodeType& node, wui::PaintContext& paint, wui::RectF bounds)
{
    node.layout(bounds);
    node.prepare(paint);
    node.paint(paint);
}

void paintProbe(wsc::Canvas& canvas, wui::PaintContext& paint)
{
    const auto& colors = wui::theme().colors;
    canvas.beginFrame();
    paint.fillRect({0, 0, static_cast<float>(kLogicalWidth),
                    static_cast<float>(kLogicalHeight)},
                   colors.neutralBackground2.rest);

    wui::Radio radio("", true);
    wui::Checkbox checkbox("", true);
    wui::Switch toggle("", true);
    wui::Slider slider(0, 100, 50);
    wui::Button button("");
    button.setAppearance(wui::ButtonAppearance::Primary);

    paintNode(radio, paint, {12, 12, 32, 32});
    paintNode(checkbox, paint, {56, 12, 32, 32});
    paintNode(toggle, paint, {100, 10, 56, 36});
    paintNode(slider, paint, {12, 62, 144, 32});
    paintNode(button, paint, {12, 116, 144, 32});
    canvas.endFrame();
}

std::vector<std::uint8_t> renderSoftware(int width, int height, float scale)
{
    auto canvas = wsc::Canvas::create(
        wsc::Canvas::Backend::Software, width, height);
    expect(canvas && canvas->initializeContext(),
           "Software geometry parity surface must initialize");
    wui::PaintContext paint(*canvas, scale);
    paintProbe(*canvas, paint);
    auto pixels = canvas->readPixelsRGBA();
    expect(pixels.size() == static_cast<std::size_t>(width * height * 4),
           "Software geometry parity readback must be complete");
    return pixels;
}

std::vector<std::uint8_t> renderOpenGL(GLFWwindow* window,
                                       int width, int height, float scale)
{
    glfwMakeContextCurrent(window);
    auto canvas =
        wsc::Canvas::create(wsc::Canvas::Backend::OpenGL, width, height);
    expect(canvas && canvas->initializeContext(),
           "OpenGL geometry parity surface must initialize");
    canvas->setSize(width, height);
    canvas->setDevicePixelRatio(scale);
    expect(canvas->setOutputTarget(
               wsc::OutputTarget::GLFramebuffer(0, width, height, true)),
           "OpenGL geometry parity framebuffer must bind");
    wui::PaintContext paint(*canvas, scale, true);
    paintProbe(*canvas, paint);
    auto pixels = canvas->readPixelsRGBA();
    expect(pixels.size() == static_cast<std::size_t>(width * height * 4),
           "OpenGL geometry parity readback must be complete");
    return pixels;
}

void compareRegion(const std::vector<std::uint8_t>& software,
                   const std::vector<std::uint8_t>& openGL,
                   int width, int height, float scale,
                   wui::RectF region, const char* component,
                   bool requireSymmetry = false)
{
    const auto background = wui::theme().colors.neutralBackground2.rest;
    const Bounds softwareBounds =
        inkBounds(software, width, height, scale, region, background);
    const Bounds openGLBounds =
        inkBounds(openGL, width, height, scale, region, background);
    expect(softwareBounds.valid() && openGLBounds.valid(),
           "Both renderers must produce visible component geometry");
    const auto close = [](int first, int second) {
        return std::abs(first - second) <= 1;
    };
    if (!close(softwareBounds.left, openGLBounds.left) ||
        !close(softwareBounds.top, openGLBounds.top) ||
        !close(softwareBounds.right, openGLBounds.right) ||
        !close(softwareBounds.bottom, openGLBounds.bottom) ||
        !close(softwareBounds.width(), openGLBounds.width()) ||
        !close(softwareBounds.height(), openGLBounds.height())) {
        throw std::runtime_error(
            std::string(component) +
            " Software/OpenGL structural bounds diverged by more than one physical pixel");
    }
    if (requireSymmetry) {
        const int perimeter =
            std::max(1, 2 * (softwareBounds.width() + softwareBounds.height()));
        const int budget = std::max(4, perimeter / 12);
        expect(mirroredMismatch(software, width, softwareBounds, background) <=
                   budget,
               "Software circular geometry must remain centre-symmetric");
        expect(mirroredMismatch(openGL, width, openGLBounds, background) <=
                   budget,
               "OpenGL circular geometry must remain centre-symmetric");
    }
}

int run(float scale)
{
    scale = std::max(1.0f, scale);
    const int width =
        static_cast<int>(std::lround(kLogicalWidth * scale));
    const int height =
        static_cast<int>(std::lround(kLogicalHeight * scale));
    const auto software = renderSoftware(width, height, scale);

    expect(glfwInit() == GLFW_TRUE,
           "SKIP: native GLFW/OpenGL surface unavailable: glfwInit failed");
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(
        width, height, "WhatsUI geometry parity", nullptr, nullptr);
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
    const auto openGL = renderOpenGL(window, width, height, scale);

    compareRegion(software, openGL, width, height, scale,
                  {10, 10, 36, 36}, "Radio", true);
    compareRegion(software, openGL, width, height, scale,
                  {54, 10, 36, 36}, "Checkbox");
    compareRegion(software, openGL, width, height, scale,
                  {98, 8, 60, 40}, "Switch");
    compareRegion(software, openGL, width, height, scale,
                  {10, 60, 148, 36}, "Slider", true);
    compareRegion(software, openGL, width, height, scale,
                  {10, 114, 148, 36}, "Button", true);

    // Sample the Slider far from the thumb and rounded end caps. Its 4-DIP
    // rail must snap to the same whole output-pixel thickness in both
    // backends, including the fractional 125% and 150% cases.
    const int railX = static_cast<int>(std::lround(40.0f * scale));
    const int railTop = static_cast<int>(std::floor(72.0f * scale));
    const int railBottom = static_cast<int>(std::ceil(84.0f * scale));
    const int expectedRailThickness =
        std::max(1, static_cast<int>(std::lround(4.0f * scale)));
    const auto background = wui::theme().colors.neutralBackground2.rest;
    expect(verticalInkRun(software, width, height, railX, railTop,
                          railBottom, background) ==
               expectedRailThickness,
           "Software Slider rail must occupy the rounded 4-DIP physical-pixel count");
    expect(verticalInkRun(openGL, width, height, railX, railTop,
                          railBottom, background) ==
               expectedRailThickness,
           "OpenGL Slider rail must occupy the rounded 4-DIP physical-pixel count");

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const float scale = argc > 1 ? std::stof(argv[1]) : 1.0f;
        return run(scale);
    } catch (const std::exception& error) {
        const std::string message = error.what();
        std::cerr << "Paint geometry backend parity failure: "
                  << message << '\n';
        return message.rfind("SKIP:", 0) == 0 ? 77 : 1;
    }
}
