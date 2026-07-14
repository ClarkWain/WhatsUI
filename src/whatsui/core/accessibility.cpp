#include "wui/accessibility.h"

#include "wui/basic_controls.h"
#include "wui/node.h"
#include "wui/overlays.h"
#include "wui/text_input.h"
#include "wui/widgets.h"

#include <string>

namespace wui {
namespace {

AccessibilityProperties propertiesForNode(const Node& node, const Node* focused)
{
    AccessibilityProperties properties;
    if (const auto* button = dynamic_cast<const Button*>(&node)) {
        properties.role = AccessibilityRole::Button;
        properties.label = button->label();
    } else if (const auto* iconButton = dynamic_cast<const IconButton*>(&node)) {
        properties.role = AccessibilityRole::Button;
        properties.label = iconButton->accessibleLabel();
    } else if (const auto* checkbox = dynamic_cast<const Checkbox*>(&node)) {
        properties.role = AccessibilityRole::CheckBox;
        properties.label = checkbox->accessibleLabel().empty()
            ? checkbox->label()
            : checkbox->accessibleLabel();
        properties.checked = checkbox->isChecked();
    } else if (const auto* radio = dynamic_cast<const Radio*>(&node)) {
        properties.role = AccessibilityRole::RadioButton;
        properties.label = radio->label();
        properties.checked = radio->isSelected();
    } else if (const auto* toggle = dynamic_cast<const Switch*>(&node)) {
        properties.role = AccessibilityRole::Switch;
        properties.label = toggle->label();
        properties.checked = toggle->isOn();
    } else if (const auto* slider = dynamic_cast<const Slider*>(&node)) {
        properties.role = AccessibilityRole::Slider;
        properties.value = std::to_string(slider->value());
    } else if (const auto* progress = dynamic_cast<const ProgressBar*>(&node)) {
        properties.role = AccessibilityRole::ProgressBar;
        properties.value = std::to_string(progress->value());
    } else if (const auto* input = dynamic_cast<const TextInput*>(&node)) {
        properties.role = AccessibilityRole::TextField;
        properties.label = input->placeholder();
        properties.value = input->controller().text();
    } else if (const auto* text = dynamic_cast<const Text*>(&node)) {
        properties.role = AccessibilityRole::Text;
        properties.label = text->value();
    } else if (dynamic_cast<const Dialog*>(&node) != nullptr) {
        properties.role = AccessibilityRole::Dialog;
    } else if (dynamic_cast<const Image*>(&node) != nullptr) {
        properties.role = AccessibilityRole::Image;
    } else if (dynamic_cast<const Divider*>(&node) != nullptr) {
        properties.role = AccessibilityRole::Separator;
    } else {
        return properties;
    }

    if (const auto* control = dynamic_cast<const ControlNode*>(&node)) {
        properties.enabled = control->isEnabled();
    }
    properties.focused = &node == focused;
    properties.bounds = node.bounds();
    return properties;
}

void appendNodeSnapshot(const Node& node,
                        const Node* focused,
                        std::vector<std::size_t>& path,
                        AccessibilitySnapshot& snapshot)
{
    auto properties = propertiesForNode(node, focused);
    if (properties.role != AccessibilityRole::Unknown) {
        snapshot.push_back({path, path.size(), std::move(properties)});
    }

    const auto& children = node.children();
    for (std::size_t index = 0; index < children.size(); ++index) {
        if (!children[index]) {
            continue;
        }
        path.push_back(index);
        appendNodeSnapshot(*children[index], focused, path, snapshot);
        path.pop_back();
    }
}

} // namespace

AccessibilitySnapshot snapshotAccessibilityTree(const Node& root, const Node* focused)
{
    AccessibilitySnapshot snapshot;
    std::vector<std::size_t> path;
    appendNodeSnapshot(root, focused, path, snapshot);
    return snapshot;
}

} // namespace wui
