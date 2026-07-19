#include "wui/accessibility.h"
#include "wui/runtime.h"
#include "wui/selection.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void expect(bool condition, const std::string& message)
{
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

wui::KeyEvent key(int code) { return {0, wui::KeyAction::Down, code}; }

void listBoxSelectionAndKeyboard()
{
    wui::ListBox list({{"a", "Alpha"}, {"b", "Beta", false}, {"c", "Charlie"}});
    list.layout({0, 0, 240, 180});
    int changes = 0;
    list.onSelectionChanged([&](int index, const wui::Option&) { changes += index + 1; });
    expect(list.onKeyEvent(key(40)) && list.activeIndex() == 2, "Down must skip disabled ListBox options");
    expect(list.onKeyEvent(key(13)) && list.selectedIndex() == 2, "Enter must select active option");
    expect(changes == 3, "ListBox must notify selection source index");
    expect(list.performAccessibilityAction(wui::AccessibilityActionKind::SetValue, "a") == wui::AccessibilityActionStatus::Succeeded && list.selectedIndex() == 0,
           "ListBox SetValue must select matching value");
    wui::PointerEvent down{0, wui::PointerType::Mouse, wui::PointerAction::Down, wui::MouseButton::Left, {32, 78}};
    wui::PointerEvent up{0, wui::PointerType::Mouse, wui::PointerAction::Up, wui::MouseButton::Left, {32, 78}};
    expect(list.onPointerEvent(down) && list.onPointerEvent(up) && list.selectedIndex() == 2,
           "ListBox pointer activation must select the hit option");

    list.setSelectionMode(wui::ListBoxSelectionMode::Multiple);
    list.setActiveIndex(0);
    expect(list.onKeyEvent(key(32)) && list.selectedIndices().size() == 2, "Multiple ListBox Space must toggle active item");
}

void listBoxTypeAheadWindowingAndOptionSemantics()
{
    std::vector<wui::Option> options;
    for (int index = 0; index < 18; ++index) options.emplace_back("v" + std::to_string(index), index == 14 ? "Zulu" : "Alpha " + std::to_string(index));
    wui::ListBox list(std::move(options));
    list.setMaxVisibleOptions(3);
    list.layout({0, 0, 240, 104});
    expect(list.maximumScrollOffset() > 0.0f, "ListBox must expose a nonzero scroll range for windowed rows");
    expect(list.onKeyEvent(key('z')) && list.activeIndex() == 14 && list.scrollOffset() > 0.0f,
           "ListBox type-ahead must activate an offscreen matching option and scroll it into view");
    const auto semantics = list.accessibilityOptions();
    expect(semantics.size() == 18 && semantics[14].properties.role == wui::AccessibilityRole::Option &&
               semantics[14].properties.value == "v14" && semantics[14].properties.bounds.has_value(),
           "ListBox must materialize per-option accessibility semantics for the visible window");
    auto root = std::make_unique<wui::Container>();
    auto snapshotList = std::make_unique<wui::ListBox>(list.options());
    snapshotList->setMaxVisibleOptions(3);
    snapshotList->layout({0, 0, 240, 104});
    root->appendChild(std::move(snapshotList));
    const auto snapshot = wui::snapshotAccessibilityTree(*root);
    bool sawListBox = false;
    bool sawVisibleOption = false;
    for (const auto& entry : snapshot) {
        sawListBox = sawListBox || entry.properties.role == wui::AccessibilityRole::ListBox;
        sawVisibleOption = sawVisibleOption ||
            (entry.properties.role == wui::AccessibilityRole::Option &&
             entry.properties.label == "Alpha 0" && entry.properties.bounds.has_value() &&
             entry.properties.actions.invoke);
    }
    expect(sawListBox && sawVisibleOption,
           "ListBox snapshot must expose visible virtual options as invokable Option children");
    const float before = list.scrollOffset();
    wui::PointerEvent wheel{0, wui::PointerType::Mouse, wui::PointerAction::Scroll, wui::MouseButton::None, {20, 40}, 0, {0, 18}};
    expect(list.onPointerEvent(wheel) && list.scrollOffset() <= before,
           "ListBox wheel input must scroll the real viewport rather than all rows at once");
}

void overlayComboboxAndDropdown()
{
    wui::OverlayHost host;
    wui::FocusManager focus;
    host.bindFocusManager(focus);
    wui::Combobox combo("Search fruit");
    combo.bindOverlayHost(host).addOption({"apple", "Apple"}).addOption({"apricot", "Apricot"}).addOption({"banana", "Banana"});
    combo.layout({20, 20, 220, 32});
    expect(combo.performAccessibilityAction(wui::AccessibilityActionKind::Expand, {}) == wui::AccessibilityActionStatus::Succeeded && combo.isOpen() && host.size() == 1,
           "Combobox Expand must show a filtered ListBox overlay");
    expect(combo.performAccessibilityAction(wui::AccessibilityActionKind::Collapse, {}) == wui::AccessibilityActionStatus::Succeeded && !combo.isOpen() && host.empty() && focus.focused() == &combo,
           "Combobox Collapse must dismiss its overlay");
    combo.setSelectedIndex(2);
    expect(combo.controller().text() == "Banana", "Combobox commit must update editable text");
    combo.setMultiselect(true);
    combo.setSelectedIndices({0, 2});
    expect(combo.isMultiselect() && combo.selectedIndices().size() == 2 && combo.controller().text().empty(),
           "Multiselect Combobox must retain multiple values without substituting a misleading single label");

    wui::Dropdown dropdown("Pick fruit");
    dropdown.bindOverlayHost(host).addOption({"apple", "Apple"}).addOption({"banana", "Banana"});
    dropdown.layout({20, 80, 220, 32});
    expect(dropdown.performAccessibilityAction(wui::AccessibilityActionKind::Expand, {}) == wui::AccessibilityActionStatus::Succeeded && dropdown.isOpen(),
           "Dropdown Expand must show ListBox overlay");
    expect(dropdown.performAccessibilityAction(wui::AccessibilityActionKind::SetValue, "banana") == wui::AccessibilityActionStatus::Succeeded && dropdown.value() == "banana",
           "Dropdown SetValue must use stable option value");
    expect(dropdown.performAccessibilityAction(wui::AccessibilityActionKind::Collapse, {}) == wui::AccessibilityActionStatus::Succeeded && !dropdown.isOpen() && focus.focused() == &dropdown,
           "Dropdown Collapse must restore closed state");
    dropdown.setMultiselect(true);
    dropdown.setSelectedIndices({0, 1});
    expect(dropdown.isMultiselect() && dropdown.selectedIndices().size() == 2,
           "Dropdown must expose the Fluent multi-select option policy");
    wui::PointerEvent down{0, wui::PointerType::Mouse, wui::PointerAction::Down, wui::MouseButton::Left, {40, 96}};
    wui::PointerEvent up{0, wui::PointerType::Mouse, wui::PointerAction::Up, wui::MouseButton::Left, {40, 96}};
    expect(dropdown.onPointerEvent(down) && dropdown.onPointerEvent(up) && dropdown.isOpen(),
           "Dropdown pointer release must open its overlay");
    dropdown.performAccessibilityAction(wui::AccessibilityActionKind::Collapse, {});
}

} // namespace

int main()
{
    listBoxSelectionAndKeyboard();
    listBoxTypeAheadWindowingAndOptionSemantics();
    overlayComboboxAndDropdown();
    std::cout << "Fluent selection controls passed\n";
}
