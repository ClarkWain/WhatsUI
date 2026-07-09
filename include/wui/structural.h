#pragma once

// Structural controls (ADR-002 §动态结构): mount/unmount subtrees in response
// to state, without a whole-tree diff. `IfNode` mounts an optional child;
// `ForEachNode` stacks a generated, rebuildable list of children.

#include <functional>
#include <memory>

#include "wui/node.h"
#include "wui/widgets.h"

namespace wui {

// Mounts a single optional child produced by a factory when visible.
class IfNode : public ContainerNode {
public:
    using Factory = std::function<std::unique_ptr<Node>()>;

    void setFactory(Factory factory);
    void setVisible(bool visible);
    [[nodiscard]] bool visible() const noexcept { return visible_; }

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;

private:
    void reconcile();

    Factory factory_;
    bool visible_{false};
};

// A vertically-stacked, rebuildable list container. Children are (re)generated
// by the ForEach builder; layout reuses Column semantics.
class ForEachNode : public Column {
};

} // namespace wui
