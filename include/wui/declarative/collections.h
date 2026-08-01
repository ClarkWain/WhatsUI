#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "wui/date_time.h"
#include "wui/list_view.h"
#include "wui/rating.h"
#include "wui/selection.h"
#include "wui/table.h"
#include "wui/tree.h"
#include "wui/declarative/builder_base.h"

namespace wui {

class Calendar : public BuilderBase<Calendar, wui::CalendarNode> {
public:
    Calendar() : BuilderBase() {}
    Calendar& displayedMonth(wui::CivilDate value) & { node_->setDisplayedMonth(value); return self(); }

    Calendar&& displayedMonth(wui::CivilDate value) && { node_->setDisplayedMonth(value); return std::move(self()); }
    Calendar& selectedDate(std::optional<wui::CivilDate> value) & { node_->setSelectedDate(value); return self(); }

    Calendar&& selectedDate(std::optional<wui::CivilDate> value) && { node_->setSelectedDate(value); return std::move(self()); }
    Calendar& selectionMode(wui::CalendarSelectionMode value) & { node_->setSelectionMode(value); return self(); }

    Calendar&& selectionMode(wui::CalendarSelectionMode value) && { node_->setSelectionMode(value); return std::move(self()); }
    Calendar& minimumDate(std::optional<wui::CivilDate> value) & { node_->minimumDate(value); return self(); }

    Calendar&& minimumDate(std::optional<wui::CivilDate> value) && { node_->minimumDate(value); return std::move(self()); }
    Calendar& maximumDate(std::optional<wui::CivilDate> value) & { node_->maximumDate(value); return self(); }

    Calendar&& maximumDate(std::optional<wui::CivilDate> value) && { node_->maximumDate(value); return std::move(self()); }
};

class DatePicker : public BuilderBase<DatePicker, wui::DatePickerNode> {
public:
    explicit DatePicker(std::string placeholder = "Select a date") : BuilderBase(std::move(placeholder)) {}
    DatePicker& value(std::optional<wui::CivilDate> value) & { node_->setValue(value); return self(); }

    DatePicker&& value(std::optional<wui::CivilDate> value) && { node_->setValue(value); return std::move(self()); }
    DatePicker& text(std::string value) & { node_->text(std::move(value)); return self(); }

    DatePicker&& text(std::string value) && { node_->text(std::move(value)); return std::move(self()); }
    DatePicker& overlayHost(wui::OverlayHost& host) & { node_->bindOverlayHost(host); return self(); }

    DatePicker&& overlayHost(wui::OverlayHost& host) && { node_->bindOverlayHost(host); return std::move(self()); }
};

class TimePicker : public BuilderBase<TimePicker, wui::TimePickerNode> {
public:
    explicit TimePicker(std::string placeholder = "Select a time") : BuilderBase(std::move(placeholder)) {}
    TimePicker& value(std::optional<wui::CivilTime> value) & { node_->setValue(value); return self(); }

    TimePicker&& value(std::optional<wui::CivilTime> value) && { node_->setValue(value); return std::move(self()); }
    TimePicker& text(std::string value) & { node_->text(std::move(value)); return self(); }

    TimePicker&& text(std::string value) && { node_->text(std::move(value)); return std::move(self()); }
    TimePicker& minuteStep(int value) & { node_->minuteStep(value); return self(); }

    TimePicker&& minuteStep(int value) && { node_->minuteStep(value); return std::move(self()); }
};

class Table : public BuilderBase<Table, wui::TableNode> {
public:
    explicit Table(std::vector<wui::TableColumn> columns = {}) : BuilderBase(std::move(columns)) {}
    Table& rows(std::vector<wui::TableRow> value) & { node_->setRows(std::move(value)); return self(); }

    Table&& rows(std::vector<wui::TableRow> value) && { node_->setRows(std::move(value)); return std::move(self()); }
    Table& rowProvider(std::size_t count, wui::TableNode::RowProvider provider, wui::TableNode::RowEnabledProvider enabled = {}) & { node_->setRowProvider(count, std::move(provider), std::move(enabled)); return self(); }

    Table&& rowProvider(std::size_t count, wui::TableNode::RowProvider provider, wui::TableNode::RowEnabledProvider enabled = {}) && { node_->setRowProvider(count, std::move(provider), std::move(enabled)); return std::move(self()); }
    Table& maxVisibleRows(std::size_t value) & { node_->maxVisibleRows(value); return self(); }

