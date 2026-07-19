#include "wui/table.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_set>

#include "wui/theme.h"

namespace wui {
namespace {
constexpr int kEnter = 13, kSpace = 32, kHome = 36, kEnd = 35;
constexpr int kLeft = 37, kUp = 38, kRight = 39, kDown = 40;

bool focused(const ControlNode& control) noexcept
{
    return (control.visualStates() & toMask(ControlVisualState::Focused)) != 0;
}

float textWidth(const std::string& value, float size) noexcept
{
    return std::max(1.0f, static_cast<float>(value.size()) * size * .55f);
}
} // namespace

Table::Table(std::vector<TableColumn> columns) : columns_(std::move(columns)) {}
Table& Table::setColumns(std::vector<TableColumn> value) { columns_ = std::move(value); markDirty(DirtyFlag::Layout); return *this; }
Table& Table::addColumn(TableColumn value) { columns_.push_back(std::move(value)); markDirty(DirtyFlag::Layout); return *this; }
const std::vector<TableColumn>& Table::columns() const noexcept { return columns_; }
Table& Table::setRows(std::vector<TableRow> value) { rows_ = std::move(value); setScrollOffset(scrollOffset_); markDirty(DirtyFlag::Layout); return *this; }
Table& Table::addRow(TableRow value) { rows_.push_back(std::move(value)); markDirty(DirtyFlag::Layout); return *this; }
Table& Table::clearRows() { rows_.clear(); scrollOffset_ = 0; markDirty(DirtyFlag::Layout); return *this; }
const std::vector<TableRow>& Table::rows() const noexcept { return rows_; }
Table& Table::accessibleLabel(std::string value) { setAccessibleLabel(std::move(value)); return *this; }
void Table::setAccessibleLabel(std::string value) { accessibleLabel_ = std::move(value); markDirty(DirtyFlag::Style); }
const std::string& Table::accessibleLabel() const noexcept { return accessibleLabel_; }
Table& Table::maxVisibleRows(std::size_t value) noexcept { maxVisibleRows_ = std::max<std::size_t>(1, value); setScrollOffset(scrollOffset_); markDirty(DirtyFlag::Layout); return *this; }
std::size_t Table::maxVisibleRows() const noexcept { return maxVisibleRows_; }
float Table::scrollOffset() const noexcept { return scrollOffset_; }
void Table::setScrollOffset(float value) noexcept { scrollOffset_ = std::clamp(value, 0.0f, maximumScrollOffset()); markDirty(DirtyFlag::Paint); }
float Table::maximumScrollOffset() const noexcept { return std::max(0.0f, static_cast<float>(rows_.size() > maxVisibleRows_ ? rows_.size() - maxVisibleRows_ : 0) * rowHeight()); }
std::size_t Table::firstVisibleRow() const noexcept { return std::min(rows_.size(), static_cast<std::size_t>(std::floor(scrollOffset_ / rowHeight()))); }
std::size_t Table::lastVisibleRowExclusive() const noexcept { return std::min(rows_.size(), firstVisibleRow() + maxVisibleRows_ + 1); }

std::vector<TableAccessibilityEntry> Table::accessibilityEntries() const
{
    std::vector<TableAccessibilityEntry> result;
    const auto widths = columnWidths();
    const std::string tableId = accessibilityId().empty() ? "table" : accessibilityId();
    const bool grid = dynamic_cast<const DataGrid*>(this) != nullptr;
    const bool gridFocused = grid && focused(*this);
    result.reserve(columns_.size() + (lastVisibleRowExclusive() - firstVisibleRow()) * (columns_.size() + 1));

    float x = bounds().x;
    for (std::size_t column = 0; column < columns_.size(); ++column) {
        TableAccessibilityEntry entry;
        entry.kind = TableAccessibilityKind::ColumnHeader;
        entry.column = column;
        const std::string columnId = columns_[column].id.empty() ? std::to_string(column) : columns_[column].id;
        entry.stableId = tableId + ".header." + columnId;
        entry.properties.automationId = entry.stableId;
        // Text is the portable fallback for a static column header. An
        // interactive sortable header is exposed as a Button until the
        // Windows adapter maps TableAccessibilityKind to HeaderItem.
        entry.properties.role = grid && columns_[column].sortable ? AccessibilityRole::Button : AccessibilityRole::Text;
        entry.properties.label = columns_[column].label;
        entry.properties.bounds = RectF{x, bounds().y, widths[column], std::min(headerHeight(), bounds().height)};
        const TableSortDirection direction = columnSortDirection(column);
        if (direction != TableSortDirection::None) {
            entry.properties.value = direction == TableSortDirection::Ascending ? "ascending" : "descending";
            entry.properties.description = "Sorted " + *entry.properties.value;
        }
        entry.properties.actions.invoke = grid && columns_[column].sortable;
        result.push_back(std::move(entry));
        x += widths[column];
    }

    const RectF viewport{bounds().x, bounds().y + headerHeight(), bounds().width,
                         std::max(0.0f, bounds().height - headerHeight())};
    for (std::size_t row = firstVisibleRow(); row < lastVisibleRowExclusive(); ++row) {
        const RectF untrimmed = rowBounds(row);
        const float top = std::max(untrimmed.y, viewport.y);
        const float bottom = std::min(untrimmed.y + untrimmed.height, viewport.y + viewport.height);
        if (bottom <= top) continue;
        const RectF visibleRow{bounds().x, top, bounds().width, bottom - top};
        const std::string rowId = rows_[row].id.empty() ? std::to_string(row) : rows_[row].id;
        const bool selected = isRowSelected(row);
        bool rowFocused = false;
        for (std::size_t column = 0; column < columns_.size(); ++column) {
            if (isCellFocused(row, column)) { rowFocused = true; break; }
        }

        TableAccessibilityEntry rowEntry;
        rowEntry.kind = TableAccessibilityKind::Row;
        rowEntry.row = row;
        rowEntry.stableId = tableId + ".row." + rowId;
        rowEntry.properties.automationId = rowEntry.stableId;
        rowEntry.properties.role = AccessibilityRole::ListItem;
        rowEntry.properties.label = rowId;
        rowEntry.properties.enabled = isEnabled() && rows_[row].enabled;
        rowEntry.properties.checked = grid ? std::optional<bool>(selected) : std::nullopt;
        rowEntry.properties.focused = gridFocused && rowFocused;
        rowEntry.properties.bounds = visibleRow;
        rowEntry.properties.actions.invoke = grid && rowEntry.properties.enabled;
        result.push_back(std::move(rowEntry));

        x = bounds().x;
        for (std::size_t column = 0; column < columns_.size(); ++column) {
            TableAccessibilityEntry cell;
            cell.kind = TableAccessibilityKind::Cell;
            cell.row = row; cell.column = column;
            const std::string columnId = columns_[column].id.empty() ? std::to_string(column) : columns_[column].id;
            cell.stableId = tableId + ".row." + rowId + ".cell." + columnId;
            cell.properties.automationId = cell.stableId;
            cell.properties.role = AccessibilityRole::Text;
            cell.properties.label = column < rows_[row].cells.size() ? rows_[row].cells[column] : std::string{};
            cell.properties.description = columns_[column].label;
            cell.properties.enabled = isEnabled() && rows_[row].enabled;
            cell.properties.focused = gridFocused && isCellFocused(row, column);
            cell.properties.bounds = RectF{x, top, widths[column], bottom - top};
            result.push_back(std::move(cell));
            x += widths[column];
        }
    }
    return result;
}
float Table::headerHeight() const noexcept { return 36.0f; }
float Table::rowHeight() const noexcept { return std::max(40.0f, theme().controls.height); }

std::vector<float> Table::columnWidths() const
{
    std::vector<float> result(columns_.size(), 0.0f);
    if (columns_.empty()) return result;
    float fixed = 0.0f; std::size_t flexible = 0;
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].width > 0) { result[i] = std::max(columns_[i].minWidth, columns_[i].width); fixed += result[i]; }
        else ++flexible;
    }
    const float available = std::max(0.0f, bounds().width - fixed);
    const float share = flexible ? available / static_cast<float>(flexible) : 0.0f;
    for (std::size_t i = 0; i < columns_.size(); ++i) if (columns_[i].width <= 0) result[i] = std::max(columns_[i].minWidth, share);
    // A table should never silently paint a trailing column outside its
    // viewport. Fluent's responsive table contract keeps the column model but
    // proportionally fits narrow surfaces; individual cell clips then make
    // truncation deterministic rather than allowing text to bleed into the
    // next semantic column. Applications needing horizontal navigation can
    // put the table in ScrollView without changing this stable base policy.
    const float required = std::accumulate(result.begin(), result.end(), 0.0f);
    if (required > bounds().width && required > 0.0f) {
        const float scale = std::max(0.0f, bounds().width) / required;
        for (float& width : result) width *= scale;
    }
    return result;
}
SizeF Table::measure(const Constraints& constraints) const
{
    const float natural = std::max(160.0f, std::accumulate(columns_.begin(), columns_.end(), 0.0f,
        [](float width, const TableColumn& column) { return width + std::max(column.minWidth, column.width); }));
    const float height = headerHeight() + rowHeight() * static_cast<float>(std::min(rows_.size(), maxVisibleRows_));
    return constraints.clamp({natural, height});
}
void Table::layout(const RectF& rect) { Node::layout(rect); setScrollOffset(scrollOffset_); clearLayoutDirtyRecursively(); }
RectF Table::rowBounds(std::size_t row) const noexcept
{
    return {bounds().x, bounds().y + headerHeight() + static_cast<float>(row) * rowHeight() - scrollOffset_, bounds().width, rowHeight()};
}
int Table::columnAt(PointF point) const noexcept
{
    if (!bounds().contains(point)) return -1;
    float x = bounds().x;
    const auto widths = columnWidths();
    for (std::size_t i = 0; i < widths.size(); ++i) { x += widths[i]; if (point.x <= x) return static_cast<int>(i); }
    return -1;
}
int Table::rowAt(PointF point) const noexcept
{
    if (point.y < bounds().y + headerHeight() || point.y >= bounds().y + bounds().height) return -1;
    const int row = static_cast<int>(std::floor((point.y - bounds().y - headerHeight() + scrollOffset_) / rowHeight()));
    return row >= 0 && static_cast<std::size_t>(row) < rows_.size() ? row : -1;
}
void Table::scrollRowIntoView(std::size_t row) noexcept
{
    if (row >= rows_.size()) return;
    const float top = static_cast<float>(row) * rowHeight();
    const float bottom = top + rowHeight();
    const float extent = static_cast<float>(maxVisibleRows_) * rowHeight();
    if (top < scrollOffset_) setScrollOffset(top);
    else if (bottom > scrollOffset_ + extent) setScrollOffset(bottom - extent);
}
bool Table::isRowSelected(std::size_t) const noexcept { return false; }
bool Table::isCellFocused(std::size_t, std::size_t) const noexcept { return false; }
TableSortDirection Table::columnSortDirection(std::size_t) const noexcept { return TableSortDirection::None; }
void Table::paintRowDecoration(PaintContext&, std::size_t, const RectF&) const {}

