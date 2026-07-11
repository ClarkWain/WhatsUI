#pragma once

// Read-only Node-tree diagnostics. UiInspector creates value snapshots only:
// it does not require a window, renderer, event loop, or live debugging
// transport. RTTI type names are intentionally diagnostic strings rather than
// a stable serialization contract; embedders may layer explicit widget names
// on top when they need cross-toolchain persistence.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "wui/node.h"

namespace wui {

struct UiInspectorEntry {
    // Empty for the inspected root. Every following element is a child index
    // from its parent, making the pre-order traversal location stable while
    // the tree shape is unchanged.
    std::vector<std::size_t> path;
    std::size_t depth{0};
    std::string type;
    RectF bounds;
    DirtyFlags dirtyFlags{toMask(DirtyFlag::None)};
    std::optional<ControlVisualStates> visualStates;
    std::size_t childCount{0};
};

using UiInspectorSnapshot = std::vector<UiInspectorEntry>;

// Collects a stable pre-order snapshot of a Node subtree.
[[nodiscard]] UiInspectorSnapshot inspectUiTree(const Node& root);

struct UiHitPath {
    std::vector<std::size_t> path;
    std::string type;
    RectF bounds;
};

// Uses the tree's ordinary hit testing and then reports the matching stable
// path. A null result means the point did not hit this subtree.
[[nodiscard]] std::optional<UiHitPath> inspectUiHitPath(Node& root, PointF point);

struct UiDirtySummary {
    std::size_t nodeCount{0};
    std::size_t dirtyNodeCount{0};
    std::size_t styleDirtyCount{0};
    std::size_t layoutDirtyCount{0};
    std::size_t paintDirtyCount{0};
    std::size_t compositingDirtyCount{0};

    [[nodiscard]] bool needsRepaint() const noexcept
    {
        // Layout invalidation also invalidates paint in Node::markDirty().
        return styleDirtyCount != 0 || layoutDirtyCount != 0 || paintDirtyCount != 0 ||
               compositingDirtyCount != 0;
    }
};

[[nodiscard]] UiDirtySummary summarizeUiDirty(const UiInspectorSnapshot& snapshot) noexcept;
[[nodiscard]] UiDirtySummary inspectUiDirty(const Node& root);

} // namespace wui
