#pragma once

// Fluent 2 tabular collections. Table is deliberately passive: it renders
// stable column/row data with no selection or sorting side effects. DataGrid
// builds on the same model and adds the interactive grid contract (sorting,
// row selection, roving keyboard focus and a windowed viewport).

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "wui/node.h"

namespace wui {

enum class TableColumnAlignment { Start, Center, End };
enum class TableSortDirection { None, Ascending, Descending };
enum class DataGridSelectionMode { None, Single, Multiple };

struct TableColumn {
    std::string id;
    std::string label;
    float width{0.0f};          // zero means a deterministic equal share.
    float minWidth{88.0f};
    TableColumnAlignment alignment{TableColumnAlignment::Start};
    bool sortable{false};
};

struct TableRow {
    std::string id;
    std::vector<std::string> cells;
    bool enabled{true};
};

// A materialized table semantic represents a virtual descendant rather than a
// retained Node. Only the header and rows in the current viewport are emitted
// so large data sets do not turn into thousands of inert accessibility nodes.
// `stableId` derives from the table, row, and column ids and survives sorting
// and window changes; platform bridges use `kind` to map to their native
// column-header/row/cell roles.
enum class TableAccessibilityKind { ColumnHeader, Row, Cell };

struct TableAccessibilityEntry {
    TableAccessibilityKind kind{TableAccessibilityKind::Cell};
    std::optional<std::size_t> row;
    std::optional<std::size_t> column;
    std::string stableId;
    AccessibilityProperties properties{};
};

// A passive semantic table. Its data is value based so applications may
// replace, sort, or window external data without rebuilding Node ownership.
class Table : public ControlNode {
public:
    Table() = default;
    explicit Table(std::vector<TableColumn> columns);

    Table& setColumns(std::vector<TableColumn> value);
    Table& addColumn(TableColumn value);
    [[nodiscard]] const std::vector<TableColumn>& columns() const noexcept;
    Table& setRows(std::vector<TableRow> value);
    Table& addRow(TableRow value);
    Table& clearRows();
    [[nodiscard]] const std::vector<TableRow>& rows() const noexcept;
    Table& accessibleLabel(std::string value);
    void setAccessibleLabel(std::string value);
    [[nodiscard]] const std::string& accessibleLabel() const noexcept;
    Table& maxVisibleRows(std::size_t value) noexcept;
    [[nodiscard]] std::size_t maxVisibleRows() const noexcept;
    [[nodiscard]] float scrollOffset() const noexcept;
    void setScrollOffset(float value) noexcept;
    [[nodiscard]] float maximumScrollOffset() const noexcept;
    [[nodiscard]] std::size_t firstVisibleRow() const noexcept;
    [[nodiscard]] std::size_t lastVisibleRowExclusive() const noexcept;
    // Header plus the visible window's row and cell semantics. The fallback
    // properties use portable roles (Text/ListItem/Button); a platform bridge
    // should map `TableAccessibilityKind` to its native Table/Grid roles.
    [[nodiscard]] std::vector<TableAccessibilityEntry> accessibilityEntries() const;

    [[nodiscard]] SizeF measure(const Constraints& constraints) const override;
    void layout(const RectF& bounds) override;
    void paint(PaintContext& context) override;
    bool onPointerEvent(const PointerEvent& event) override;

protected:
    [[nodiscard]] float headerHeight() const noexcept;
    [[nodiscard]] float rowHeight() const noexcept;
    [[nodiscard]] std::vector<float> columnWidths() const;
    [[nodiscard]] int columnAt(PointF point) const noexcept;
    [[nodiscard]] int rowAt(PointF point) const noexcept;
    [[nodiscard]] RectF rowBounds(std::size_t row) const noexcept;
    void scrollRowIntoView(std::size_t row) noexcept;
    virtual bool isRowSelected(std::size_t row) const noexcept;
    virtual bool isCellFocused(std::size_t row, std::size_t column) const noexcept;
    virtual TableSortDirection columnSortDirection(std::size_t column) const noexcept;
    virtual void paintRowDecoration(PaintContext& context, std::size_t row, const RectF& rect) const;

    std::vector<TableColumn> columns_;
    std::vector<TableRow> rows_;
    std::string accessibleLabel_{"Table"};
    std::size_t maxVisibleRows_{8};
    float scrollOffset_{0.0f};
};

// Interactive table semantics. Sorting is deterministic by cell text unless
// `onSort` is supplied; the callback can then sort an application-owned model
// while the grid retains its requested column/direction state.
class DataGrid final : public Table {
public:
    using SortHandler = std::function<void(std::size_t, TableSortDirection)>;
    using SelectionHandler = std::function<void(const std::vector<std::size_t>&)>;

    DataGrid& selectionMode(DataGridSelectionMode value) noexcept;
    [[nodiscard]] DataGridSelectionMode selectionMode() const noexcept;
    DataGrid& selectedRows(std::vector<std::size_t> value);
    [[nodiscard]] const std::vector<std::size_t>& selectedRows() const noexcept;
    DataGrid& onSelectionChanged(SelectionHandler handler);
    DataGrid& onSort(SortHandler handler);
    [[nodiscard]] std::optional<std::size_t> sortColumn() const noexcept;
    [[nodiscard]] TableSortDirection sortDirection() const noexcept;
    void sortBy(std::size_t column);
    [[nodiscard]] std::size_t focusedRow() const noexcept;
    [[nodiscard]] std::size_t focusedColumn() const noexcept;

    bool onPointerEvent(const PointerEvent& event) override;
    bool onKeyEvent(const KeyEvent& event) override;
    [[nodiscard]] AccessibilityActionCapabilities accessibilityActions() const noexcept override;
    AccessibilityActionStatus performAccessibilityAction(AccessibilityActionKind kind,
                                                          std::string_view value) override;

protected:
    bool isRowSelected(std::size_t row) const noexcept override;
    bool isCellFocused(std::size_t row, std::size_t column) const noexcept override;
    TableSortDirection columnSortDirection(std::size_t column) const noexcept override;
    void paintRowDecoration(PaintContext& context, std::size_t row, const RectF& rect) const override;

private:
    void selectRow(std::size_t row, bool toggle);
    bool moveFocus(int rowDelta, int columnDelta);
    void normalizeState();
    DataGridSelectionMode selectionMode_{DataGridSelectionMode::Single};
    std::vector<std::size_t> selectedRows_;
    std::optional<std::size_t> sortColumn_;
    TableSortDirection sortDirection_{TableSortDirection::None};
    std::size_t focusedRow_{0};
    std::size_t focusedColumn_{0};
    SelectionHandler onSelectionChanged_;
    SortHandler onSort_;
};

} // namespace wui
