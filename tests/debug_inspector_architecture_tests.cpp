#include "debug_inspector/inspector_diagnostics.h"
#include "debug_inspector/inspector_page.h"
#include "debug_inspector/sample_tree.h"

#include "wui/runtime.h"
#include "wui/ui_inspector.h"
#include "wui/view.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

using namespace whatsui::debug_inspector;

static_assert(wui::isViewLikeV<SampleTree>);
static_assert(wui::isViewLikeV<InspectorPage>);
static_assert(!std::is_base_of_v<wui::Node, InspectorPage>);

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

void diagnosticsRemainIndependentFromPresentation()
{
    const InspectorDiagnostics diagnostics = collectSampleDiagnostics();

    expect(!diagnostics.entries.empty(),
           "The sample collector must produce a non-empty tree snapshot");
    expect(diagnostics.dirty.nodeCount == diagnostics.entries.size(),
           "The collector must summarize the same immutable snapshot");
    expect(diagnostics.hit.has_value(),
           "The deterministic sample probe must hit a retained node");
}

void inspectorPageIsAnOrdinaryViewLikeComponent()
{
    const InspectorDiagnostics diagnostics = collectSampleDiagnostics();
    wui::UiRoot root;
    root.setContent(InspectorPage(diagnostics));
    root.layout({0.0f, 0.0f, 1200.0f, 760.0f});

    expect(root.content() != nullptr,
           "The inspector page must materialize through the ordinary UI boundary");
    expect(wui::inspectUiTree(*root.content()).size()
               > diagnostics.entries.size(),
           "The inspector page must render diagnostic rows around the sample tree");
}

} // namespace

int main()
{
    try {
        diagnosticsRemainIndependentFromPresentation();
        inspectorPageIsAnOrdinaryViewLikeComponent();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Debug Inspector architecture test failure: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
