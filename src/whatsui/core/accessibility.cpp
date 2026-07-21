#include "wui/accessibility.h"

#include "wui/accordion.h"
#include "wui/avatar.h"
#include "wui/basic_controls.h"
#include "wui/feedback.h"
#include "wui/form_feedback.h"
#include "wui/drawer.h"
#include "wui/badge.h"
#include "wui/date_time.h"
#include "wui/node.h"
#include "wui/navigation.h"
#include "wui/overlays.h"
#include "wui/popover.h"
#include "wui/persona.h"
#include "wui/rating.h"
#include "wui/selection.h"
#include "wui/table.h"
#include "wui/tree.h"
#include "wui/text_input.h"
#include "wui/widgets.h"

#include <algorithm>
#include <string>

namespace wui {
namespace {

AccessibilityProperties propertiesForNode(const Node& node, const Node* focused)
{
    AccessibilityProperties properties;
    if (const auto* popoverButton = dynamic_cast<const PopoverButton*>(&node)) {
        properties.role = AccessibilityRole::Button;
        properties.label = popoverButton->label();
        properties.expanded = popoverButton->isOpen();
    } else if (const auto* accordion = dynamic_cast<const Accordion*>(&node)) {
        properties.role = AccessibilityRole::Group;
        properties.label = accordion->accessibleLabel();
    } else if (const auto* accordionItem = dynamic_cast<const AccordionItem*>(&node)) {
        properties.role = AccessibilityRole::Button;
        properties.label = accordionItem->header();
        properties.expanded = accordionItem->isExpanded();
    } else if (const auto* drawer = dynamic_cast<const Drawer*>(&node)) {
        properties.role = AccessibilityRole::Dialog;
        properties.label = drawer->title();
        properties.description = drawer->subtitle();
        properties.actions.focus = true;
    } else if (const auto* persona = dynamic_cast<const Persona*>(&node)) {
        properties.role = persona->isInteractive() ? AccessibilityRole::Button : AccessibilityRole::Group;
        properties.label = persona->generatedAccessibleLabel();
    } else if (const auto* dataGrid = dynamic_cast<const DataGrid*>(&node)) {
        properties.role = AccessibilityRole::DataGrid;
        properties.label = dataGrid->accessibleLabel();
        properties.value = std::to_string(dataGrid->focusedRow());
    } else if (const auto* table = dynamic_cast<const Table*>(&node)) {
        properties.role = AccessibilityRole::Table;
        properties.label = table->accessibleLabel();
    } else if (const auto* calendar = dynamic_cast<const Calendar*>(&node)) {
        properties.role = AccessibilityRole::Calendar;
        properties.label = "Calendar";
        properties.value = formatIsoDate(calendar->focusedDate());
    } else if (const auto* datePicker = dynamic_cast<const DatePicker*>(&node)) {
        properties.role = AccessibilityRole::ComboBox;
        properties.label = "Date";
        properties.value = datePicker->text();
        properties.expanded = datePicker->isOpen();
    } else if (const auto* timePicker = dynamic_cast<const TimePicker*>(&node)) {
        properties.role = AccessibilityRole::ComboBox;
        properties.label = "Time";
        properties.value = timePicker->text();
        properties.expanded = timePicker->isOpen();
    } else if (const auto* tree = dynamic_cast<const Tree*>(&node)) {
        properties.role = AccessibilityRole::Tree;
        properties.label = tree->accessibleLabel();
    } else if (const auto* treeItem = dynamic_cast<const TreeItem*>(&node)) {
        properties.role = AccessibilityRole::TreeItem;
        properties.label = treeItem->label();
        properties.checked = treeItem->isSelected();
        properties.level = static_cast<int>(treeItem->level());
        if (treeItem->hasChildren()) properties.expanded = treeItem->isExpanded();
    } else if (const auto* teachingPopover = dynamic_cast<const TeachingPopover*>(&node)) {
        properties.role = AccessibilityRole::Dialog;
        properties.label = teachingPopover->title();
        properties.description = teachingPopover->body();
    } else if (const auto* popover = dynamic_cast<const Popover*>(&node)) {
        properties.role = AccessibilityRole::Group;
        properties.label = popover->accessibleLabel().empty() ? popover->title() : popover->accessibleLabel();
        properties.description = popover->body();
    } else if (const auto* menuButton = dynamic_cast<const MenuButton*>(&node)) {
        properties.role = AccessibilityRole::Button;
        properties.label = menuButton->label();
        properties.expanded = menuButton->isOpen();
    } else if (const auto* splitButton = dynamic_cast<const SplitButton*>(&node)) {
        properties.role = AccessibilityRole::Button;
        properties.label = splitButton->label();
        properties.expanded = splitButton->isOpen();
    } else if (const auto* compoundButton = dynamic_cast<const CompoundButton*>(&node)) {
        properties.role = AccessibilityRole::Button;
        properties.label = compoundButton->label();
        properties.description = compoundButton->secondaryContent();
    } else if (const auto* toggleButton = dynamic_cast<const ToggleButton*>(&node)) {
        properties.role = AccessibilityRole::Button;
        properties.label = toggleButton->label();
        properties.checked = toggleButton->isChecked();
    } else if (const auto* button = dynamic_cast<const Button*>(&node)) {
        properties.role = AccessibilityRole::Button;
        properties.label = button->label();
    } else if (const auto* iconButton = dynamic_cast<const IconButton*>(&node)) {
        properties.role = iconButton->checked().has_value()
            ? AccessibilityRole::CheckBox
            : AccessibilityRole::Button;
        properties.label = iconButton->accessibleLabel();
        properties.checked = iconButton->checked();
    } else if (const auto* checkbox = dynamic_cast<const Checkbox*>(&node)) {
        properties.role = AccessibilityRole::CheckBox;
        properties.label = checkbox->accessibleLabel().empty()
            ? checkbox->label()
            : checkbox->accessibleLabel();
        properties.checked = checkbox->isChecked();
        properties.mixed = checkbox->isMixed();
        properties.required = checkbox->isRequired();
    } else if (const auto* group = dynamic_cast<const RadioGroup*>(&node)) {
        properties.role = AccessibilityRole::RadioGroup;
        properties.label = group->accessibleLabel();
        properties.value = group->value();
        properties.required = group->isRequired();
        properties.selectionIsSelectionRequired = true;
    } else if (const auto* radio = dynamic_cast<const Radio*>(&node)) {
        properties.role = AccessibilityRole::RadioButton;
        properties.label = radio->label();
        properties.checked = radio->isSelected();
    } else if (const auto* toggle = dynamic_cast<const Switch*>(&node)) {
        properties.role = AccessibilityRole::Switch;
        properties.label = toggle->label();
        properties.checked = toggle->isOn();
        properties.required = toggle->isRequired();
    } else if (const auto* rating = dynamic_cast<const Rating*>(&node)) {
        properties.role = AccessibilityRole::RadioGroup;
        properties.label = rating->accessibleLabel();
        properties.value = rating->accessibleValueText();
        properties.description = rating->labelForValue(rating->value());
    } else if (const auto* slider = dynamic_cast<const Slider*>(&node)) {
        properties.role = AccessibilityRole::Slider;
        properties.label = slider->accessibleLabel();
        properties.value = std::to_string(slider->value());
        properties.numericValue = slider->value();
        properties.minimumValue = slider->minimum();
        properties.maximumValue = slider->maximum();
        properties.smallChange = slider->step() > 0.0f ? slider->step() : 1.0f;
        properties.largeChange = std::max(slider->step() > 0.0f ? slider->step() : 1.0f,
                                          (slider->maximum() - slider->minimum()) / 10.0f);
    } else if (const auto* progress = dynamic_cast<const ProgressBar*>(&node)) {
        properties.role = AccessibilityRole::ProgressBar;
        properties.label = progress->accessibleLabel();
        if (const auto value = progress->determinateValue()) {
            properties.value = std::to_string(*value);
            properties.numericValue = *value;
            properties.minimumValue = progress->minimum();
            properties.maximumValue = progress->maximum();
            properties.smallChange = 0.0;
            properties.largeChange = 0.0;
        } else {
            properties.busy = true;
            properties.description = "In progress";
        }
    } else if (const auto* toast = dynamic_cast<const Toast*>(&node)) {
        properties.role = AccessibilityRole::Alert;
        properties.label = toast->title();
        properties.description = toast->body();
        properties.live = true;
    } else if (const auto* spinner = dynamic_cast<const Spinner*>(&node)) {
        properties.role = AccessibilityRole::ProgressBar;
        properties.label = spinner->label();
        properties.description = "In progress";
        properties.busy = true;
    } else if (const auto* avatar = dynamic_cast<const Avatar*>(&node)) {
        properties.role = AccessibilityRole::Image;
        properties.label = avatar->accessibleLabel().empty() ? avatar->name()
                                                               : avatar->accessibleLabel();
    } else if (const auto* group = dynamic_cast<const AvatarGroup*>(&node)) {
        properties.role = AccessibilityRole::Group;
        properties.label = group->accessibleLabel().empty()
            ? std::to_string(group->children().size()) + " people"
            : group->accessibleLabel();
    } else if (const auto* combo = dynamic_cast<const Combobox*>(&node)) {
        properties.role = AccessibilityRole::ComboBox;
        properties.label = combo->accessibleLabel().empty() ? combo->placeholder() : combo->accessibleLabel();
        properties.value = combo->controller().text();
        properties.expanded = combo->isOpen();
    } else if (const auto* dropdown = dynamic_cast<const Dropdown*>(&node)) {
        properties.role = AccessibilityRole::ComboBox;
        properties.label = dropdown->accessibleLabel().empty() ? dropdown->placeholder() : dropdown->accessibleLabel();
        properties.value = dropdown->value();
        properties.expanded = dropdown->isOpen();
    } else if (const auto* listBox = dynamic_cast<const ListBox*>(&node)) {
        properties.role = AccessibilityRole::ListBox;
        properties.label = listBox->accessibleLabel();
        properties.selectionCanSelectMultiple =
            listBox->selectionMode() == ListBoxSelectionMode::Multiple;
        if (const int selected = listBox->selectedIndex(); selected >= 0 && static_cast<std::size_t>(selected) < listBox->options().size()) {
            properties.value = listBox->options()[static_cast<std::size_t>(selected)].value;
        }
    } else if (const auto* input = dynamic_cast<const TextInput*>(&node)) {
        properties.role = AccessibilityRole::TextField;
        properties.label = input->accessibleLabel().empty()
            ? input->placeholder()
            : input->accessibleLabel();
        properties.value = input->controller().text();
    } else if (const auto* toolbar = dynamic_cast<const Toolbar*>(&node)) {
        properties.role = AccessibilityRole::Toolbar;
        properties.label = toolbar->accessibleLabel();
    } else if (const auto* toolbarItem = dynamic_cast<const ToolbarItem*>(&node)) {
        properties.role = AccessibilityRole::Button;
        properties.label = toolbarItem->label();
    } else if (const auto* tabList = dynamic_cast<const TabList*>(&node)) {
        properties.role = AccessibilityRole::TabList;
        properties.label = tabList->accessibleLabel();
        properties.value = tabList->value();
        properties.selectionIsSelectionRequired = true;
    } else if (const auto* tab = dynamic_cast<const Tab*>(&node)) {
        properties.role = AccessibilityRole::Tab;
        properties.label = tab->label();
        properties.checked = tab->isSelected();
    } else if (const auto* tabPanel = dynamic_cast<const TabPanel*>(&node)) {
        properties.role = AccessibilityRole::TabPanel;
        properties.label = tabPanel->accessibleLabel().empty() ? tabPanel->value() : tabPanel->accessibleLabel();
    } else if (const auto* link = dynamic_cast<const Link*>(&node)) {
        properties.role = AccessibilityRole::Link;
        properties.label = link->label();
        properties.description = link->href();
    } else if (const auto* breadcrumb = dynamic_cast<const Breadcrumb*>(&node)) {
        properties.role = AccessibilityRole::Group;
        properties.label = breadcrumb->accessibleLabel();
    } else if (const auto* breadcrumbItem = dynamic_cast<const BreadcrumbItem*>(&node)) {
        properties.role = breadcrumbItem->isCurrent() ? AccessibilityRole::Text : AccessibilityRole::Link;
        properties.label = breadcrumbItem->label();
    } else if (const auto* text = dynamic_cast<const Text*>(&node)) {
        properties.role = text->role() == TextRole::Heading
            ? AccessibilityRole::Heading
            : AccessibilityRole::Text;
        properties.label = text->value();
    } else if (const auto* badge = dynamic_cast<const Badge*>(&node)) {
        properties.role = AccessibilityRole::Text;
        properties.label = badge->generatedAccessibleLabel();
    } else if (const auto* counter = dynamic_cast<const CounterBadge*>(&node)) {
        const std::string label = counter->generatedAccessibleLabel();
        if (label.empty()) return properties;
        properties.role = AccessibilityRole::Text;
        properties.label = label;
        properties.value = counter->text();
    } else if (const auto* presence = dynamic_cast<const PresenceBadge*>(&node)) {
        properties.role = AccessibilityRole::Text;
        properties.label = presence->generatedAccessibleLabel();
    } else if (const auto* card = dynamic_cast<const Card*>(&node)) {
        // A passive Card is grouping content; a selectable Card is exposed as
        // a selectable list item. UIA already projects `checked` onto the
        // SelectionItemIsSelected property, while the framework Toggle action
        // provides a deterministic cross-platform selection operation.
        properties.role = card->isSelectable()
            ? AccessibilityRole::ListItem
            : AccessibilityRole::Group;
        if (card->isSelectable()) {
            properties.checked = card->isSelected();
        }
    } else if (dynamic_cast<const Menu*>(&node) != nullptr) {
        properties.role = AccessibilityRole::Menu;
    } else if (dynamic_cast<const Dialog*>(&node) != nullptr) {
        properties.role = AccessibilityRole::Dialog;
    } else if (const auto* ratingDisplay = dynamic_cast<const RatingDisplay*>(&node)) {
        properties.role = AccessibilityRole::Image;
        properties.label = ratingDisplay->generatedAccessibleLabel();
    } else if (const auto* image = dynamic_cast<const Image*>(&node)) {
        if (image->isDecorative()) return properties;
        properties.role = AccessibilityRole::Image;
        properties.label = image->alt();
    } else if (const auto* divider = dynamic_cast<const Divider*>(&node)) {
        properties.role = AccessibilityRole::Separator;
        properties.label = divider->content();
    } else if (const auto* field = dynamic_cast<const Field*>(&node)) {
        properties.role = AccessibilityRole::Group;
        properties.label = field->label();
        properties.description = field->validationMessage().empty()
            ? field->hint()
            : field->validationMessage();
        properties.required = field->isRequired();
        properties.enabled = field->isEnabled();
    } else if (const auto* message = dynamic_cast<const MessageBar*>(&node)) {
        properties.role = AccessibilityRole::Alert;
        properties.label = message->title().empty() ? message->body() : message->title();
        properties.description = message->title().empty() ? std::string{} : message->body();
        properties.live = true;
    } else {
        return properties;
    }

    properties.actions = node.accessibilityActions();
    // Drawers are focusable dialog boundaries even though the surface itself
    // is not a ControlNode. This permits native adapters to move focus into
    // the active modal before traversing its contained controls.
    if (dynamic_cast<const Drawer*>(&node) != nullptr) {
        properties.actions.focus = true;
    }
    if (const auto* control = dynamic_cast<const ControlNode*>(&node)) {
        properties.enabled = control->isEnabled();
        properties.actions.focus = true;
    }
    // Table deliberately models a passive, read-only data presentation.
    // It inherits ControlNode for shared painting/disabled-state support, but
    // it must not become a keyboard stop merely because of that implementation
    // detail. DataGrid keeps the normal interactive control focus contract.
    if (dynamic_cast<const Table*>(&node) != nullptr &&
        dynamic_cast<const DataGrid*>(&node) == nullptr) {
        properties.actions.focus = false;
    }
    // Passive Personas are presentational identity groups, not focus stops.
    // Interactive Personas opt in to Button semantics through onClick().
    if (const auto* persona = dynamic_cast<const Persona*>(&node)) {
        properties.actions.focus = persona->isInteractive();
    }
    properties.automationId = node.accessibilityId();
    properties.focused = &node == focused;
    properties.bounds = node.bounds();
    return properties;
}

void appendNodeSnapshot(const Node& node,
                        const Node* focused,
                        std::vector<std::size_t>& path,
                        AccessibilitySnapshot& snapshot)
{
    // A linked inactive TabPanel is intentionally absent from both the visual
    // and semantic trees.  Its retained descendants must not remain exposed
    // to screen readers or UIA while another tab owns the visible panel.
    if (const auto* tabPanel = dynamic_cast<const TabPanel*>(&node);
        tabPanel != nullptr && !tabPanel->isActive()) {
        return;
    }
    auto properties = propertiesForNode(node, focused);
    if (properties.role != AccessibilityRole::Unknown) {
        snapshot.push_back({path, path.size(), std::move(properties)});
    }

    // Accordion items retain their body nodes across collapse/expand cycles,
    // but a collapsed body is not part of the active semantic surface.
    // Preserve the item Button itself while preventing hidden descendants from
    // being exposed to a screen reader or Windows UI Automation.
    if (const auto* accordionItem = dynamic_cast<const AccordionItem*>(&node);
        accordionItem != nullptr && !accordionItem->isExpanded()) {
        return;
    }
    // Tree items retain descendants across collapse/expand cycles. Keep the
    // current TreeItem in the semantic tree, but never expose its hidden
    // branch to UI Automation or a screen reader.
    if (const auto* treeItem = dynamic_cast<const TreeItem*>(&node);
        treeItem != nullptr && !treeItem->isExpanded()) {
        return;
    }

    // A ListBox windows its visual rows, but its visible options still need
    // to be discoverable as ListItem children to UI Automation. These are
    // logical children, not retained Nodes: a reserved path segment prevents
    // collisions with ordinary child indices and lets UiWindow route Invoke
    // back to the owning ListBox by option value.
    if (const auto* listBox = dynamic_cast<const ListBox*>(&node)) {
        for (auto option : listBox->accessibilityOptions()) {
            if (!option.properties.bounds.has_value()) {
                continue;
            }
            path.push_back(detail::kVirtualAccessibilityChild);
            path.push_back(option.index);
            snapshot.push_back({path, path.size(), std::move(option.properties)});
            path.pop_back();
            path.pop_back();
        }
    }

    // Tables window their rows just like ListBox. Materialize only the header
    // and current row window as logical descendants, preserving data-model
    // ids rather than inventing retained Node objects for every cell.
    if (const auto* table = dynamic_cast<const Table*>(&node)) {
        const bool dataGrid = dynamic_cast<const DataGrid*>(table) != nullptr;
        for (auto entry : table->accessibilityEntries()) {
            switch (entry.kind) {
            case TableAccessibilityKind::ColumnHeader:
                entry.properties.role = AccessibilityRole::ColumnHeader;
                break;
            case TableAccessibilityKind::Row:
                entry.properties.role = dataGrid ? AccessibilityRole::DataGridRow
                                                 : AccessibilityRole::TableRow;
                break;
            case TableAccessibilityKind::Cell:
                entry.properties.role = dataGrid ? AccessibilityRole::DataGridCell
                                                 : AccessibilityRole::TableCell;
                break;
            }
            path.push_back(detail::kVirtualAccessibilityChild);
            path.push_back(static_cast<std::size_t>(entry.kind));
            if (entry.row.has_value()) path.push_back(*entry.row);
            if (entry.column.has_value()) path.push_back(*entry.column);
            snapshot.push_back({path, path.size(), std::move(entry.properties)});
            if (entry.column.has_value()) path.pop_back();
            if (entry.row.has_value()) path.pop_back();
            path.pop_back();
            path.pop_back();
        }
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
