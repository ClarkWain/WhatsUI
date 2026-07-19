#include <iostream>
#include <stdexcept>

#include "wui/accessibility.h"
#include "wui/navigation.h"
#include "wui/ui.h"
#include "wui/widgets.h"

namespace {
void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void testToolbarAndLinkActivation()
{
    int invoked = 0;
    wui::Toolbar toolbar;
    toolbar.addItem("Cut").onInvoke([&] { ++invoked; });
    toolbar.addItem("Paste", wui::ToolbarItemAppearance::Primary).onInvoke([&] { invoked += 2; });
    toolbar.layout({0, 0, 240, 32});
    expect(toolbar.children().size() == 2, "Toolbar must retain one real control per item");
    auto* cut = dynamic_cast<wui::ToolbarItem*>(toolbar.children().front().get());
    expect(cut != nullptr, "Toolbar items must expose their own control contract");
    expect(cut->performAccessibilityAction(wui::AccessibilityActionKind::Invoke, {}) ==
               wui::AccessibilityActionStatus::Succeeded && invoked == 1,
           "Toolbar item must be invokable through accessibility without pointer synthesis");
    expect(toolbar.onKeyEvent({0, wui::KeyAction::Down, 39}) && toolbar.focusedIndex() == 1,
           "Toolbar arrow navigation must move roving focus between items");

    wui::Link link("Open docs");
    link.href("https://example.invalid").onInvoke([&] { ++invoked; });
    expect(link.performAccessibilityAction(wui::AccessibilityActionKind::Invoke, {}) ==
               wui::AccessibilityActionStatus::Succeeded && invoked == 2,
           "Link must expose a deterministic invoke action rather than navigating implicitly");
}

void testToolbarOrientationAndOverflow()
{
    wui::Toolbar toolbar;
    toolbar.addItem("New"); toolbar.addItem("Open"); toolbar.addItem("Save"); toolbar.addItem("Export");
    toolbar.layout({0, 0, 108, 32});
    expect(toolbar.overflowedItems().size() >= 1 && toolbar.overflowedItems().back() == "Export",
           "Toolbar must deterministically collapse trailing commands that do not fit");
    std::size_t overflowCount = 0;
    toolbar.onOverflow([&](const std::vector<std::string>& items) { overflowCount = items.size(); });
    expect(toolbar.onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Up, wui::MouseButton::Left, {96, 16}}) &&
               overflowCount == toolbar.overflowedItems().size(),
           "Toolbar overflow affordance must expose its hidden command list without implicit menus");
    toolbar.setOrientation(wui::ToolbarOrientation::Vertical);
    toolbar.layout({0, 0, 80, 64});
    expect(toolbar.measure({0, 100, 0, 400}).height > toolbar.measure({0, 400, 0, 100}).height,
           "Vertical Toolbar must stack command items on its main axis");
}

void testDeclarativeBuilders()
{
    auto toolbar = wui::ui::Toolbar().item("Save", wui::ToolbarItemAppearance::Primary)
                                     .accessibleLabel("Document actions");
    auto tabs = wui::ui::TabList().tab("files", "Files").tab("activity", "Activity").value("activity");
    auto link = wui::ui::Link("Open guide").href("https://example.invalid");
    auto breadcrumb = wui::ui::Breadcrumb().item("Home").item("Library", true).maxVisible(3);
    expect(toolbar.intoNode() != nullptr && tabs.intoNode() != nullptr && link.intoNode() != nullptr &&
               breadcrumb.intoNode() != nullptr,
           "Navigation controls must be available through the declarative builder API");
}