    Table&& maxVisibleRows(std::size_t value) && { node_->maxVisibleRows(value); return std::move(self()); }
    Table& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Table&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class DataGrid : public BuilderBase<DataGrid, wui::DataGridNode> {
public:
    DataGrid() : BuilderBase() {}
    DataGrid& columns(std::vector<wui::TableColumn> value) & { node_->setColumns(std::move(value)); return self(); }

    DataGrid&& columns(std::vector<wui::TableColumn> value) && { node_->setColumns(std::move(value)); return std::move(self()); }
    DataGrid& rows(std::vector<wui::TableRow> value) & { node_->setRows(std::move(value)); return self(); }

    DataGrid&& rows(std::vector<wui::TableRow> value) && { node_->setRows(std::move(value)); return std::move(self()); }
    DataGrid& rowProvider(std::size_t count, wui::TableNode::RowProvider provider, wui::TableNode::RowEnabledProvider enabled = {}) & { node_->setRowProvider(count, std::move(provider), std::move(enabled)); return self(); }

    DataGrid&& rowProvider(std::size_t count, wui::TableNode::RowProvider provider, wui::TableNode::RowEnabledProvider enabled = {}) && { node_->setRowProvider(count, std::move(provider), std::move(enabled)); return std::move(self()); }
    DataGrid& selectionMode(wui::DataGridSelectionMode value) & { node_->selectionMode(value); return self(); }

    DataGrid&& selectionMode(wui::DataGridSelectionMode value) && { node_->selectionMode(value); return std::move(self()); }
    DataGrid& selectedRows(std::vector<std::size_t> value) & { node_->selectedRows(std::move(value)); return self(); }

    DataGrid&& selectedRows(std::vector<std::size_t> value) && { node_->selectedRows(std::move(value)); return std::move(self()); }
    DataGrid& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    DataGrid&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Tree : public BuilderBase<Tree, wui::TreeNode> {
public:
    Tree() : BuilderBase() {}
    Tree& item(std::string id, std::string label) & { node_->addItem(std::move(id), std::move(label)); return self(); }

    Tree&& item(std::string id, std::string label) && { node_->addItem(std::move(id), std::move(label)); return std::move(self()); }
    Tree& maxVisibleItems(std::size_t value) & { node_->setMaxVisibleItems(value); return self(); }

    Tree&& maxVisibleItems(std::size_t value) && { node_->setMaxVisibleItems(value); return std::move(self()); }
    Tree& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Tree&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class ListBox : public BuilderBase<ListBox, wui::ListBoxNode> {
public:
    explicit ListBox(std::vector<wui::Option> options = {}) : BuilderBase(std::move(options)) {}
    ListBox& option(wui::Option value) & { node_->addOption(std::move(value)); return self(); }

    ListBox&& option(wui::Option value) && { node_->addOption(std::move(value)); return std::move(self()); }
    ListBox& selectedIndex(int value) & { node_->setSelectedIndex(value); return self(); }

    ListBox&& selectedIndex(int value) && { node_->setSelectedIndex(value); return std::move(self()); }
    ListBox& multiple(bool value = true) &
    { node_->setSelectionMode(value ? wui::ListBoxSelectionMode::Multiple : wui::ListBoxSelectionMode::Single); return self(); }

    ListBox&& multiple(bool value = true) &&
    { node_->setSelectionMode(value ? wui::ListBoxSelectionMode::Multiple : wui::ListBoxSelectionMode::Single); return std::move(self()); }
    ListBox& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    ListBox&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class Combobox : public BuilderBase<Combobox, wui::ComboboxNode> {
public:
    explicit Combobox(std::string placeholder = {}) : BuilderBase(std::move(placeholder)) {}
    Combobox& option(wui::Option value) & { node_->addOption(std::move(value)); return self(); }

    Combobox&& option(wui::Option value) && { node_->addOption(std::move(value)); return std::move(self()); }
    Combobox& selectedIndex(int value) & { node_->setSelectedIndex(value); return self(); }

    Combobox&& selectedIndex(int value) && { node_->setSelectedIndex(value); return std::move(self()); }
    Combobox& selectedIndices(std::vector<int> value) & { node_->setSelectedIndices(std::move(value)); return self(); }

    Combobox&& selectedIndices(std::vector<int> value) && { node_->setSelectedIndices(std::move(value)); return std::move(self()); }
    Combobox& multiselect(bool value = true) & { node_->setMultiselect(value); return self(); }

    Combobox&& multiselect(bool value = true) && { node_->setMultiselect(value); return std::move(self()); }
    Combobox& overlayHost(wui::OverlayHost& host) & { node_->bindOverlayHost(host); return self(); }

    Combobox&& overlayHost(wui::OverlayHost& host) && { node_->bindOverlayHost(host); return std::move(self()); }
    Combobox& onSelectionChanged(wui::ComboboxNode::SelectionHandler handler) &
    { node_->onSelectionChanged(std::move(handler)); return self(); }

    Combobox&& onSelectionChanged(wui::ComboboxNode::SelectionHandler handler) &&
    { node_->onSelectionChanged(std::move(handler)); return std::move(self()); }
    Combobox& onChange(wui::TextFieldNode::ChangeHandler handler) & { node_->onChange(std::move(handler)); return self(); }

    Combobox&& onChange(wui::TextFieldNode::ChangeHandler handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
};

class Dropdown : public BuilderBase<Dropdown, wui::DropdownNode> {
public:
    explicit Dropdown(std::string placeholder = "Select an option") : BuilderBase(std::move(placeholder)) {}
    Dropdown& option(wui::Option value) & { node_->addOption(std::move(value)); return self(); }

    Dropdown&& option(wui::Option value) && { node_->addOption(std::move(value)); return std::move(self()); }
    Dropdown& selectedIndex(int value) & { node_->setSelectedIndex(value); return self(); }

    Dropdown&& selectedIndex(int value) && { node_->setSelectedIndex(value); return std::move(self()); }
    Dropdown& selectedIndices(std::vector<int> value) & { node_->setSelectedIndices(std::move(value)); return self(); }

    Dropdown&& selectedIndices(std::vector<int> value) && { node_->setSelectedIndices(std::move(value)); return std::move(self()); }
    Dropdown& multiselect(bool value = true) & { node_->setMultiselect(value); return self(); }

    Dropdown&& multiselect(bool value = true) && { node_->setMultiselect(value); return std::move(self()); }
    Dropdown& overlayHost(wui::OverlayHost& host) & { node_->bindOverlayHost(host); return self(); }

    Dropdown&& overlayHost(wui::OverlayHost& host) && { node_->bindOverlayHost(host); return std::move(self()); }
    Dropdown& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Dropdown&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    Dropdown& onSelectionChanged(wui::DropdownNode::SelectionHandler handler) &
    { node_->onSelectionChanged(std::move(handler)); return self(); }

    Dropdown&& onSelectionChanged(wui::DropdownNode::SelectionHandler handler) &&
    { node_->onSelectionChanged(std::move(handler)); return std::move(self()); }
};

class Rating : public BuilderBase<Rating, wui::RatingNode> {
public:
    explicit Rating(float value = 0.0f, int maximum = 5) : BuilderBase(value, maximum) {}
    Rating& value(float value) & { node_->setValue(value); return self(); }

    Rating&& value(float value) && { node_->setValue(value); return std::move(self()); }
    Rating& maximum(int value) & { node_->setMaximum(value); return self(); }

    Rating&& maximum(int value) && { node_->setMaximum(value); return std::move(self()); }
    Rating& step(float value) & { node_->setStep(value); return self(); }

    Rating&& step(float value) && { node_->setStep(value); return std::move(self()); }
    Rating& color(wui::RatingColor value) & { node_->setColor(value); return self(); }

    Rating&& color(wui::RatingColor value) && { node_->setColor(value); return std::move(self()); }
    Rating& size(wui::RatingSize value) & { node_->setSize(value); return self(); }

    Rating&& size(wui::RatingSize value) && { node_->setSize(value); return std::move(self()); }
    Rating& shape(wui::RatingShape value) & { node_->setShape(value); return self(); }

    Rating&& shape(wui::RatingShape value) && { node_->setShape(value); return std::move(self()); }
    Rating& readOnly(bool value = true) & { node_->setReadOnly(value); return self(); }

    Rating&& readOnly(bool value = true) && { node_->setReadOnly(value); return std::move(self()); }
    Rating& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    Rating&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
    Rating& itemLabel(wui::RatingNode::ItemLabelHandler handler) & { node_->setItemLabel(std::move(handler)); return self(); }

    Rating&& itemLabel(wui::RatingNode::ItemLabelHandler handler) && { node_->setItemLabel(std::move(handler)); return std::move(self()); }
    Rating& bind(wui::State<float>& state) & { node_->bind(state); return self(); }

    Rating&& bind(wui::State<float>& state) && { node_->bind(state); return std::move(self()); }
    Rating& onChange(std::function<void(float)> handler) & { node_->onChange(std::move(handler)); return self(); }

    Rating&& onChange(std::function<void(float)> handler) && { node_->onChange(std::move(handler)); return std::move(self()); }
    Rating& enabled(bool value) & { node_->setEnabled(value); return self(); }

    Rating&& enabled(bool value) && { node_->setEnabled(value); return std::move(self()); }
};

class RatingDisplay : public BuilderBase<RatingDisplay, wui::RatingDisplayNode> {
public:
    explicit RatingDisplay(std::optional<float> value = std::optional<float>{0.0f}, int maximum = 5)
        : BuilderBase(value, maximum) {}
    RatingDisplay& value(float value) & { node_->setValue(value); return self(); }

    RatingDisplay&& value(float value) && { node_->setValue(value); return std::move(self()); }
    RatingDisplay& maximum(int value) & { node_->setMaximum(value); return self(); }

    RatingDisplay&& maximum(int value) && { node_->setMaximum(value); return std::move(self()); }
    RatingDisplay& count(std::uint64_t value) & { node_->setCount(value); return self(); }

    RatingDisplay&& count(std::uint64_t value) && { node_->setCount(value); return std::move(self()); }
    RatingDisplay& compact(bool value = true) & { node_->setCompact(value); return self(); }

    RatingDisplay&& compact(bool value = true) && { node_->setCompact(value); return std::move(self()); }
    RatingDisplay& color(wui::RatingColor value) & { node_->setColor(value); return self(); }

    RatingDisplay&& color(wui::RatingColor value) && { node_->setColor(value); return std::move(self()); }
    RatingDisplay& size(wui::RatingSize value) & { node_->setSize(value); return self(); }

    RatingDisplay&& size(wui::RatingSize value) && { node_->setSize(value); return std::move(self()); }
    RatingDisplay& shape(wui::RatingShape value) & { node_->setShape(value); return self(); }

    RatingDisplay&& shape(wui::RatingShape value) && { node_->setShape(value); return std::move(self()); }
    RatingDisplay& accessibleLabel(std::string value) & { node_->setAccessibleLabel(std::move(value)); return self(); }

    RatingDisplay&& accessibleLabel(std::string value) && { node_->setAccessibleLabel(std::move(value)); return std::move(self()); }
};

class ListView : public BuilderBase<ListView, wui::ListViewNode> {
public:
    explicit ListView(std::vector<wui::ListViewNode::Item> items = {}, int selectedIndex = -1)
        : BuilderBase(std::move(items), selectedIndex) {}
    ListView& itemProvider(std::size_t count, wui::ListViewNode::ItemProvider provider, wui::ListViewNode::SelectableProvider selectable = {}) & { node_->setItemProvider(count, std::move(provider), std::move(selectable)); return self(); }

    ListView&& itemProvider(std::size_t count, wui::ListViewNode::ItemProvider provider, wui::ListViewNode::SelectableProvider selectable = {}) && { node_->setItemProvider(count, std::move(provider), std::move(selectable)); return std::move(self()); }
    ListView& selectedIndex(int value) & { node_->setSelectedIndex(value); return self(); }

    ListView&& selectedIndex(int value) && { node_->setSelectedIndex(value); return std::move(self()); }
    ListView& bind(wui::State<int>& state) & { node_->bind(state); return self(); }

    ListView&& bind(wui::State<int>& state) && { node_->bind(state); return std::move(self()); }
    ListView& onSelectionChanged(std::function<void(int)> handler) & { node_->onSelectionChanged(std::move(handler)); return self(); }

    ListView&& onSelectionChanged(std::function<void(int)> handler) && { node_->onSelectionChanged(std::move(handler)); return std::move(self()); }
};

} // namespace wui
