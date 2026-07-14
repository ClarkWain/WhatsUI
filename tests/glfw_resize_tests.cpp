#include <array>
#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "wui/glfw_platform.h"

namespace {

constexpr unsigned int kGlViewport = 0x0BA2;
constexpr unsigned int kGlNoError = 0;

using GlGetIntegervProc = void (*)(unsigned int, int*);
using GlGetErrorProc = unsigned int (*)();

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::fputs(message, stderr);
        std::fputc('\n', stderr);
        throw std::runtime_error(message);
    }
}

std::array<int, 4> viewport(GlGetIntegervProc getIntegerv)
{
    std::array<int, 4> result{};
    getIntegerv(kGlViewport, result.data());
    return result;
}

void expectViewport(GlGetIntegervProc getIntegerv, int width, int height)
{
    const auto actual = viewport(getIntegerv);
    expect(actual[0] == 0 && actual[1] == 0 && actual[2] == width && actual[3] == height,
           "GLFW RenderSurface resize must update the complete OpenGL viewport");
}

void expectSurfaceSize(const wui::RenderSurface& surface, float width, float height)
{
    const auto actual = surface.framebufferSize();
    expect(actual.width == width && actual.height == height,
           "GLFW RenderSurface resize must publish the current framebuffer dimensions");
}

void testViewportTracksFramebufferResize()
{
    auto host = wui::createGlfwPlatformHost();

    // Keep the native regression window out of the user's task switcher while
    // still exercising a real GLFW/OpenGL context. The hint is consumed by
    // the next GLFW window creation only.
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    auto window = host->createWindow("WhatsUI GLFW resize regression", {640.0f, 560.0f});
    auto& surface = window->surface();

    const auto getIntegerv = reinterpret_cast<GlGetIntegervProc>(glfwGetProcAddress("glGetIntegerv"));
    const auto getError = reinterpret_cast<GlGetErrorProc>(glfwGetProcAddress("glGetError"));
    expect(getIntegerv != nullptr && getError != nullptr,
           "The GLFW OpenGL context must expose viewport inspection functions");

    // This sequence mirrors the Todo failure: initial 640x560, then a wider
    // native framebuffer, then a narrower one. Before the viewport fix the
    // first expanded pass retained the old 640x560 viewport and left a black
    // band in the newly available framebuffer area.
    for (const auto [width, height] : {std::pair{640, 560}, std::pair{1136, 720}, std::pair{480, 560}}) {
        surface.resize({static_cast<float>(width), static_cast<float>(height)});
        expectSurfaceSize(surface, static_cast<float>(width), static_cast<float>(height));
        expectViewport(getIntegerv, width, height);
        expect(getError() == kGlNoError, "GLFW RenderSurface resize must not leave an OpenGL error");
        surface.beginFrame();
        surface.endFrame();
    }

    // Minimized windows report a zero framebuffer. It must not reset the
    // previously valid viewport or logical surface dimensions.
    surface.resize({0.0f, 0.0f});
    expectSurfaceSize(surface, 480.0f, 560.0f);
    expectViewport(getIntegerv, 480, 560);

    // Destroy the GL/Win32 resources before the host terminates GLFW.
    window.reset();
}

} // namespace

int main()
{
    try {
        testViewportTracksFramebufferResize();
        return 0;
    } catch (const std::exception& error) {
        // Headless CI images may not have a desktop OpenGL/GLFW surface. CTest
        // maps 77 to "Not Run" so this is visible rather than a false pass;
        // Windows release validation must execute this test on a graphical host.
        const std::string_view message{error.what()};
        if (message.find("glfwInit() failed") != std::string_view::npos
            || message.find("glfwCreateWindow() failed") != std::string_view::npos) {
            std::fprintf(stderr, "SKIP: native GLFW/OpenGL surface unavailable: %s\n", error.what());
            return 77;
        }
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
