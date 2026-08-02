#pragma once

#include "wui/ui_inspector.h"

#include <optional>

namespace whatsui::debug_inspector {

struct InspectorDiagnostics {
    wui::UiInspectorSnapshot entries;
    wui::UiDirtySummary dirty;
    std::optional<wui::UiHitPath> hit;
};

[[nodiscard]] InspectorDiagnostics collectSampleDiagnostics();

} // namespace whatsui::debug_inspector
