#pragma once

#include "inspector_diagnostics.h"

#include <filesystem>

namespace whatsui::debug_inspector {

void writeInspectorReference(
    InspectorDiagnostics diagnostics,
    const std::filesystem::path& outputDirectory);

} // namespace whatsui::debug_inspector
