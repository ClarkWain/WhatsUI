#pragma once

#include "inspector_diagnostics.h"

#include "wui/declarative.h"

namespace whatsui::debug_inspector {

class InspectorRail {
public:
    explicit InspectorRail(const InspectorDiagnostics& diagnostics)
        : diagnostics_(&diagnostics)
    {
    }

    [[nodiscard]] wui::Box body();

private:
    const InspectorDiagnostics* diagnostics_;
};

} // namespace whatsui::debug_inspector
