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
} // namespace

int main()
{
    try {
        testStableIdentityAndDisclosure(); testKeyboardAndDisabledSelection(); testScrollAndAccessibilityActions();
        std::cout << "Fluent Tree tests passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << "Fluent Tree test failure: " << error.what() << '\n'; return 1; }
}