void Table::paint(PaintContext& context)
{
    const auto& current = theme(); const RectF rect = bounds();
    if (rect.width <= 0 || rect.height <= 0) return;
    context.fillRoundRect(rect, current.radius.medium, current.colors.neutralBackground1.rest);
    const RectF header{rect.x, rect.y, rect.width, std::min(headerHeight(), rect.height)};
    context.fillRoundRect(header, current.radius.medium, current.colors.neutralBackground2.rest);
    const auto widths = columnWidths(); float x = rect.x;
    for (std::size_t c = 0; c < columns_.size(); ++c) {
        const auto& col = columns_[c]; const float w = widths[c];
        std::string label = col.label;
        const auto direction = columnSortDirection(c);
        if (direction == TableSortDirection::Ascending) label += "  ^";
        else if (direction == TableSortDirection::Descending) label += "  v";
        const auto& style = current.typography.caption1Strong;
        float tx = x + current.spacing.horizontal.m;
        const float labelWidth = textWidth(label, style.size);
        if (col.alignment == TableColumnAlignment::Center) tx = x + std::max(0.0f, (w - labelWidth) * .5f);
        else if (col.alignment == TableColumnAlignment::End) tx = x + std::max(0.0f, w - labelWidth - current.spacing.horizontal.m);
        const int cellClip = context.save();
        context.clipRect({x + 1.0f, header.y, std::max(0.0f, w - 2.0f), header.height});
        context.drawText(label, tx, context.centeredTextBottom(label, {x, header.y, w, header.height}, style.size, style.weight), style.size,
                         current.colors.neutralForeground2, style.weight, style.family);
        context.restoreTo(cellClip);
        x += w;
    }
    context.fillRect({rect.x, header.y + header.height - current.stroke.thin, rect.width, current.stroke.thin}, current.colors.neutralStroke1);
    const int clip = context.save(); context.clipRect({rect.x, rect.y + header.height, rect.width, std::max(0.0f, rect.height - header.height)});
    for (std::size_t r = firstVisibleRow(); r < lastVisibleRowExclusive(); ++r) {
        const RectF row = rowBounds(r); if (row.y >= rect.y + rect.height) break;
        paintRowDecoration(context, r, row);
        float cellX = rect.x;
        for (std::size_t c = 0; c < columns_.size(); ++c) {
            const float w = widths[c]; const std::string text = c < rows_[r].cells.size() ? rows_[r].cells[c] : std::string{};
            const auto& style = current.typography.body1;
            float tx = cellX + current.spacing.horizontal.m;
            const float cellWidth = textWidth(text, style.size);
            if (columns_[c].alignment == TableColumnAlignment::Center) tx = cellX + std::max(0.0f, (w - cellWidth) * .5f);
            else if (columns_[c].alignment == TableColumnAlignment::End) tx = cellX + std::max(0.0f, w - cellWidth - current.spacing.horizontal.m);
            const Color fg = rows_[r].enabled ? current.colors.neutralForeground1 : current.colors.neutralForegroundDisabled;
            const int cellClip = context.save();
            context.clipRect({cellX + 1.0f, row.y, std::max(0.0f, w - 2.0f), row.height});
            context.drawText(text, tx, context.centeredTextBottom(text, {cellX, row.y, w, row.height}, style.size, style.weight), style.size, fg, style.weight, style.family);
            context.restoreTo(cellClip);
            if (isCellFocused(r, c) && focused(*this)) context.strokeRoundRect({cellX + 2, row.y + 2, std::max(0.0f, w - 4), std::max(0.0f, row.height - 4)}, current.radius.small, current.controls.focusWidth, current.colors.strokeFocusInner);
            cellX += w;
        }
        context.fillRect({rect.x, row.y + row.height - current.stroke.thin, rect.width, current.stroke.thin}, current.colors.neutralStroke1);
    }
    context.restoreTo(clip);
    if (focused(*this)) context.strokeRoundRect({rect.x + current.controls.focusInset, rect.y + current.controls.focusInset, rect.width - current.controls.focusInset * 2, rect.height - current.controls.focusInset * 2}, current.radius.medium, current.controls.focusWidth, current.colors.strokeFocusInner);
    clearDirty(DirtyFlag::Paint);
}
bool Table::onPointerEvent(const PointerEvent& event)
{
    if (!isEnabled() || !bounds().contains(event.position)) return false;
    if (event.action == PointerAction::Scroll) { setScrollOffset(scrollOffset_ - event.scrollDelta.y); return true; }
    return false;
}

