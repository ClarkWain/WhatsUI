#include <iostream>
#include <memory>
#include <stdexcept>

#include "wui/accessibility.h"
#include "wui/accordion.h"
#include "wui/widgets.h"

namespace {
void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void testSingleAndMultipleExpansion()
{
    wui::Accordion accordion;
    int changes = 0;
    auto& first = accordion.addItem("Account", "Update your profile.");
    auto& second = accordion.addItem("Appearance", "Choose a theme.");
    accordion.onExpandedChange([&](std::size_t, bool) { ++changes; });
    first.setExpanded(true);
    expect(first.isExpanded() && !second.isExpanded(), "Accordion must expand its selected item");
    second.setExpanded(true);
    expect(!first.isExpanded() && second.isExpanded() && changes == 3,
           "Single Accordion must close the previous item before expanding the next item");

    accordion.setExpandMode(wui::AccordionExpandMode::Multiple);
    first.setExpanded(true);
    expect(first.isExpanded() && second.isExpanded() && accordion.expandedIndices().size() == 2,
           "Multiple Accordion must retain independently expanded disclosures");
}

void testInputAndFocus()
{
    wui::Accordion accordion;
    auto& first = accordion.addItem("First", "Body");
    auto& disabled = accordion.addItem("Unavailable", "Disabled body");
    auto& third = accordion.addItem("Third", "Body");
    disabled.setEnabled(false);
    accordion.layout({0, 0, 360, 220});
    expect(first.bounds().height == 44.0f,
           "Medium Accordion headers must use the Fluent 44-DIP container");
    expect(first.onKeyEvent({0, wui::KeyAction::Down, 32}) && first.isExpanded(),
           "Space on a focused AccordionItem must toggle its disclosure");
    expect(first.onKeyEvent({0, wui::KeyAction::Down, 40}) && accordion.focusedIndex() == 2,
           "Down Arrow must move roving focus and skip disabled AccordionItems");
    expect(accordion.onKeyEvent({0, wui::KeyAction::Down, 36}) && accordion.focusedIndex() == 0,
           "Home must move Accordion roving focus to its first enabled item");
    expect(accordion.onKeyEvent({0, wui::KeyAction::Down, 35}) && accordion.focusedIndex() == 2,
           "End must move Accordion roving focus to its final enabled item");

    bool toggled = false;
    third.onExpandedChange([&](bool) { toggled = true; });
    expect(third.performAccessibilityAction(wui::AccessibilityActionKind::Expand, {}) ==
               wui::AccessibilityActionStatus::Succeeded && third.isExpanded() && toggled,
           "Accessibility Expand must use the same Accordion policy and callback as interactive input");
    expect(third.performAccessibilityAction(wui::AccessibilityActionKind::Collapse, {}) ==
               wui::AccessibilityActionStatus::Succeeded && !third.isExpanded(),
           "Accessibility Collapse must deterministically close an expanded item");
}

void testRetainedBodyContent()
{
    wui::Accordion accordion;
    auto& item = accordion.addItem("Advanced", "");
    auto retained = std::make_unique<wui::Spacer>(wui::SizeF{12, 36});
    auto* identity = retained.get();
    item.setContent(std::move(retained));
    item.setExpanded(true);
    accordion.layout({0, 0, 320, 180});
    expect(item.content() == identity && item.content()->bounds().height > 0.0f,
           "Expanded AccordionItem must retain and lay out its body subtree");
    item.setExpanded(false);
    accordion.layout({0, 0, 320, 180});
    expect(item.content() == identity && item.content()->bounds().height == 0.0f,
           "Collapsed AccordionItem must retain body identity while removing it from layout");
}

void testCollapsedBodyIsAbsentFromAccessibilityTree()
{
    wui::Accordion accordion;
    auto& item = accordion.addItem("Advanced", "");
    item.setContent(std::make_unique<wui::Text>("Hidden diagnostics"));
    item.setExpanded(false);
    const auto collapsed = wui::snapshotAccessibilityTree(accordion);
    bool sawHiddenText = false;
    for (const auto& entry : collapsed) {
        sawHiddenText = sawHiddenText || entry.properties.label == "Hidden diagnostics";
    }
    expect(!sawHiddenText,
           "Collapsed Accordion bodies must be absent from the accessibility tree");
    item.setExpanded(true);
    const auto expanded = wui::snapshotAccessibilityTree(accordion);
    bool sawExpandedText = false;
    for (const auto& entry : expanded) {
        sawExpandedText = sawExpandedText || entry.properties.label == "Hidden diagnostics";
    }
    expect(sawExpandedText,
           "Expanded Accordion bodies must return to the accessibility tree");
}
} // namespace

int main()
{
    try {
        testSingleAndMultipleExpansion();
        testInputAndFocus();
        testRetainedBodyContent();
        testCollapsedBodyIsAbsentFromAccessibilityTree();
        std::cout << "Fluent accordion tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fluent accordion test failure: " << error.what() << '\n';
        return 1;
    }
}
