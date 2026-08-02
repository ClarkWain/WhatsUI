#include "wui/ui_inspector.h"

#include "ui_inspector_style.h"

#include "wui/basic_controls.h"

#include <algorithm>
#include <typeinfo>
#include <utility>

namespace wui {
namespace {

DirtyFlags dirtyFlagsFor(const Node& node) noexcept
{
    DirtyFlags flags = toMask(DirtyFlag::None);
    constexpr DirtyFlag inspectedFlags[] = {
        DirtyFlag::Style,
        DirtyFlag::Layout,
        DirtyFlag::Paint,
        DirtyFlag::Compositing,
    };
    for (const DirtyFlag flag : inspectedFlags) {
        if (node.isDirty(flag)) flags |= toMask(flag);
    }
    return flags;
}

void appendSnapshot(
    const Node& node,
    std::vector<std::size_t>& path,
    UiInspectorSnapshot& snapshot)
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
    entry.resolvedStyle = detail::resolveInspectorStyle(node);
    snapshot.push_back(std::move(entry));

    const auto& children = node.children();
    for (std::size_t index = 0; index < children.size(); ++index) {
        path.push_back(index);
        appendSnapshot(*children[index], path, snapshot);
        path.pop_back();
    }
}

bool findPath(
    const Node& node,
    const Node* target,
    std::vector<std::size_t>& path)
{
    if (&node == target) return true;

    const auto& children = node.children();
    for (std::size_t index = 0; index < children.size(); ++index) {
        path.push_back(index);
        if (findPath(*children[index], target, path)) return true;
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
    if (hit == nullptr) return std::nullopt;

    std::vector<std::size_t> path;
    if (!findPath(root, hit, path)) {
        // A custom Node must return a member of its own subtree.
        return std::nullopt;
    }
    return UiHitPath{std::move(path), typeid(*hit).name(), hit->bounds()};
}

UiDirtySummary summarizeUiDirty(
    const UiInspectorSnapshot& snapshot) noexcept
{
    UiDirtySummary summary;
    summary.nodeCount = snapshot.size();
    for (const auto& entry : snapshot) {
        if (entry.dirtyFlags == toMask(DirtyFlag::None)) continue;

        ++summary.dirtyNodeCount;
        if ((entry.dirtyFlags & toMask(DirtyFlag::Style)) != 0) {
            ++summary.styleDirtyCount;
        }
        if ((entry.dirtyFlags & toMask(DirtyFlag::Layout)) != 0) {
            ++summary.layoutDirtyCount;
        }
        if ((entry.dirtyFlags & toMask(DirtyFlag::Paint)) != 0) {
            ++summary.paintDirtyCount;
        }
        if ((entry.dirtyFlags & toMask(DirtyFlag::Compositing)) != 0) {
            ++summary.compositingDirtyCount;
        }
    }
    return summary;
}

UiDirtySummary inspectUiDirty(const Node& root)
{
    return summarizeUiDirty(inspectUiTree(root));
}

UiRepaintOverlayModel buildUiRepaintOverlayModel(
    const UiInspectorSnapshot& snapshot)
{
    UiRepaintOverlayModel model;
    model.regions.reserve(snapshot.size());
    for (const UiInspectorEntry& entry : snapshot) {
        if (entry.dirtyFlags == toMask(DirtyFlag::None)
            || entry.bounds.width <= 0.0f
            || entry.bounds.height <= 0.0f) {
            continue;
        }

        model.regions.push_back(
            {entry.path, entry.bounds, entry.dirtyFlags});
        if (!model.unionBounds) {
            model.unionBounds = entry.bounds;
            continue;
        }

        const RectF previous = *model.unionBounds;
        const float left = std::min(previous.x, entry.bounds.x);
        const float top = std::min(previous.y, entry.bounds.y);
        const float right = std::max(
            previous.x + previous.width,
            entry.bounds.x + entry.bounds.width);
        const float bottom = std::max(
            previous.y + previous.height,
            entry.bounds.y + entry.bounds.height);
        model.unionBounds = RectF{
            left, top, right - left, bottom - top};
    }
    return model;
}

UiRepaintOverlayModel inspectUiRepaintOverlay(const Node& root)
{
    return buildUiRepaintOverlayModel(inspectUiTree(root));
}

} // namespace wui
