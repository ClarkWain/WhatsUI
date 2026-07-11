// UiInspector reference application.
//
// The default entry point captures a deterministic Software-rendered Fluent
// page. Defining WUI_DEBUG_INSPECTOR_INTERACTIVE lets the same declarative
// page run in the GLFW host once this example is wired to WhatsUI::Glfw.

#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "wsc/Canvas.h"

#include "wui/paint_context.h"
#include "wui/theme.h"
#include "wui/ui.h"
#include "wui/ui_inspector.h"
#include "wui/whatscanvas_text.h"

#ifdef WUI_DEBUG_INSPECTOR_INTERACTIVE
#include "wui/glfw_platform.h"
#endif

namespace {

struct Diagnostics {
    wui::UiInspectorSnapshot entries;
    wui::UiDirtySummary dirty;
    std::optional<wui::UiHitPath> hit;
};

std::unique_ptr<wui::Node> buildSampleTree()
{
    using namespace wui::ui;
    const auto& current = wui::theme();
    return Box().background(current.colors.surface).radius(current.radius.lg).padding(16.0f).children(
        Column().gap(12.0f).align(wui::Alignment::Stretch).children(
            Row().align(wui::Alignment::Center).children(
                Column().gap(2.0f).flex(1.0f).children(
                    Text("Sample work item").size(15.0f).lineHeight(21.0f).color(current.colors.text),
                    Text("A retained tree rendered beside its snapshot").size(11.0f).lineHeight(16.0f).color(current.colors.textMuted)),
                Text("LIVE").size(10.0f).lineHeight(14.0f).color(current.colors.accent)),
            Checkbox("Mark the layout pass complete", false),
            Row().align(wui::Alignment::Center).gap(8.0f).children(
                Button("Inspect node").variant(wui::ButtonVariant::Primary),
                Button("Reset").variant(wui::ButtonVariant::Ghost))));
}

std::string formatPath(const std::vector<std::size_t>& path)
{
    if (path.empty()) return "/";
    std::string result;
    for (const auto index : path) result += "/" + std::to_string(index);
    return result;
}

std::string formatRect(const wui::RectF& rect)
{
    const auto rounded = [](float value) { return static_cast<int>(std::lround(value)); };
    return std::to_string(rounded(rect.x)) + ", " + std::to_string(rounded(rect.y)) + "  "
        + std::to_string(rounded(rect.width)) + " x " + std::to_string(rounded(rect.height));
}

std::string formatDirty(wui::DirtyFlags flags)
{
    if (flags == wui::toMask(wui::DirtyFlag::None)) return "clean";
    std::string result;
    const auto append = [&result](const char* value) {
        if (!result.empty()) result += ", ";
        result += value;
    };
    if ((flags & wui::toMask(wui::DirtyFlag::Style)) != 0) append("style");
    if ((flags & wui::toMask(wui::DirtyFlag::Layout)) != 0) append("layout");
    if ((flags & wui::toMask(wui::DirtyFlag::Paint)) != 0) append("paint");
    if ((flags & wui::toMask(wui::DirtyFlag::Compositing)) != 0) append("compositing");
    return result;
}

std::string compactType(std::string type)
{
    // RTTI names differ by compiler. Keep the diagnostic string recognisable
    // while preventing a toolchain-specific name from overwhelming a row.
    if (type.size() > 36) return type.substr(0, 33) + "...";
    return type;
}

Diagnostics collectDiagnostics()
{
    auto sample = buildSampleTree();
    sample->layout({0.0f, 0.0f, 320.0f, 218.0f});
    Diagnostics diagnostics;
    diagnostics.entries = wui::inspectUiTree(*sample);
    diagnostics.dirty = wui::summarizeUiDirty(diagnostics.entries);
    diagnostics.hit = wui::inspectUiHitPath(*sample, {48.0f, 128.0f});
    return diagnostics;
}

std::unique_ptr<wui::Node> buildInspectorPage(const Diagnostics& diagnostics)
{
    using namespace wui::ui;
    const auto& current = wui::theme();
    const std::string summary = std::to_string(diagnostics.dirty.nodeCount) + " nodes  ·  "
        + std::to_string(diagnostics.dirty.dirtyNodeCount) + " dirty  ·  "
        + (diagnostics.dirty.needsRepaint() ? "repaint queued" : "no repaint pending");
    const std::string hit = diagnostics.hit
        ? "Hit " + formatPath(diagnostics.hit->path) + "  ·  " + compactType(diagnostics.hit->type)
            + "  ·  " + formatRect(diagnostics.hit->bounds)
        : "No node was hit by the sample probe.";

    auto entries = std::make_unique<wui::Column>();
    entries->setGap(4.0f);
    entries->setAlign(wui::Alignment::Stretch);
    for (const auto& entry : diagnostics.entries) {
        const std::string title = formatPath(entry.path) + "  " + compactType(entry.type);
        const std::string detail = "rect " + formatRect(entry.bounds) + "  ·  " + formatDirty(entry.dirtyFlags)
            + "  ·  " + std::to_string(entry.childCount) + " children";
        entries->appendChild(Box().background(current.colors.surface).radius(current.radius.sm).padding({10.0f, 6.0f, 10.0f, 6.0f})
            .children(Column().gap(1.0f).children(
                Text(title).size(11.0f).lineHeight(16.0f).color(current.colors.text),
                Text(detail).size(10.0f).lineHeight(14.0f).color(current.colors.textMuted))).intoNode());
    }

    auto diagnosticsPanel = Box().background(current.colors.surfaceAlt).radius(current.radius.lg).padding(16.0f).children(
        Column().gap(10.0f).align(wui::Alignment::Stretch).children(
            Text("Snapshot diagnostics").size(16.0f).lineHeight(22.0f).color(current.colors.text),
            Text(summary).size(11.0f).lineHeight(16.0f).color(current.colors.accent),
            Box().background(current.colors.surface).radius(current.radius.sm).padding(10.0f).children(
                Column().gap(2.0f).children(
                    Text("Hit path probe").size(10.0f).lineHeight(14.0f).color(current.colors.textMuted),
                    Text(hit).wrap().size(11.0f).lineHeight(16.0f).color(current.colors.text))),
            std::move(entries)));

    auto previewPanel = Box().width(340.0f).background(current.colors.surfaceAlt).radius(current.radius.lg).padding(16.0f).children(
        Column().gap(12.0f).align(wui::Alignment::Stretch).children(
            Text("Live sample tree").size(16.0f).lineHeight(22.0f).color(current.colors.text),
            Text("The inspector observes a value snapshot; it never mutates this retained subtree.")
                .wrap().size(11.0f).lineHeight(16.0f).color(current.colors.textMuted),
            buildSampleTree()));

    auto rail = Box().width(1120.0f).padding({32.0f, 26.0f, 32.0f, 28.0f}).children(
        Column().gap(18.0f).align(wui::Alignment::Stretch).children(
            Row().align(wui::Alignment::Center).children(
                Column().gap(2.0f).flex(1.0f).children(
                    Text("UI Inspector").size(30.0f).lineHeight(38.0f).color(current.colors.text),
                    Text("Read-only layout, dirty-state, and hit-test diagnostics for retained WhatsUI trees.")
                        .size(13.0f).lineHeight(18.0f).color(current.colors.textMuted)),
                Text("SOFTWARE REFERENCE").size(10.0f).lineHeight(14.0f).color(current.colors.accent)),
            Row().align(wui::Alignment::Start).gap(16.0f).children(
                std::move(previewPanel), std::move(diagnosticsPanel).flex(1.0f))));
    return Box().background(current.colors.background).children(
        Row().align(wui::Alignment::Stretch).children(Spacer().flex(1.0f), std::move(rail), Spacer().flex(1.0f)));
}

void render(wui::Node& root, wsc::Canvas& canvas, float scale)
{
    wui::WhatsCanvasTextMeasurer text(canvas, scale);
    wui::setTextMeasurer(&text);
    root.layout({0.0f, 0.0f, static_cast<float>(canvas.getWidth()) / scale, static_cast<float>(canvas.getHeight()) / scale});
    wui::PaintContext paint(canvas, scale);
    root.prepare(paint);
    for (int pass = 0; pass < 2; ++pass) {
        canvas.beginFrame();
        paint.fillRect(root.bounds(), wui::theme().colors.background);
        root.paint(paint);
        canvas.endFrame();
    }
    wui::setTextMeasurer(nullptr);
}

} // namespace

int main(int argc, char** argv)
{
    const Diagnostics diagnostics = collectDiagnostics();
#ifdef WUI_DEBUG_INSPECTOR_INTERACTIVE
    try {
        return wui::runGlfwApp("WhatsUI UI Inspector", {1200.0f, 760.0f}, [diagnostics](wui::UiWindow&) {
            return buildInspectorPage(diagnostics);
        });
    } catch (const std::exception& error) {
        std::cerr << "FATAL: " << error.what() << std::endl;
        return 1;
    }
#else
    const std::filesystem::path output = argc > 1 ? argv[1] : "debug_inspector_visual";
    std::filesystem::create_directories(output);
    constexpr float scale = 2.0f;
    auto canvas = wsc::Canvas::create(wsc::Canvas::Backend::Software, 2400, 1520);
    if (!canvas || !canvas->initializeContext()) throw std::runtime_error("failed to create software canvas");
    auto root = buildInspectorPage(diagnostics);
    render(*root, *canvas, scale);
    const auto path = output / "ui_inspector_reference.ppm";
    if (!canvas->savePixelsPPM(path.string())) throw std::runtime_error("failed to save inspector capture");
    std::cout << "wrote " << path << std::endl;
    return 0;
#endif
}
