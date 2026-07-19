#include <cstdlib>
#include <iostream>
#include <string>

#include "wui/accessibility.h"
#include "wui/overlays.h"
#include "wui/runtime.h"

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

const wui::AccessibilityProperties& onlySemanticNode(const wui::Node& node)
{
    static wui::AccessibilityProperties properties;
    const auto snapshot = wui::snapshotAccessibilityTree(node);
    expect(snapshot.size() == 1, "fixture must project exactly one semantic node");
    properties = snapshot.front().properties;
    return properties;
}

void testMenuButtonMovesAndRestoresFocus()
{
    wui::FocusManager focus;
    wui::OverlayHost overlays;
    overlays.bindFocusManager(focus);

    wui::MenuButton button("More");
    button.bindOverlayHost(overlays)
        .addItem({"Unavailable", {}, false, {}})
        .addItem({"Rename", {}, true, {}});
    button.layout({0.0f, 0.0f, 96.0f, 32.0f});
    focus.setFocused(&button);

    const auto collapsed = onlySemanticNode(button);
    expect(collapsed.role == wui::AccessibilityRole::Button && collapsed.expanded == false,
           "collapsed MenuButton must expose a collapsed Button semantic");
    expect(collapsed.actions.expandCollapse && !collapsed.actions.invoke,
           "MenuButton must expose ExpandCollapse instead of a synthetic Invoke action");

    expect(button.performAccessibilityAction(wui::AccessibilityActionKind::Expand, {})
               == wui::AccessibilityActionStatus::Succeeded,
           "accessible Expand must open MenuButton");
    expect(button.isOpen() && overlays.size() == 1,
           "Expand must create one host-owned menu");
    auto* menu = dynamic_cast<wui::Menu*>(overlays.top()->content.get());
    expect(menu != nullptr && focus.focused() == menu,
           "opening MenuButton must move keyboard focus into its Menu");
    const auto menuSnapshot = wui::snapshotAccessibilityTree(*menu, menu);
    expect(menuSnapshot.size() == 1
               && menuSnapshot.front().properties.role == wui::AccessibilityRole::Menu
               && menuSnapshot.front().properties.focused,
           "the focused overlay must project a focused Menu semantic for native UIA");
    expect(menu->selectedIndex() == 1,
           "menu focus must start on the first enabled item");
    expect((button.visualStates() & wui::toMask(wui::ControlVisualState::Pressed)) != 0,
           "expanded MenuButton must retain its pressed surface state for paint");
    expect(onlySemanticNode(button).expanded == true,
           "expanded MenuButton semantic must update in the same frame");

    expect(menu->onKeyEvent({0, wui::KeyAction::Down, 27}),
           "Escape must dismiss the focused Menu");
    expect(!button.isOpen() && overlays.empty() && focus.focused() == &button,
           "menu dismissal must restore focus to its MenuButton owner");
    expect((button.visualStates() & wui::toMask(wui::ControlVisualState::Pressed)) == 0,
           "collapsed MenuButton must release its pressed surface state");
    expect(onlySemanticNode(button).expanded == false,
           "MenuButton semantic must return to collapsed after dismissal");
}

void testSplitButtonKeepsInvokeAndDisclosureIndependent()
{
    wui::FocusManager focus;
    wui::OverlayHost overlays;
    overlays.bindFocusManager(focus);
    int primaryInvocations = 0;
    int menuInvocations = 0;

    wui::SplitButton button("Save");
    button.bindOverlayHost(overlays)
        .onClick([&] { ++primaryInvocations; })
        .addItem({"Save as", {}, true, [&] { ++menuInvocations; }});
    button.layout({0.0f, 0.0f, 112.0f, 32.0f});
    focus.setFocused(&button);

    const auto collapsed = onlySemanticNode(button);
    expect(collapsed.actions.invoke && collapsed.actions.expandCollapse
               && collapsed.expanded == false,
           "SplitButton must independently expose Invoke and ExpandCollapse");
    expect(button.performAccessibilityAction(wui::AccessibilityActionKind::Invoke, {})
               == wui::AccessibilityActionStatus::Succeeded
               && primaryInvocations == 1 && overlays.empty(),
           "SplitButton accessible Invoke must execute only its primary command");
    expect(button.performAccessibilityAction(wui::AccessibilityActionKind::Expand, {})
               == wui::AccessibilityActionStatus::Succeeded,
           "SplitButton accessible Expand must operate only its disclosure");
    auto* menu = overlays.top() != nullptr
        ? dynamic_cast<wui::Menu*>(overlays.top()->content.get()) : nullptr;
    expect(menu != nullptr && focus.focused() == menu && onlySemanticNode(button).expanded == true,
           "SplitButton disclosure must focus and publish its expanded menu");
    expect(menu->onKeyEvent({0, wui::KeyAction::Down, 13}),
           "Enter on the focused menu item must invoke it");
    expect(menuInvocations == 1 && primaryInvocations == 1,
           "menu item invocation must never run the SplitButton primary command");
    expect(overlays.empty() && focus.focused() == &button
               && onlySemanticNode(button).expanded == false,
           "menu item invocation must collapse and restore SplitButton focus");

    expect(button.performAccessibilityAction(wui::AccessibilityActionKind::Collapse, {})
               == wui::AccessibilityActionStatus::Succeeded,
           "Collapse must be idempotent for an already-collapsed SplitButton");
}

} // namespace

int main()
{
    testMenuButtonMovesAndRestoresFocus();
    testSplitButtonKeepsInvokeAndDisclosureIndependent();
    return 0;
}
