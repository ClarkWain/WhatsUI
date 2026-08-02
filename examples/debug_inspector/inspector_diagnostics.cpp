#include "inspector_diagnostics.h"

#include "sample_tree.h"

#include "wui/runtime.h"

namespace whatsui::debug_inspector {

InspectorDiagnostics collectSampleDiagnostics()
{
    wui::UiRoot sample;
    sample.setContent(SampleTree());
    sample.layout({0.0f, 0.0f, 320.0f, 218.0f});

    wui::Node& root = *sample.content();
    InspectorDiagnostics diagnostics;
    diagnostics.entries = wui::inspectUiTree(root);
    diagnostics.dirty = wui::summarizeUiDirty(diagnostics.entries);
    diagnostics.hit = wui::inspectUiHitPath(root, {48.0f, 128.0f});
    return diagnostics;
}

} // namespace whatsui::debug_inspector
