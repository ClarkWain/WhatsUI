#pragma once

#include "inspector_diagnostics.h"

#include "wui/declarative.h"

#include <utility>

namespace whatsui::debug_inspector {

class InspectorPage {
public:
    explicit InspectorPage(InspectorDiagnostics diagnostics)
        : diagnostics_(std::move(diagnostics))
    {
    }

    [[nodiscard]] wui::Box body();

private:
    InspectorDiagnostics diagnostics_;
};

} // namespace whatsui::debug_inspector
