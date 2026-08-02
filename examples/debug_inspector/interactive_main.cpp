#include "inspector_diagnostics.h"
#include "inspector_page.h"

#include "wui/glfw_platform.h"

#include <exception>
#include <iostream>

int main()
{
    using namespace whatsui::debug_inspector;
    try {
        return wui::runGlfwApp(
            "WhatsUI UI Inspector",
            {1200.0f, 760.0f},
            InspectorPage(collectSampleDiagnostics()));
    } catch (const std::exception& error) {
        std::cerr << "Debug Inspector: " << error.what() << std::endl;
        return 1;
    }
}
