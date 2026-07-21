#include <algorithm>
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
#include "wui/icons.h"
#include "wui/overlays.h"
#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/whatscanvas_text.h"
#include "wui/widgets.h"

namespace {

constexpr int kLogicalWidth = 520;
constexpr int kLogicalHeight = 250;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool near(std::uint8_t actual, std::uint8_t expected, int tolerance = 3)
{
    return std::abs(static_cast<int>(actual) - static_cast<int>(expected))
        <= tolerance;
}

bool pixelNear(const std::vector<unsigned char>& pixels, int width, int height,
               int x, int y, wui::Color expected, int tolerance = 3)
{
    if (x < 0 || y < 0 || x >= width || y >= height) return false;
    const auto offset =
        static_cast<std::size_t>((y * width + x) * 4);
    return offset + 3 < pixels.size()
        && near(pixels[offset], expected.r, tolerance)
        && near(pixels[offset + 1], expected.g, tolerance)
        && near(pixels[offset + 2], expected.b, tolerance)
        && near(pixels[offset + 3], expected.a, tolerance);
}

struct InkBounds {
    int left{0};
    int top{0};
    int right{-1};
    int bottom{-1};
};

InkBounds iconInkBounds(const std::vector<unsigned char>& pixels,
                        int width, int height, int left, int top,
                        int right, int bottom)
{
    InkBounds ink{right, bottom, left - 1, top - 1};
    for (int y = std::max(0, top); y < std::min(height, bottom); ++y) {
        for (int x = std::max(0, left); x < std::min(width, right); ++x) {
            const auto offset =
                static_cast<std::size_t>((y * width + x) * 4);
            // Both neutralForeground2 and brandForeground1 are materially
            // darker than the selected/hover surfaces used by this probe.
            if (pixels[offset] < 190 && pixels[offset + 1] < 190
                && pixels[offset + 2] < 220) {
                ink.left = std::min(ink.left, x);
                ink.top = std::min(ink.top, y);
                ink.right = std::max(ink.right, x);
                ink.bottom = std::max(ink.bottom, y);
            }
        }
    }
    return ink;
}

void writePpm(const std::string& path,
              const std::vector<unsigned char>& rgba,
              int width, int height)
{
    std::ofstream output(path, std::ios::binary);
    expect(output.good(), "OpenGL Button visual output could not be opened");
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

int run(const std::string& outputPath, float scale)
{
    scale = std::max(1.0f, scale);
    const int width =
        static_cast<int>(std::lround(kLogicalWidth * scale));
    const int height =
        static_cast<int>(std::lround(kLogicalHeight * scale));

    expect(glfwInit() == GLFW_TRUE,
           "SKIP: native GLFW/OpenGL surface unavailable: glfwInit failed");
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(
        width, height, "WhatsUI Fluent Button OpenGL visual", nullptr, nullptr);
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
    const auto& colors = wui::theme().colors;

    wui::Button primary("Add");
    primary.setAppearance(wui::ButtonAppearance::Primary);
    wui::Button secondary("Clear done");
    secondary.setAppearance(wui::ButtonAppearance::Secondary);
    wui::Button edit("Edit");
    edit.setAppearance(wui::ButtonAppearance::Subtle);
    wui::IconButton star(wui::IconName::Star, "Important");
    star.setChecked(true);
    wui::IconButton remove(wui::IconName::Delete, "Delete");
    remove.setVisualState(wui::ControlVisualState::Hovered, true);
    wui::Button pressed("Pressed");
    pressed.setAppearance(wui::ButtonAppearance::Primary);
    pressed.setVisualState(wui::ControlVisualState::Pressed, true);
    wui::Button focusVisible("Keyboard focus");
    focusVisible.setAppearance(wui::ButtonAppearance::Secondary);
    focusVisible.setVisualState(wui::ControlVisualState::Focused, true);
    focusVisible.setVisualState(wui::ControlVisualState::FocusVisible, true);

    wui::PaintContext paint(*canvas, scale, true);
    canvas->beginFrame();
    paint.fillRect({0.0f, 0.0f,
                    static_cast<float>(kLogicalWidth),
                    static_cast<float>(kLogicalHeight)},
                   colors.neutralBackground2.rest);
    paintNode(primary, {24, 28, 64, 32}, paint);
    paintNode(secondary, {112, 28, 108, 32}, paint);
    paintNode(edit, {244, 28, 56, 32}, paint);
    paintNode(star, {324, 28, 32, 32}, paint);
    paintNode(remove, {380, 28, 32, 32}, paint);
    paintNode(pressed, {24, 92, 92, 32}, paint);
    paintNode(focusVisible, {144, 92, 132, 32}, paint);
    canvas->endFrame();

    const auto pixels = canvas->readPixelsRGBA();
    expect(pixels.size()
               == static_cast<std::size_t>(width * height * 4),
           "OpenGL Button visual readback must contain a complete frame");

    const auto px = [scale](float value) {
        return static_cast<int>(std::lround(value * scale));
    };
    expect(pixelNear(pixels, width, height, px(28), px(44),
                     colors.brandBackground.rest),
           "OpenGL Primary Button must retain the Fluent rest fill");
    expect(pixelNear(pixels, width, height, px(28), px(108),
                     colors.brandBackground.pressed),
           "OpenGL Primary Button must expose the Fluent pressed fill");
    expect(pixelNear(pixels, width, height, px(328), px(44),
                     colors.neutralBackground1.selected),
           "OpenGL selected icon Button must expose the Fluent Subtle selected fill");
    expect(pixelNear(pixels, width, height, px(384), px(44),
                     colors.neutralBackground1.hover),
           "OpenGL icon Button must expose the Fluent Subtle hover fill");
    expect(pixelNear(pixels, width, height, px(248), px(32),
                     colors.neutralBackground2.rest),
           "OpenGL Subtle Button rest state must remain transparent");

    for (const auto bounds : {wui::RectF{324, 28, 32, 32},
                              wui::RectF{380, 28, 32, 32}}) {
        const int left = px(bounds.x);
        const int top = px(bounds.y);
        const int right = px(bounds.x + bounds.width);
        const int bottom = px(bounds.y + bounds.height);
        const auto ink = iconInkBounds(
            pixels, width, height, left, top, right, bottom);
        expect(ink.right >= ink.left && ink.bottom >= ink.top,
               "OpenGL icon Button must paint visible icon ink");
        const float inkCenterX =
            static_cast<float>(ink.left + ink.right) * 0.5f;
        const float inkCenterY =
            static_cast<float>(ink.top + ink.bottom) * 0.5f;
        const float buttonCenterX =
            static_cast<float>(left + right - 1) * 0.5f;
        const float buttonCenterY =
            static_cast<float>(top + bottom - 1) * 0.5f;
        expect(std::fabs(inkCenterX - buttonCenterX) <= 1.0f
                   && std::fabs(inkCenterY - buttonCenterY) <= 1.0f,
               "OpenGL icon Button visible ink must stay within one physical pixel of the control centre");
    }

    writePpm(outputPath, pixels, width, height);
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
            argc > 1 ? argv[1] : "fluent_button_opengl_150dpi.ppm";
        const float scale =
            argc > 2 ? std::stof(argv[2]) : 1.5f;
        return run(output, scale);
    } catch (const std::exception& error) {
        const std::string_view message{error.what()};
        if (message.find("SKIP:") == 0) {
            std::fprintf(stderr, "%s\n", error.what());
            return 77;
        }
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
