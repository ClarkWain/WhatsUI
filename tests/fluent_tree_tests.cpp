#include <iostream>
#include <stdexcept>

#include "wui/tree.h"

namespace {
void expect(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

void populateTree(wui::Tree& tree)
{
    auto& files = tree.addItem("files", "Project files");
    files.addItem("readme", "README.md");
    auto& source = files.addItem("source", "Source");
    source.addItem("tree", "tree.cpp");
    auto& disabled = tree.addItem("disabled", "Unavailable");
    disabled.setEnabled(false);
    tree.addItem("settings", "Settings");
    tree.layout({0, 0, 300, 96});
}

void testStableIdentityAndDisclosure()
{
    wui::Tree tree; populateTree(tree);
    expect(tree.rowHeight() == 32.0f,
           "Medium Tree must use the Fluent 32-DIP row token");
    auto* files = tree.visibleItems().front();
    auto* readme = tree.visibleItems()[1];
    expect(files->isExpanded() && readme->id() == "readme", "expanded Tree must flatten nested stable items");
    expect(files->level() == 1 && readme->level() == 2 && tree.visibleItems()[3]->level() == 3,
           "TreeItem level must reflect retained hierarchy rather than visible-row position");
    files->setExpanded(false);
    expect(tree.visibleItems().size() == 3, "collapsed TreeItem must remove descendants from visible projection");
    expect(readme->level() == 2, "collapsed ancestors must not change retained TreeItem semantic level");
    files->setExpanded(true);
    expect(tree.visibleItems()[1] == readme, "expand/collapse must retain nested TreeItem identity");

    // The expand chevron owns the full official 24-DIP slot, not only the
    // visible 16-DIP glyph. Clicking near the slot edge must still toggle.
    tree.layout({0, 0, 300, 96});
    const wui::PointF disclosureEdge{23.0f, 16.0f};
    expect(files->onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Down,
                                  wui::MouseButton::Left, disclosureEdge}) &&
               files->onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Up,
                                      wui::MouseButton::Left, disclosureEdge}) &&
               !files->isExpanded(),
           "Tree disclosure must expose the full 24-DIP Fluent icon slot");
}

void testKeyboardAndDisabledSelection()
{
    wui::Tree tree; populateTree(tree);
    expect(tree.select("files") && tree.selectedId() == "files", "Tree must select a stable item id");
    expect(tree.onKeyEvent({0, wui::KeyAction::Down, 40}) && tree.selectedId() == "files", "Arrow Down must rove focus without changing selection");
    expect(tree.select("source"), "Tree must focus a stable branch before right-arrow navigation");
    expect(tree.onKeyEvent({0, wui::KeyAction::Down, 39}), "Right must navigate into an expanded branch");
    expect(tree.onKeyEvent({0, wui::KeyAction::Down, 13}) && tree.selectedId() == "tree", "Enter must select focused TreeItem");
    expect(tree.onKeyEvent({0, wui::KeyAction::Down, 35}), "End must find final enabled visible TreeItem");
    expect(tree.onKeyEvent({0, wui::KeyAction::Down, 32}) && tree.selectedId() == "settings", "Space must select focused TreeItem and skip disabled entries");
    expect(tree.onKeyEvent({0, wui::KeyAction::Down, 36}), "Home must move roving focus to first enabled item");
    expect(tree.onKeyEvent({0, wui::KeyAction::Down, 37}) && !tree.visibleItems().front()->isExpanded(), "Left must collapse expanded tree branch");
}