void testTabsKeyboardAndPanelIdentity()
{
    int changes = 0;
    wui::TabList tabs;
    tabs.accessibleLabel("Settings sections");
    tabs.addTab("general", "General");
    tabs.addTab("appearance", "Appearance", false);
    tabs.addTab("advanced", "Advanced");
    tabs.onChange([&](const std::string&) { ++changes; });
    tabs.layout({0, 0, 440, 40});
    expect(tabs.value() == "general", "First enabled Tab must become the initial selection");
    expect(tabs.onKeyEvent({0, wui::KeyAction::Down, 39}) && tabs.value() == "advanced" && changes == 1,
           "Automatic TabList arrows must skip disabled tabs while selecting the next enabled tab");
    expect(tabs.onKeyEvent({0, wui::KeyAction::Down, 35}) && tabs.value() == "advanced",
           "TabList End must select the final enabled tab");
    tabs.setActivationMode(wui::TabList::ActivationMode::Manual);
    expect(tabs.onKeyEvent({0, wui::KeyAction::Down, 37}) && tabs.value() == "advanced",
           "Manual TabList arrows must move focus without changing the active panel");
    expect(tabs.onKeyEvent({0, wui::KeyAction::Down, 13}) && tabs.value() == "general",
           "Manual TabList Enter must activate the focused tab");
    wui::TabPanel panel("advanced");
    panel.accessibleLabel("Advanced settings").tabList(tabs);
    expect(panel.value() == "advanced" && panel.accessibleLabel() == "Advanced settings" && !panel.isActive() &&
               panel.measure({0, 100, 0, 100}).height == 0.0f,
           "TabPanel must link value to TabList and remove inactive content from layout");
    tabs.setValue("advanced");
    expect(panel.isActive(), "TabPanel must become active when its linked tab value is selected");
}

void testBreadcrumbCollapseAndSemantics()
{
    int invoked = 0;
    wui::Breadcrumb breadcrumb;
    breadcrumb.maxVisible(3).accessibleLabel("Location");
    breadcrumb.addItem("Home").onInvoke([&] { ++invoked; });
    breadcrumb.addItem("Projects").onInvoke([&] { ++invoked; });
    breadcrumb.addItem("WhatsUI").onInvoke([&] { ++invoked; });
    breadcrumb.addItem("Design").current();
    breadcrumb.layout({0, 0, 360, 24});
    const auto hidden = breadcrumb.hiddenItems();
    expect(hidden.size() == 1 && hidden.front() == "Projects",
           "Breadcrumb must collapse middle destinations while retaining first and final context");
    auto* first = dynamic_cast<wui::BreadcrumbItem*>(breadcrumb.children().front().get());
    expect(first && first->performAccessibilityAction(wui::AccessibilityActionKind::Invoke, {}) ==
                        wui::AccessibilityActionStatus::Succeeded && invoked == 1,
           "Visible breadcrumb destinations must remain independently invokable");
}

void testNavigationAccessibilitySnapshot()
{
    auto root = std::make_unique<wui::Container>();
    auto tabs = std::make_unique<wui::TabList>();
    tabs->accessibleLabel("Demo tabs");
    tabs->addTab("one", "One");
    root->appendChild(std::move(tabs));
    root->appendChild(std::make_unique<wui::Link>("Privacy"));
    root->layout({0, 0, 400, 80});
    const auto snapshot = wui::snapshotAccessibilityTree(*root);
    bool sawTabList = false, sawTab = false, sawLink = false;
    for (const auto& entry : snapshot) {
        sawTabList = sawTabList || entry.properties.role == wui::AccessibilityRole::TabList;
        sawTab = sawTab || entry.properties.role == wui::AccessibilityRole::Tab;
        sawLink = sawLink || entry.properties.role == wui::AccessibilityRole::Link;
    }
    expect(sawTabList && sawTab && sawLink,
           "Navigation controls must project native semantic roles rather than generic text");
}
} // namespace

int main()
{
    try {
        testToolbarAndLinkActivation();
        testToolbarOrientationAndOverflow();
        testDeclarativeBuilders();
        testTabsKeyboardAndPanelIdentity();
        testBreadcrumbCollapseAndSemantics();
        testNavigationAccessibilitySnapshot();
        std::cout << "Fluent navigation tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fluent navigation test failure: " << error.what() << '\n';
        return 1;
    }
}
