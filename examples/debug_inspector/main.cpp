#include "inspector_diagnostics.h"
#include "software_capture.h"

#include <exception>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
{
    using namespace whatsui::debug_inspector;
    try {
        const std::filesystem::path outputDirectory = argc > 1
            ? std::filesystem::path(argv[1])
            : std::filesystem::path("debug_inspector_visual");
        writeInspectorReference(
            collectSampleDiagnostics(), outputDirectory);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Debug Inspector: " << error.what() << std::endl;
        return 1;
    }
}