void testScrollAndAccessibilityActions()
{
    wui::Tree tree; populateTree(tree);
    tree.setMaxVisibleItems(2); tree.layout({0, 0, 280, 64});
    expect(tree.maximumScrollOffset() > 0.0f, "Tree with a viewport must expose scroll range");
    tree.select("tree");
    expect(tree.scrollOffset() > 0.0f, "selection below viewport must scroll TreeItem into view");
    auto* files = tree.visibleItems().front();
    expect(files->performAccessibilityAction(wui::AccessibilityActionKind::Collapse, {}) == wui::AccessibilityActionStatus::Succeeded,
           "TreeItem accessibility Collapse must use normal disclosure state");
    expect(!files->isExpanded() && files->performAccessibilityAction(wui::AccessibilityActionKind::Expand, {}) == wui::AccessibilityActionStatus::Succeeded,
           "TreeItem accessibility Expand must be deterministic");
}

void testLargeTreeLayoutsOnlyViewportRows()
{
    wui::Tree tree;
    for (int index = 0; index < 1000; ++index) tree.addItem("item-" + std::to_string(index), "Item " + std::to_string(index));
    tree.layout({0, 0, 320, 64});
    auto range = tree.visibleRange();
    expect(range.first == 0 && range.size() == 2,
           "Large Tree should expose a bounded viewport range");
    std::size_t laidOut = 0;
    for (auto* item : tree.visibleItems()) if (item->bounds().height > 0.0f) ++laidOut;
    expect(laidOut == 2, "Large Tree layout should assign real bounds only to viewport rows");

    tree.setScrollOffset(32.0f * 500.0f);
    tree.layout({0, 0, 320, 64});
    range = tree.visibleRange();
    laidOut = 0;
    for (auto* item : tree.visibleItems()) if (item->bounds().height > 0.0f) ++laidOut;
    expect(range.first == 500 && laidOut == 2,
           "Large Tree scroll should keep row layout bounded to the viewport");
}

    void testRemovingFocusedTreeItemDoesNotLeaveStalePointer()
    {
        wui::Tree tree;
        tree.addItem("first", "First");
        tree.addItem("second", "Second");
        tree.layout({0, 0, 320, 64});
        expect(tree.onKeyEvent({0, wui::KeyAction::Down, 40}),
            "Tree should establish roving focus on first key navigation");
        tree.removeChild(0).reset();
        tree.layout({0, 0, 320, 64});
        expect(tree.onKeyEvent({0, wui::KeyAction::Down, 40}),
            "Tree keyboard navigation must recover after the focused item is removed");
    }

        void testCollapsingFocusedDescendantMovesFocusToAncestor()
        {
            wui::Tree tree;
            populateTree(tree);
            auto* files = tree.visibleItems().front();
            auto* leaf = tree.visibleItems()[3];
            expect(leaf->performAccessibilityAction(wui::AccessibilityActionKind::SetFocus, {}) == wui::AccessibilityActionStatus::Succeeded,
                "TreeItem SetFocus should establish roving focus on a descendant");
            files->setExpanded(false);
            expect(tree.onKeyEvent({0, wui::KeyAction::Down, 32}) && tree.selectedId() == "files",
                "Collapsing an ancestor must move roving focus from hidden descendants back to the collapsed ancestor");
        }

        void testTreeItemChildMutationInvalidatesVisibleProjection()
        {
            wui::Tree tree;
            auto& branch = tree.addItem("branch", "Branch");
            branch.addItem("child", "Child");
            tree.layout({0, 0, 320, 64});
            expect(tree.visibleItems().size() == 2, "Tree should expose nested children before mutation");
            branch.clearChildren();
            expect(tree.visibleItems().size() == 1,
                "TreeItem clearChildren must invalidate the owning Tree visible projection");
        }
} // namespace

int main()
{
    try {
        testStableIdentityAndDisclosure(); testKeyboardAndDisabledSelection(); testScrollAndAccessibilityActions(); testLargeTreeLayoutsOnlyViewportRows(); testRemovingFocusedTreeItemDoesNotLeaveStalePointer(); testCollapsingFocusedDescendantMovesFocusToAncestor(); testTreeItemChildMutationInvalidatesVisibleProjection();
        std::cout << "Fluent Tree tests passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << "Fluent Tree test failure: " << error.what() << '\n'; return 1; }
}
