#include "inspector_components.h"

#include "sample_tree.h"

#include "wui/theme.h"

#include <cmath>
#include <string>
#include <vector>

namespace whatsui::debug_inspector {
namespace {

std::string formatPath(const std::vector<std::size_t>& path)
{
    if (path.empty()) return "/";

    std::string result;
    for (const std::size_t index : path) {
        result += "/" + std::to_string(index);
    }
    return result;
}

std::string formatRect(const wui::RectF& rect)
{
    const auto rounded = [](float value) {
        return static_cast<int>(std::lround(value));
    };
    return std::to_string(rounded(rect.x)) + ", "
        + std::to_string(rounded(rect.y)) + "  "
        + std::to_string(rounded(rect.width)) + " x "
        + std::to_string(rounded(rect.height));
}

std::string formatDirty(wui::DirtyFlags flags)
{
    if (flags == wui::toMask(wui::DirtyFlag::None)) return "clean";

    std::string result;
    const auto append = [&result](const char* value) {
        if (!result.empty()) result += ", ";
        result += value;
    };
    if ((flags & wui::toMask(wui::DirtyFlag::Style)) != 0) {
        append("style");
    }
    if ((flags & wui::toMask(wui::DirtyFlag::Layout)) != 0) {
        append("layout");
    }
    if ((flags & wui::toMask(wui::DirtyFlag::Paint)) != 0) {
        append("paint");
    }
    if ((flags & wui::toMask(wui::DirtyFlag::Compositing)) != 0) {
        append("compositing");
    }
    return result;
}

std::string compactType(std::string type)
{
    constexpr std::size_t maximumLength = 36;
    constexpr std::size_t prefixLength = 33;
    if (type.size() > maximumLength) {
        return type.substr(0, prefixLength) + "...";
    }
    return type;
}

std::string formatSummary(const InspectorDiagnostics& diagnostics)
{
    return std::to_string(diagnostics.dirty.nodeCount) + " nodes  ·  "
        + std::to_string(diagnostics.dirty.dirtyNodeCount) + " dirty  ·  "
        + (diagnostics.dirty.needsRepaint()
               ? "repaint queued"
               : "no repaint pending");
}

std::string formatHit(const InspectorDiagnostics& diagnostics)
{
    if (!diagnostics.hit) {
        return "No node was hit by the sample probe.";
    }
    return "Hit " + formatPath(diagnostics.hit->path) + "  ·  "
        + compactType(diagnostics.hit->type) + "  ·  "
        + formatRect(diagnostics.hit->bounds);
}

wui::Box buildEntryRow(const wui::UiInspectorEntry& entry)
{
    using namespace wui;
    const auto& current = theme();
    const std::string title =
        formatPath(entry.path) + "  " + compactType(entry.type);
    const std::string detail = "rect " + formatRect(entry.bounds)
        + "  ·  " + formatDirty(entry.dirtyFlags)
        + "  ·  " + std::to_string(entry.childCount) + " children";

    return Box()
        .background(current.colors.surface)
        .radius(current.radius.sm)
        .padding({10.0f, 6.0f, 10.0f, 6.0f})
        .children(
            Column()
                .gap(1.0f)
                .children(
                    Text(title)
                        .size(11.0f)
                        .lineHeight(16.0f)
                        .color(current.colors.text),
                    Text(detail)
                        .size(10.0f)
                        .lineHeight(14.0f)
                        .color(current.colors.textMuted)
                )
        );
}

wui::Column buildEntryList(const wui::UiInspectorSnapshot& entries)
{
    wui::Column list;
    list.gap(4.0f).align(wui::Alignment::Stretch);
    for (const auto& entry : entries) {
        list.children(buildEntryRow(entry));
    }
    return list;
}

class InspectorHeader {
public:
    wui::Row body()
    {
        using namespace wui;
        const auto& current = theme();

        return Row()
            .align(Alignment::Center)
            .children(
                Column()
                    .gap(2.0f)
                    .flex(1.0f)
                    .children(
                        Text("UI Inspector")
                            .size(30.0f)
                            .lineHeight(38.0f)
                            .color(current.colors.text),
                        Text("Read-only layout, dirty-state, and hit-test diagnostics for retained WhatsUI trees.")
                            .size(13.0f)
                            .lineHeight(18.0f)
                            .color(current.colors.textMuted)
                    ),
                Text("SOFTWARE REFERENCE")
                    .size(10.0f)
                    .lineHeight(14.0f)
                    .color(current.colors.accent)
            );
    }
};

class PreviewPanel {
public:
    wui::Box body()
    {
        using namespace wui;
        const auto& current = theme();

        return Box()
            .width(340.0f)
            .background(current.colors.surfaceAlt)
            .radius(current.radius.lg)
            .padding(16.0f)
            .children(
                Column()
                    .gap(12.0f)
                    .align(Alignment::Stretch)
                    .children(
                        Text("Live sample tree")
                            .size(16.0f)
                            .lineHeight(22.0f)
                            .color(current.colors.text),
                        Text("The inspector observes a value snapshot; it never mutates this retained subtree.")
                            .wrap()
                            .size(11.0f)
                            .lineHeight(16.0f)
                            .color(current.colors.textMuted),
                        SampleTree()
                    )
            );
    }
};

class DiagnosticsPanel {
public:
    explicit DiagnosticsPanel(const InspectorDiagnostics& diagnostics)
        : diagnostics_(&diagnostics)
    {
    }

    wui::Box body()
    {
        using namespace wui;
        const auto& current = theme();

        return Box()
            .background(current.colors.surfaceAlt)
            .radius(current.radius.lg)
            .padding(16.0f)
            .flex(1.0f)
            .children(
                Column()
                    .gap(10.0f)
                    .align(Alignment::Stretch)
                    .children(
                        Text("Snapshot diagnostics")
                            .size(16.0f)
                            .lineHeight(22.0f)
                            .color(current.colors.text),
                        Text(formatSummary(*diagnostics_))
                            .size(11.0f)
                            .lineHeight(16.0f)
                            .color(current.colors.accent),
                        Box()
                            .background(current.colors.surface)
                            .radius(current.radius.sm)
                            .padding(10.0f)
                            .children(
                                Column()
                                    .gap(2.0f)
                                    .children(
                                        Text("Hit path probe")
                                            .size(10.0f)
                                            .lineHeight(14.0f)
                                            .color(current.colors.textMuted),
                                        Text(formatHit(*diagnostics_))
                                            .wrap()
                                            .size(11.0f)
                                            .lineHeight(16.0f)
                                            .color(current.colors.text)
                                    )
                            ),
                        buildEntryList(diagnostics_->entries)
                    )
            );
    }

private:
    const InspectorDiagnostics* diagnostics_;
};

} // namespace

wui::Box InspectorRail::body()
{
    using namespace wui;

    return Box()
        .width(1120.0f)
        .padding({32.0f, 26.0f, 32.0f, 28.0f})
        .children(
            Column()
                .gap(18.0f)
                .align(Alignment::Stretch)
                .children(
                    InspectorHeader(),
                    Row()
                        .align(Alignment::Start)
                        .gap(16.0f)
                        .children(
                            PreviewPanel(),
                            DiagnosticsPanel(*diagnostics_)
                        )
                )
        );
}

} // namespace whatsui::debug_inspector
