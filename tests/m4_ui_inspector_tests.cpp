#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>

#include "wui/ui_inspector.h"

namespace {

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class InspectLeaf final : public wui::Node {
public:
    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        return constraints.clamp({20.0f, 10.0f});
    }

    void paint(wui::PaintContext&) override
    {
        clearDirty(wui::DirtyFlag::Paint);
    }
};

class InspectControl final : public wui::ControlNode {
public:
    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        return constraints.clamp({20.0f, 10.0f});
    }

    void paint(wui::PaintContext&) override
    {
        clearDirty(wui::DirtyFlag::Paint);
    }
};

class InspectGroup final : public wui::ContainerNode {
public:
    [[nodiscard]] wui::SizeF measure(const wui::Constraints& constraints) const override
    {
        return constraints.clamp({100.0f, 50.0f});
    }

    void layout(const wui::RectF& bounds) override
    {
        wui::Node::layout(bounds);
        if (!children().empty()) {
            children()[0]->layout({bounds.x, bounds.y, 40.0f, bounds.height});
        }
        if (children().size() > 1) {
            children()[1]->layout({bounds.x + 50.0f, bounds.y, 40.0f, bounds.height});
        }
        clearLayoutDirtyRecursively();
    }
};

void testPreorderAndControlMetadata()
{
    InspectGroup root;
    auto leaf = std::make_unique<InspectLeaf>();
    auto control = std::make_unique<InspectControl>();
    control->setVisualState(wui::ControlVisualState::Hovered, true);
    control->setVisualState(wui::ControlVisualState::Focused, true);
    root.appendChild(std::move(leaf));
    root.appendChild(std::move(control));
    root.layout({5.0f, 7.0f, 100.0f, 50.0f});

    const auto snapshot = wui::inspectUiTree(root);
    expect(snapshot.size() == 3, "Inspector must snapshot each node once in pre-order");
    expect(snapshot[0].depth == 0 && snapshot[0].path.empty() && snapshot[0].childCount == 2,
           "Root entry must retain its stable tree metadata");
    expect(snapshot[1].path == std::vector<std::size_t>{0} && snapshot[1].bounds.x == 5.0f,
           "First child must report its pre-order path and final rect");
    expect(snapshot[2].path == std::vector<std::size_t>{1} && snapshot[2].visualStates &&
               (*snapshot[2].visualStates & wui::toMask(wui::ControlVisualState::Hovered)) != 0 &&
               (*snapshot[2].visualStates & wui::toMask(wui::ControlVisualState::Focused)) != 0,
           "Control entries must expose their current visual-state mask");
    expect(snapshot[2].type == typeid(InspectControl).name(),
           "Inspector type must identify the dynamic node type via RTTI");
}

void testHitPathAndDirtySummary()
{
    InspectGroup root;
    root.appendChild(std::make_unique<InspectLeaf>());
    root.appendChild(std::make_unique<InspectControl>());
    root.layout({0.0f, 0.0f, 100.0f, 50.0f});

    const auto hit = wui::inspectUiHitPath(root, {55.0f, 25.0f});
    expect(hit && hit->path == std::vector<std::size_t>{1} && hit->bounds.x == 50.0f,
           "Hit inspection must return the final hit node and its stable path");
    expect(!wui::inspectUiHitPath(root, {120.0f, 25.0f}),
           "Misses must not produce a hit-path diagnostic");

    root.children()[1]->markDirty(wui::DirtyFlag::Style);
    root.children()[0]->markDirty(wui::DirtyFlag::Compositing);
    const auto summary = wui::inspectUiDirty(root);
    expect(summary.nodeCount == 3 && summary.dirtyNodeCount == 3,
           "Dirty summary must include propagated root invalidation and dirty descendants");
    expect(summary.styleDirtyCount == 2 && summary.compositingDirtyCount == 2 && summary.needsRepaint(),
           "Dirty summary must count each pending flag and report repaint work");
}

} // namespace

int main()
{
    testPreorderAndControlMetadata();
    testHitPathAndDirtySummary();
    return 0;
}
