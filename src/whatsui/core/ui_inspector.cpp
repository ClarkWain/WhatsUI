#include "wui/ui_inspector.h"

#include <algorithm>
#include <typeinfo>
#include <utility>

#include "wui/basic_controls.h"
#include "wui/theme.h"
#include "wui/widgets.h"

namespace wui {
namespace {

[[nodiscard]] DirtyFlags dirtyFlagsFor(const Node& node) noexcept
{
    DirtyFlags flags = toMask(DirtyFlag::None);
    constexpr DirtyFlag kFlags[] = {
        DirtyFlag::Style,
        DirtyFlag::Layout,
        DirtyFlag::Paint,
        DirtyFlag::Compositing,
    };
    for (const auto flag : kFlags) {
        if (node.isDirty(flag)) {
            flags |= toMask(flag);
        }
    }
    return flags;
}

[[nodiscard]] UiInspectorEntry::ResolvedStyle makeStyle(const std::string& role, bool enabled)
{
    UiInspectorEntry::ResolvedStyle style;
    style.role = role;
    style.enabled = enabled;
    return style;
}

[[nodiscard]] std::optional<UiInspectorEntry::ResolvedStyle> resolveStyle(const Node& node)
{
    const Theme& current = theme();
    const auto states = [&node] {
        if (const auto* control = dynamic_cast<const ControlNode*>(&node)) return control->visualStates();
        return ControlVisualStates{toMask(ControlVisualState::None)};
    }();
    const bool enabled = (states & toMask(ControlVisualState::Disabled)) == 0;
    const bool hovered = (states & toMask(ControlVisualState::Hovered)) != 0;
    const bool pressed = (states & toMask(ControlVisualState::Pressed)) != 0;

    if (const auto* button = dynamic_cast<const Button*>(&node)) {
        auto style = makeStyle("Button", enabled);
        Color background = current.colors.accent;
        Color foreground = current.colors.onAccent;
        const bool ghost = button->variant() == ButtonVariant::Ghost;
        if (button->variant() == ButtonVariant::Danger) background = current.colors.danger;
        if (ghost) { background = current.colors.surface; foreground = current.colors.text; style.border = current.colors.border; }
        if (!enabled) { background = current.colors.disabled; foreground = current.colors.textDisabled; style.border.reset(); }
        else if (pressed) background = ghost ? current.colors.surfacePressed
            : button->variant() == ButtonVariant::Danger ? scaleColor(current.colors.danger, 0.84f) : current.colors.accentPressed;
        else if (hovered) background = ghost ? current.colors.surfaceHover
            : button->variant() == ButtonVariant::Danger ? scaleColor(current.colors.danger, 0.92f) : current.colors.accentHover;
        style.foreground = foreground;
        style.background = background;
        style.cornerRadius = current.radius.md;
        style.controlExtent = current.controls.height;
        return style;
    }
    if (const auto* checkbox = dynamic_cast<const Checkbox*>(&node)) {
        auto style = makeStyle("Checkbox", enabled);
        Color box = checkbox->isChecked() ? current.colors.accent : current.colors.surface;
        Color border = checkbox->isChecked() ? current.colors.accent : current.colors.borderStrong;
        Color foreground = current.colors.text;
        if (!enabled) { box = current.colors.disabled; border = current.colors.border; foreground = current.colors.textMuted; }
        else if (pressed) box = checkbox->isChecked() ? current.colors.accentPressed : current.colors.surfacePressed;
        else if (hovered) box = checkbox->isChecked() ? current.colors.accentHover : current.colors.surfaceHover;
        style.background = box; style.border = border; style.foreground = foreground;
        style.cornerRadius = current.radius.sm; style.controlExtent = current.controls.checkboxSize;
        return style;
    }
    if (const auto* radio = dynamic_cast<const Radio*>(&node)) {
        auto style = makeStyle("Radio", enabled);
        Color fill = radio->isSelected() ? current.colors.accent : current.colors.surface;
        Color border = radio->isSelected() ? current.colors.accent : current.colors.borderStrong;
        if (!enabled) { fill = current.colors.disabled; border = current.colors.border; }
        else if (pressed) fill = radio->isSelected() ? current.colors.accentPressed : current.colors.surfacePressed;
        else if (hovered) fill = radio->isSelected() ? current.colors.accentHover : current.colors.surfaceHover;
        style.background = fill; style.border = border; style.foreground = enabled ? current.colors.text : current.colors.textDisabled;
        style.cornerRadius = current.radius.pill; style.controlExtent = current.controls.checkboxSize;
        return style;
    }
    if (const auto* toggle = dynamic_cast<const Switch*>(&node)) {
        auto style = makeStyle("Switch", enabled);
        Color fill = toggle->isOn() ? current.colors.accent : current.colors.surface;
        Color border = toggle->isOn() ? current.colors.accent : current.colors.borderStrong;
        if (!enabled) { fill = current.colors.disabled; border = current.colors.border; }
        else if (pressed) fill = toggle->isOn() ? current.colors.accentPressed : current.colors.surfacePressed;
        else if (hovered) fill = toggle->isOn() ? current.colors.accentHover : current.colors.surfaceHover;
        style.background = fill;
        style.border = border;
        // Switch paint uses this foreground for both its compact thumb and
        // label. Disabled switches override both with textDisabled.
        style.foreground = !enabled ? current.colors.textDisabled
            : toggle->isOn() ? current.colors.onAccent : current.colors.textMuted;
        style.cornerRadius = current.radius.pill; style.controlExtent = current.controls.compactHeight;
        return style;
    }
    if (dynamic_cast<const Slider*>(&node) != nullptr) {
        auto style = makeStyle("Slider", enabled);
        style.background = !enabled ? current.colors.textDisabled
            : pressed ? current.colors.accentPressed : hovered ? current.colors.accentHover : current.colors.accent;
        style.border = current.colors.border; style.cornerRadius = current.radius.pill; style.controlExtent = current.controls.height;
        return style;
    }
    if (dynamic_cast<const ProgressBar*>(&node) != nullptr) {
        auto style = makeStyle("ProgressBar", true);
        style.background = current.colors.accent; style.border = current.colors.border; style.cornerRadius = current.radius.pill;
        style.controlExtent = 4.0f;
        return style;
    }
    if (const auto* divider = dynamic_cast<const Divider*>(&node)) {
        auto style = makeStyle("Divider", true);
        style.background = current.colors.border; style.controlExtent = divider->thickness();
        return style;
    }
    return std::nullopt;
}

void appendSnapshot(const Node& node, std::vector<std::size_t>& path, UiInspectorSnapshot& snapshot)
{
    UiInspectorEntry entry;
    entry.path = path;
    entry.depth = path.size();
    entry.type = typeid(node).name();
    entry.bounds = node.bounds();
    entry.measuredConstraints = node.lastMeasuredConstraints();
    entry.dirtyFlags = dirtyFlagsFor(node);
    if (const auto* control = dynamic_cast<const ControlNode*>(&node)) {
        entry.visualStates = control->visualStates();
    }
    entry.childCount = node.children().size();
    entry.resolvedStyle = resolveStyle(node);
    snapshot.push_back(std::move(entry));

    const auto& children = node.children();
    for (std::size_t index = 0; index < children.size(); ++index) {
        path.push_back(index);
        appendSnapshot(*children[index], path, snapshot);
        path.pop_back();
    }
}

[[nodiscard]] bool findPath(const Node& node, const Node* target, std::vector<std::size_t>& path)
{
    if (&node == target) {
        return true;
    }
    const auto& children = node.children();
    for (std::size_t index = 0; index < children.size(); ++index) {
        path.push_back(index);
        if (findPath(*children[index], target, path)) {
            return true;
        }
        path.pop_back();
    }
    return false;
}

} // namespace

UiInspectorSnapshot inspectUiTree(const Node& root)
{
    UiInspectorSnapshot snapshot;
    std::vector<std::size_t> path;
    appendSnapshot(root, path, snapshot);
    return snapshot;
}

std::optional<UiHitPath> inspectUiHitPath(Node& root, PointF point)
{
    Node* const hit = root.hitTest(point);
    if (hit == nullptr) {
        return std::nullopt;
    }

    std::vector<std::size_t> path;
    if (!findPath(root, hit, path)) {
        // A custom Node must return one of the nodes in its own subtree. Do
        // not expose an unstable pointer when that contract is violated.
        return std::nullopt;
    }

    return UiHitPath{std::move(path), typeid(*hit).name(), hit->bounds()};
}

UiDirtySummary summarizeUiDirty(const UiInspectorSnapshot& snapshot) noexcept
{
    UiDirtySummary summary;
    summary.nodeCount = snapshot.size();
    for (const auto& entry : snapshot) {
        if (entry.dirtyFlags == toMask(DirtyFlag::None)) {
            continue;
        }
        ++summary.dirtyNodeCount;
        if ((entry.dirtyFlags & toMask(DirtyFlag::Style)) != 0) ++summary.styleDirtyCount;
        if ((entry.dirtyFlags & toMask(DirtyFlag::Layout)) != 0) ++summary.layoutDirtyCount;
        if ((entry.dirtyFlags & toMask(DirtyFlag::Paint)) != 0) ++summary.paintDirtyCount;
        if ((entry.dirtyFlags & toMask(DirtyFlag::Compositing)) != 0) ++summary.compositingDirtyCount;
    }
    return summary;
}

UiDirtySummary inspectUiDirty(const Node& root)
{
    return summarizeUiDirty(inspectUiTree(root));
}

UiRepaintOverlayModel buildUiRepaintOverlayModel(const UiInspectorSnapshot& snapshot)
{
    UiRepaintOverlayModel model;
    model.regions.reserve(snapshot.size());
    for (const UiInspectorEntry& entry : snapshot) {
        if (entry.dirtyFlags == toMask(DirtyFlag::None) || entry.bounds.width <= 0.0f || entry.bounds.height <= 0.0f) continue;
        model.regions.push_back({entry.path, entry.bounds, entry.dirtyFlags});
        if (!model.unionBounds) {
            model.unionBounds = entry.bounds;
        } else {
            const RectF previous = *model.unionBounds;
            const float left = std::min(previous.x, entry.bounds.x);
            const float top = std::min(previous.y, entry.bounds.y);
            const float right = std::max(previous.x + previous.width, entry.bounds.x + entry.bounds.width);
            const float bottom = std::max(previous.y + previous.height, entry.bounds.y + entry.bounds.height);
            model.unionBounds = RectF{left, top, right - left, bottom - top};
        }
    }
    return model;
}

UiRepaintOverlayModel inspectUiRepaintOverlay(const Node& root)
{
    return buildUiRepaintOverlayModel(inspectUiTree(root));
}

} // namespace wui