DataGrid& DataGrid::selectionMode(DataGridSelectionMode value) noexcept { selectionMode_ = value; if (value == DataGridSelectionMode::None) selectedRows_.clear(); else normalizeState(); markDirty(DirtyFlag::Paint); return *this; }
DataGridSelectionMode DataGrid::selectionMode() const noexcept { return selectionMode_; }
DataGrid& DataGrid::selectedRows(std::vector<std::size_t> value) { selectedRows_ = std::move(value); normalizeState(); markDirty(DirtyFlag::Paint); return *this; }
const std::vector<std::size_t>& DataGrid::selectedRows() const noexcept { return selectedRows_; }
DataGrid& DataGrid::onSelectionChanged(SelectionHandler handler) { onSelectionChanged_ = std::move(handler); return *this; }
DataGrid& DataGrid::onSort(SortHandler handler) { onSort_ = std::move(handler); return *this; }
std::optional<std::size_t> DataGrid::sortColumn() const noexcept { return sortColumn_; }
TableSortDirection DataGrid::sortDirection() const noexcept { return sortDirection_; }
void DataGrid::normalizeState()
{
    selectedRows_.erase(std::remove_if(selectedRows_.begin(), selectedRows_.end(), [this](std::size_t index) { return index >= rows_.size() || !rows_[index].enabled; }), selectedRows_.end());
    std::sort(selectedRows_.begin(), selectedRows_.end()); selectedRows_.erase(std::unique(selectedRows_.begin(), selectedRows_.end()), selectedRows_.end());
    if (selectionMode_ == DataGridSelectionMode::Single && selectedRows_.size() > 1) selectedRows_.resize(1);
    if (focusedRow_ >= rows_.size()) focusedRow_ = rows_.empty() ? 0 : rows_.size() - 1;
    if (focusedColumn_ >= columns_.size()) focusedColumn_ = columns_.empty() ? 0 : columns_.size() - 1;
}
void DataGrid::sortBy(std::size_t column)
{
    if (column >= columns_.size() || !columns_[column].sortable) return;
    if (sortColumn_ != column) { sortColumn_ = column; sortDirection_ = TableSortDirection::Ascending; }
    else sortDirection_ = sortDirection_ == TableSortDirection::Ascending ? TableSortDirection::Descending : TableSortDirection::Ascending;
    if (onSort_) onSort_(column, sortDirection_);
    else {
        std::vector<std::string> selectedIds; for (const auto index : selectedRows_) if (index < rows_.size()) selectedIds.push_back(rows_[index].id);
        const auto direction = sortDirection_;
        std::stable_sort(rows_.begin(), rows_.end(), [column, direction](const TableRow& left, const TableRow& right) {
            const std::string& a = column < left.cells.size() ? left.cells[column] : std::string{};
            const std::string& b = column < right.cells.size() ? right.cells[column] : std::string{};
            return direction == TableSortDirection::Ascending ? a < b : a > b;
        });
        selectedRows_.clear(); for (std::size_t i = 0; i < rows_.size(); ++i) if (std::find(selectedIds.begin(), selectedIds.end(), rows_[i].id) != selectedIds.end()) selectedRows_.push_back(i);
    }
    normalizeState(); markDirty(DirtyFlag::Paint);
}
std::size_t DataGrid::focusedRow() const noexcept { return focusedRow_; }
std::size_t DataGrid::focusedColumn() const noexcept { return focusedColumn_; }
void DataGrid::selectRow(std::size_t row, bool toggle)
{
    if (selectionMode_ == DataGridSelectionMode::None || row >= rows_.size() || !rows_[row].enabled) return;
    if (selectionMode_ == DataGridSelectionMode::Single) selectedRows_ = {row};
    else {
        const auto it = std::find(selectedRows_.begin(), selectedRows_.end(), row);
        if (toggle && it != selectedRows_.end()) selectedRows_.erase(it); else if (it == selectedRows_.end()) selectedRows_.push_back(row);
    }
    normalizeState(); if (onSelectionChanged_) onSelectionChanged_(selectedRows_); markDirty(DirtyFlag::Paint);
}
bool DataGrid::moveFocus(int rowDelta, int columnDelta)
{
    if (rows_.empty() || columns_.empty()) return false;
    focusedRow_ = static_cast<std::size_t>(std::clamp(static_cast<int>(focusedRow_) + rowDelta, 0, static_cast<int>(rows_.size()) - 1));
    focusedColumn_ = static_cast<std::size_t>(std::clamp(static_cast<int>(focusedColumn_) + columnDelta, 0, static_cast<int>(columns_.size()) - 1));
    scrollRowIntoView(focusedRow_); setVisualState(ControlVisualState::Focused, true); markDirty(DirtyFlag::Paint); return true;
}
bool DataGrid::onPointerEvent(const PointerEvent& event)
{
    if (Table::onPointerEvent(event)) return true;
    if (!isEnabled() || !bounds().contains(event.position) || event.action != PointerAction::Up || event.button != MouseButton::Left) return false;
    const int column = columnAt(event.position); if (column < 0) return false;
    if (event.position.y < bounds().y + headerHeight()) { sortBy(static_cast<std::size_t>(column)); return columns_[static_cast<std::size_t>(column)].sortable; }
    const int row = rowAt(event.position); if (row < 0) return false;
    focusedRow_ = static_cast<std::size_t>(row); focusedColumn_ = static_cast<std::size_t>(column); setVisualState(ControlVisualState::Focused, true);
    selectRow(static_cast<std::size_t>(row), selectionMode_ == DataGridSelectionMode::Multiple && (event.modifiers & KeyModifierControl)); return true;
}
bool DataGrid::onKeyEvent(const KeyEvent& event)
{
    if (!isEnabled() || event.action != KeyAction::Down) return false;
    switch (event.keyCode) {
    case kUp: return moveFocus(-1, 0); case kDown: return moveFocus(1, 0); case kLeft: return moveFocus(0, -1); case kRight: return moveFocus(0, 1);
    case kHome: if (event.modifiers & KeyModifierControl) focusedRow_ = 0; else focusedColumn_ = 0; scrollRowIntoView(focusedRow_); markDirty(DirtyFlag::Paint); return true;
    case kEnd: if (event.modifiers & KeyModifierControl) focusedRow_ = rows_.empty() ? 0 : rows_.size() - 1; else focusedColumn_ = columns_.empty() ? 0 : columns_.size() - 1; scrollRowIntoView(focusedRow_); markDirty(DirtyFlag::Paint); return true;
    case kEnter: case kSpace: if (!rows_.empty()) { selectRow(focusedRow_, selectionMode_ == DataGridSelectionMode::Multiple && event.keyCode == kSpace); return true; } return false;
    default: return false;
    }
}
AccessibilityActionCapabilities DataGrid::accessibilityActions() const noexcept { AccessibilityActionCapabilities actions; actions.setValue = true; return actions; }
AccessibilityActionStatus DataGrid::performAccessibilityAction(AccessibilityActionKind kind, std::string_view value)
{
    if (!isEnabled()) return AccessibilityActionStatus::ElementNotEnabled;
    if (kind != AccessibilityActionKind::SetValue) return AccessibilityActionStatus::NotSupported;
    try { const std::size_t row = static_cast<std::size_t>(std::stoul(std::string(value))); if (row >= rows_.size()) return AccessibilityActionStatus::InvalidValue; focusedRow_ = row; selectRow(row, false); return AccessibilityActionStatus::Succeeded; }
    catch (...) { return AccessibilityActionStatus::InvalidValue; }
}
bool DataGrid::isRowSelected(std::size_t row) const noexcept { return std::find(selectedRows_.begin(), selectedRows_.end(), row) != selectedRows_.end(); }
bool DataGrid::isCellFocused(std::size_t row, std::size_t column) const noexcept { return row == focusedRow_ && column == focusedColumn_; }
TableSortDirection DataGrid::columnSortDirection(std::size_t column) const noexcept { return sortColumn_ && *sortColumn_ == column ? sortDirection_ : TableSortDirection::None; }
void DataGrid::paintRowDecoration(PaintContext& context, std::size_t row, const RectF& rect) const
{
    const auto& current = theme();
    if (isRowSelected(row)) context.fillRect(rect, current.colors.neutralBackground1.selected);
    else if (row == focusedRow_ && focused(*this)) context.fillRect(rect, current.colors.neutralBackground1.hover);
}
} // namespace wui
