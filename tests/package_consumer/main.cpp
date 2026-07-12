#include <wui/wui.h>

#if defined(WHATSUI_PACKAGE_USE_GLFW)
#include <wui/glfw_platform.h>
#endif

int main()
{
#if defined(WHATSUI_PACKAGE_USE_GLFW)
    // Link the installed GLFW backend without opening a native window. This
    // keeps the package smoke suitable for CI desktops while proving the
    // imported target's renderer, GLFW and Windows-system-library closure.
    const auto factory = &wui::createGlfwPlatformHost;
    if (factory == nullptr) {
        return 2;
    }
#endif
    wui::SizeF size{640.0f, 480.0f};
    return size.width == 640.0f && size.height == 480.0f ? 0 : 1;
}
