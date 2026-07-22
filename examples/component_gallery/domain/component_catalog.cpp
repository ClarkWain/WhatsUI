#include "domain/component_catalog.h"

#include <algorithm>
#include <utility>

namespace whatsui::gallery {
namespace {

ComponentDescriptor component(std::string id, std::string name, std::string summary,
                              ComponentCategory category, ComponentIcon icon,
                              bool featured = false,
                              ComponentMaturity maturity = ComponentMaturity::Stable,
                              std::vector<std::string> keywords = {})
{
    return {std::move(id), std::move(name), std::move(summary), category, icon,
            maturity, featured, std::move(keywords)};
}

std::vector<ComponentDescriptor> defaultComponents()
{
    using C = ComponentCategory;
    using I = ComponentIcon;
    using M = ComponentMaturity;
    return {
        component("button", "Button", "Triggers an immediate action.", C::Controls, I::Component, true, M::Stable, {"action", "primary"}),
        component("compound-button", "Compound button", "Adds supporting text to an action.", C::Controls, I::Component),
        component("menu-button", "Menu button", "Opens a menu of related actions.", C::Controls, I::Component),
        component("toggle-button", "Toggle button", "Switches an action between two states.", C::Controls, I::Component, true),
        component("split-button", "Split button", "Combines a primary action and menu.", C::Controls, I::Component),
        component("checkbox", "Checkbox", "Selects one or more independent options.", C::Controls, I::Component, true),
        component("radio-group", "Radio group", "Selects one option from a group.", C::Controls, I::Component),
        component("switch", "Switch", "Turns a setting on or off.", C::Controls, I::Component, true),
        component("slider", "Slider", "Selects a value from a continuous range.", C::Controls, I::Component),
        component("rating", "Rating", "Captures a rating through familiar symbols.", C::Controls, I::Component),
        component("label", "Label", "Names an associated form control.", C::Inputs, I::Input),
        component("text", "Text", "Displays Fluent typography and rich text.", C::Inputs, I::Input),
        component("input", "Input", "Captures a single line of text.", C::Inputs, I::Input, true, M::Stable, {"textfield", "search"}),
        component("textarea", "Textarea", "Captures multiple lines of text.", C::Inputs, I::Input),
        component("field", "Field", "Composes labels, controls, hints, and validation.", C::Inputs, I::Input),
        component("combobox", "Combobox", "Filters and selects from a list.", C::Inputs, I::Input),
        component("dropdown", "Dropdown", "Selects from a closed list of options.", C::Inputs, I::Input),
        component("progress-bar", "Progress bar", "Communicates determinate task progress.", C::Feedback, I::Feedback, true),
        component("spinner", "Spinner", "Communicates indeterminate activity.", C::Feedback, I::Feedback),
        component("message-bar", "Message bar", "Presents persistent page-level status.", C::Feedback, I::Feedback),
        component("toast", "Toast", "Presents transient, actionable feedback.", C::Feedback, I::Feedback),
        component("badge", "Badge", "Highlights status or a compact attribute.", C::Feedback, I::Feedback),
        component("counter-badge", "Counter badge", "Displays a compact numeric count.", C::Feedback, I::Feedback),
        component("presence-badge", "Presence badge", "Communicates a person's availability.", C::Feedback, I::Feedback),
        component("toolbar", "Toolbar", "Groups frequently used commands.", C::Navigation, I::Navigation),
        component("tab-list", "Tab list", "Switches between peer content views.", C::Navigation, I::Navigation),
        component("link", "Link", "Navigates to related content.", C::Navigation, I::Navigation),
        component("breadcrumb", "Breadcrumb", "Shows hierarchy and parent navigation.", C::Navigation, I::Navigation),
        component("accordion", "Accordion", "Progressively discloses grouped content.", C::Navigation, I::Navigation),
        component("tree", "Tree", "Navigates hierarchical collections.", C::Navigation, I::Navigation),
        component("table", "Table", "Displays structured read-only data.", C::DataDisplay, I::Data),
        component("data-grid", "Data grid", "Adds selection and sorting to tabular data.", C::DataDisplay, I::Data, true),
        component("list-view", "List view", "Displays selectable rows of data.", C::DataDisplay, I::Data),
        component("image", "Image", "Displays fitted or cropped image content.", C::DataDisplay, I::Data),
        component("rating-display", "Rating display", "Displays a read-only rating summary.", C::DataDisplay, I::Data),
        component("card", "Card", "Groups related information and actions.", C::Layout, I::Layout, true),
        component("divider", "Divider", "Separates related content regions.", C::Layout, I::Layout),
        component("drawer", "Drawer", "Hosts supplemental content at an edge.", C::Layout, I::Layout),
        component("avatar", "Avatar", "Represents a person or entity.", C::Identity, I::Person),
        component("avatar-group", "Avatar group", "Presents a compact group of people.", C::Identity, I::Person),
        component("persona", "Persona", "Combines identity and supporting details.", C::Identity, I::Person),
        component("calendar", "Calendar", "Selects a date from a month view.", C::DateTime, I::Calendar, false, M::Preview),
        component("date-picker", "Date picker", "Captures a formatted date.", C::DateTime, I::Calendar),
        component("time-picker", "Time picker", "Captures a formatted time.", C::DateTime, I::Calendar),
        component("popover", "Popover", "Anchors contextual content to a trigger.", C::Overlays, I::Overlay),
        component("teaching-popover", "Teaching popover", "Explains a feature in context.", C::Overlays, I::Overlay),
    };
}

} // namespace

ComponentCatalog::ComponentCatalog()
    : components_(defaultComponents())
{
}

ComponentCatalog::ComponentCatalog(std::vector<ComponentDescriptor> components)
    : components_(std::move(components))
{
}

const std::vector<ComponentDescriptor>& ComponentCatalog::components() const noexcept
{
    return components_;
}

const ComponentDescriptor* ComponentCatalog::find(std::string_view id) const noexcept
{
    const auto found = std::find_if(components_.begin(), components_.end(),
                                    [id](const ComponentDescriptor& item) {
                                        return item.id == id;
                                    });
    return found == components_.end() ? nullptr : &*found;
}

} // namespace whatsui::gallery
