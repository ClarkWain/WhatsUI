#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
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

struct NativeSize {
    int windowWidth{0};
    int windowHeight{0};
    int framebufferWidth{0};
    int framebufferHeight{0};
};

NativeSize nativeSize(GLFWwindow* window)
{
    NativeSize result;
    glfwGetWindowSize(window, &result.windowWidth, &result.windowHeight);
    glfwGetFramebufferSize(window, &result.framebufferWidth, &result.framebufferHeight);
    return result;
}

NativeSize resizeNativeWindowAndWait(
    GLFWwindow* nativeWindow,
    const wui::RenderSurface& surface,
    int requestedWidth,
    int requestedHeight)
{
    glfwSetWindowSize(nativeWindow, requestedWidth, requestedHeight);

    // Win32 normally delivers the framebuffer callback synchronously from
    // glfwSetWindowSize. Keep a short event-pump deadline for other desktop
    // implementations and loaded CI machines without turning a missing
    // callback into a race-dependent assertion.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    NativeSize actual;
    do {
        glfwPollEvents();
        actual = nativeSize(nativeWindow);
        const auto published = surface.framebufferSize();
        if (actual.windowWidth == requestedWidth && actual.windowHeight == requestedHeight
            && actual.framebufferWidth > 0 && actual.framebufferHeight > 0
            && published.width == static_cast<float>(actual.framebufferWidth)
            && published.height == static_cast<float>(actual.framebufferHeight)) {
            return actual;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);

    throw std::runtime_error(
        "Native GLFW resize did not reach the framebuffer callback and RenderSurface before the deadline");
}

void expectMetricsMatchNative(const wui::PlatformWindow& window, GLFWwindow* nativeWindow, const NativeSize& native)
{
    const auto metrics = window.metrics();
    expect(metrics.framebufferSize.width == static_cast<float>(native.framebufferWidth)
               && metrics.framebufferSize.height == static_cast<float>(native.framebufferHeight),
           "PlatformWindow metrics must publish the framebuffer dimensions produced by native resize");

#if defined(_WIN32)
    float xScale = 1.0f;
    float yScale = 1.0f;
    glfwGetWindowContentScale(nativeWindow, &xScale, &yScale);
    expect(xScale > 0.0f && yScale > 0.0f,
           "Windows native resize regression requires a valid GLFW content scale");
    const float expectedLogicalWidth = static_cast<float>(native.windowWidth) / xScale;
    const float expectedLogicalHeight = static_cast<float>(native.windowHeight) / xScale;
    expect(std::fabs(metrics.logicalSize.width - expectedLogicalWidth) < 0.01f
               && std::fabs(metrics.logicalSize.height - expectedLogicalHeight) < 0.01f,
           "Windows PlatformWindow logical size must remain native client pixels divided by content scale");
#else
    (void)nativeWindow;
    expect(metrics.logicalSize.width == static_cast<float>(native.windowWidth)
               && metrics.logicalSize.height == static_cast<float>(native.windowHeight),
           "PlatformWindow logical size must track GLFW window size after native resize");
#endif
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
    GLFWwindow* nativeWindow = glfwGetCurrentContext();
    expect(nativeWindow != nullptr,
           "The GLFW resize regression requires the created window's OpenGL context to be current");

    const auto getIntegerv = reinterpret_cast<GlGetIntegervProc>(glfwGetProcAddress("glGetIntegerv"));
    const auto getError = reinterpret_cast<GlGetErrorProc>(glfwGetProcAddress("glGetError"));
    expect(getIntegerv != nullptr && getError != nullptr,
           "The GLFW OpenGL context must expose viewport inspection functions");

    // This sequence mirrors the Todo failure: expand the real native client
    // area, then make it narrower. Deliberately use glfwSetWindowSize rather
    // than calling RenderSurface::resize directly: the regression contract is
    // the complete native event -> framebuffer callback -> Canvas/viewport
    // chain installed by GlfwPlatformHost.
    for (const auto [width, height] : {std::pair{720, 500}, std::pair{1136, 720}, std::pair{480, 560}}) {
        const auto actual = resizeNativeWindowAndWait(nativeWindow, surface, width, height);
        expectSurfaceSize(surface,
                          static_cast<float>(actual.framebufferWidth),
                          static_cast<float>(actual.framebufferHeight));
        expectMetricsMatchNative(*window, nativeWindow, actual);
        expectViewport(getIntegerv, actual.framebufferWidth, actual.framebufferHeight);
        expect(getError() == kGlNoError, "GLFW RenderSurface resize must not leave an OpenGL error");
        surface.beginFrame();
        surface.endFrame();
    }

    // Minimized windows report a zero framebuffer. It must not reset the
    // previously valid viewport or logical surface dimensions.
    const auto lastValid = nativeSize(nativeWindow);
    surface.resize({0.0f, 0.0f});
    expectSurfaceSize(surface,
                      static_cast<float>(lastValid.framebufferWidth),
                      static_cast<float>(lastValid.framebufferHeight));
    expectViewport(getIntegerv, lastValid.framebufferWidth, lastValid.framebufferHeight);

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
