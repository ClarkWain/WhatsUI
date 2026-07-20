#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>

#include "wui/basic_controls.h"
#include "wui/theme.h"
#include "wui/ui_inspector.h"
#include "wui/widgets.h"

namespace {

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool sameColor(wui::Color left, wui::Color right)
{
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
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

void testMeasuredConstraintsAndResolvedStyle()
{
    wui::Column root;
    auto button = std::make_unique<wui::Button>("Save");
    button->setVariant(wui::ButtonVariant::Ghost);
    button->setVisualState(wui::ControlVisualState::Hovered, true);
    root.appendChild(std::move(button));
    root.layout({0.0f, 0.0f, 180.0f, 60.0f});

    const auto snapshot = wui::inspectUiTree(root);
    expect(snapshot.size() == 2 && snapshot[1].measuredConstraints,
           "Framework layout measurement must record an honest child constraint envelope");
    expect(snapshot[1].measuredConstraints->maxWidth == 180.0f,
           "Recorded child constraints must preserve the layout container width");
    expect(snapshot[1].resolvedStyle && snapshot[1].resolvedStyle->role == "Button",
           "Supported Fluent controls must expose a resolved style role");
    expect(snapshot[1].resolvedStyle->background && sameColor(*snapshot[1].resolvedStyle->background, wui::theme().colors.surfaceHover),
           "Resolved ghost button style must apply the live hovered theme token");
    expect(snapshot[1].resolvedStyle->border && sameColor(*snapshot[1].resolvedStyle->border, wui::theme().colors.border),
           "Resolved ghost button style must include its Fluent border token");
}

void testResolvedRadioStyleMatchesFluentComposition()
{
    wui::Column root;
    auto radio = std::make_unique<wui::Radio>("Selected", true);
    radio->setVisualState(wui::ControlVisualState::Hovered, true);
    root.appendChild(std::move(radio));
    root.layout({0.0f, 0.0f, 180.0f, 40.0f});

    const auto snapshot = wui::inspectUiTree(root);
    expect(snapshot.size() == 2 && snapshot[1].resolvedStyle
               && snapshot[1].resolvedStyle->role == "Radio",
           "Inspector must expose a resolved Radio style");
    const auto& style = *snapshot[1].resolvedStyle;
    expect(style.background && style.background->a == 0,
           "Resolved Radio background must remain transparent");
    expect(style.border
               && sameColor(*style.border, wui::theme().colors.compoundBrandStroke.hover),
           "Resolved checked Radio border must use the compound-brand hover token");
    expect(style.foreground
               && sameColor(*style.foreground, wui::theme().colors.neutralForeground2),
           "Resolved hovered Radio label must use the Fluent hover foreground token");
}

void testResolvedCheckboxStyleMatchesFluentComposition()
{
    wui::Column root;
    auto checked = std::make_unique<wui::Checkbox>("Checked", true);
    checked->setVisualState(wui::ControlVisualState::Pressed, true);
    auto mixed = std::make_unique<wui::Checkbox>("Mixed");
    mixed->setMixed();
    mixed->setVisualState(wui::ControlVisualState::Hovered, true);
    root.appendChild(std::move(checked));
    root.appendChild(std::move(mixed));
    root.layout({0.0f, 0.0f, 180.0f, 80.0f});

    const auto snapshot = wui::inspectUiTree(root);
    expect(snapshot.size() == 3 && snapshot[1].resolvedStyle
               && snapshot[2].resolvedStyle,
           "Inspector must expose checked and mixed Checkbox styles");
    const auto& checkedStyle = *snapshot[1].resolvedStyle;
    expect(checkedStyle.background
               && sameColor(*checkedStyle.background,
                            wui::theme().colors.compoundBrandBackground.pressed)
               && checkedStyle.border
               && sameColor(*checkedStyle.border,
                            wui::theme().colors.compoundBrandBackground.pressed),
           "Resolved checked Checkbox must use compound-brand pressed surface tokens");
    const auto& mixedStyle = *snapshot[2].resolvedStyle;
    expect(mixedStyle.background && mixedStyle.background->a == 0
               && mixedStyle.border
               && sameColor(*mixedStyle.border,
                            wui::theme().colors.compoundBrandStroke.hover),
           "Resolved mixed Checkbox must keep a transparent face and compound-brand border");
}

void testRepaintOverlayRetainsIndependentParentAndChildDirtyRegions()
{
    wui::UiInspectorSnapshot snapshot;
    snapshot.push_back({{}, 0, "Root", {0.0f, 0.0f, 100.0f, 50.0f}, std::nullopt,
                        wui::toMask(wui::DirtyFlag::Layout), std::nullopt, 1, std::nullopt});
    snapshot.push_back({{0}, 1, "Child", {10.0f, 10.0f, 20.0f, 20.0f}, std::nullopt,
                        wui::toMask(wui::DirtyFlag::Paint), std::nullopt, 0, std::nullopt});

    const auto overlay = wui::buildUiRepaintOverlayModel(snapshot);
    expect(overlay.regions.size() == 2 && overlay.regions[0].path.empty() &&
               overlay.regions[1].path == std::vector<std::size_t>{0},
           "A conservative repaint overlay must retain parent and child dirty regions independently");
    expect(overlay.unionBounds && overlay.unionBounds->x == 0.0f && overlay.unionBounds->y == 0.0f &&
               overlay.unionBounds->width == 100.0f && overlay.unionBounds->height == 50.0f,
           "Repaint overlay union must include every retained independent dirty region");
    expect((overlay.regions[0].dirtyFlags & wui::toMask(wui::DirtyFlag::Layout)) != 0 &&
               (overlay.regions[1].dirtyFlags & wui::toMask(wui::DirtyFlag::Paint)) != 0,
           "Repaint overlay must preserve each region's dirty reason");
}

void testRepaintOverlayScalesLinearlyWithLargeFlatSnapshot()
{
    // No timing assertion is used: the data shape itself makes a quadratic
    // descendant scan impractical while keeping this deterministic test about
    // output completeness, not the speed of the current machine.
    constexpr std::size_t kEntries = 10000;
    wui::UiInspectorSnapshot snapshot;
    snapshot.reserve(kEntries);
    for (std::size_t index = 0; index < kEntries; ++index) {
        snapshot.push_back({{index}, 1, "Leaf", {static_cast<float>(index), 0.0f, 1.0f, 1.0f},
                            std::nullopt, wui::toMask(wui::DirtyFlag::Paint), std::nullopt, 0, std::nullopt});
    }
    const auto overlay = wui::buildUiRepaintOverlayModel(snapshot);
    expect(overlay.regions.size() == kEntries && overlay.regions.front().path == std::vector<std::size_t>{0} &&
               overlay.regions.back().path == std::vector<std::size_t>{kEntries - 1},
           "Large flat snapshots must retain every dirty region in stable traversal order");
    expect(overlay.unionBounds && overlay.unionBounds->x == 0.0f && overlay.unionBounds->width == static_cast<float>(kEntries),
           "Large flat snapshot union must be accumulated in one pass");
}

} // namespace

int main()
{
    testPreorderAndControlMetadata();
    testHitPathAndDirtySummary();
    testMeasuredConstraintsAndResolvedStyle();
    testResolvedRadioStyleMatchesFluentComposition();
    testResolvedCheckboxStyleMatchesFluentComposition();
    testRepaintOverlayRetainsIndependentParentAndChildDirtyRegions();
    testRepaintOverlayScalesLinearlyWithLargeFlatSnapshot();
    return 0;
}
