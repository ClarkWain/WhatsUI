#include <stdexcept>
#include <string>
#include <vector>
#include <memory>

#include "wui/accessibility.h"
#include "wui/text_input.h"
#include "wui/widgets.h"

namespace {

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testControlSemantics()
{
    wui::AccessibilityNode todoToggle{wui::AccessibilityRole::CheckBox};
    todoToggle.setLabel("Buy groceries")
        .setDescription("Due today")
        .setEnabled(true)
        .setChecked(false);

    const auto& semantics = todoToggle.properties();
    expect(semantics.role == wui::AccessibilityRole::CheckBox,
           "A semantic control must retain its role");
    expect(semantics.hasAccessibleName() && semantics.label == "Buy groceries",
           "A semantic control must retain its accessible label");
    expect(semantics.description == "Due today" && semantics.enabled,
           "Description and enabled state must be modeled independently");
    expect(semantics.checked && !*semantics.checked && semantics.supportsCheckedState(),
           "Checkbox state must preserve an explicit unchecked value");

    todoToggle.setChecked(true).clearChecked().setValue(std::string{"not applicable"}).clearValue();
    expect(!todoToggle.properties().checked && !todoToggle.properties().value,
           "Optional checked and value semantics must be clearable");
}

void testSnapshotTraversal()
{
    wui::AccessibilityNode root{wui::AccessibilityRole::Application};
    root.setLabel("WhatsUI Todo");

    wui::AccessibilityNode& list = root.addChild(wui::AccessibilityNode{wui::AccessibilityRole::List});
    list.setLabel("Tasks");
    list.addChild(wui::AccessibilityNode{wui::AccessibilityRole::ListItem})
        .setLabel("Ship M3")
        .setChecked(false);

    root.addChild(wui::AccessibilityNode{wui::AccessibilityRole::Button})
        .setLabel("Add task")
        .setEnabled(false);

    const auto snapshot = wui::snapshotAccessibilityTree(root);
    expect(snapshot.size() == 4, "Snapshot must include every semantic node exactly once");
    expect(snapshot[0].depth == 0 && snapshot[0].path.empty(),
           "Root snapshot entry must have depth zero and an empty path");
    expect(snapshot[1].path == std::vector<std::size_t>{0} && snapshot[1].properties.label == "Tasks",
           "Traversal must visit the first child before descendants");
    expect(snapshot[2].path == std::vector<std::size_t>({0, 0}) &&
               snapshot[2].properties.role == wui::AccessibilityRole::ListItem,
           "Traversal paths must identify nested children deterministically");
    expect(snapshot[3].path == std::vector<std::size_t>{1} && !snapshot[3].properties.enabled,
           "Sibling state must remain intact in a snapshot");

    const auto* item = wui::findAccessibilitySnapshotEntry(snapshot, {0, 0});
    expect(item != nullptr && item->properties.label == "Ship M3",
           "Path lookup must find a stable semantic snapshot entry");
    expect(wui::findAccessibilitySnapshotEntry(snapshot, {7}) == nullptr,
           "Path lookup must return null for a missing semantic entry");
}

void testVisualControlSnapshot()
{
    auto root = std::make_unique<wui::Column>();
    auto task = std::make_unique<wui::Checkbox>("Buy groceries", false);
    auto* taskRaw = task.get();
    auto input = std::make_unique<wui::TextInput>("Add a task");
    input->text("Milk");
    auto action = std::make_unique<wui::Button>("Add");
    action->setEnabled(false);
    root->child(std::move(task));
    root->child(std::move(input));
    root->child(std::move(action));
    root->layout({0.0f, 0.0f, 320.0f, 160.0f});

    const auto snapshot = wui::snapshotAccessibilityTree(*root, taskRaw);
    expect(snapshot.size() == 3, "Visual snapshot must omit decorative layout nodes");
    expect(snapshot[0].path == std::vector<std::size_t>{0} &&
               snapshot[0].properties.role == wui::AccessibilityRole::CheckBox &&
               snapshot[0].properties.label == "Buy groceries" &&
               snapshot[0].properties.checked && !*snapshot[0].properties.checked &&
               snapshot[0].properties.focused && snapshot[0].properties.bounds.has_value(),
           "Checkbox snapshot must expose current role, name, checked, focus and bounds");
    expect(snapshot[1].properties.role == wui::AccessibilityRole::TextField &&
               snapshot[1].properties.label == "Add a task" &&
               snapshot[1].properties.value && *snapshot[1].properties.value == "Milk",
           "Text fields must expose their placeholder name and current value");
    expect(snapshot[2].properties.role == wui::AccessibilityRole::Button &&
               snapshot[2].properties.label == "Add" && !snapshot[2].properties.enabled,
           "Buttons must expose their name and enabled state");
}

} // namespace

int main()
{
    testControlSemantics();
    testSnapshotTraversal();
    testVisualControlSnapshot();
    return 0;
}
