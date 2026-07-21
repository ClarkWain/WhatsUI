# Fluent Table and DataGrid

`Table` and `DataGrid` share value-based `TableColumn` and `TableRow` data.
This keeps columns, cells, and row identities stable even when an application
replaces a large data set; widgets do not create one retained `Node` per cell.

## Table

`Table` is passive tabular content. It owns header/body layout, column
alignment, disabled-row presentation, and an explicit windowed viewport via
`maxVisibleRows()` / `scrollOffset()`. It intentionally has no selection,
sort, or keyboard-grid behaviour. Use it for comparisons and read-only data.
On a narrow surface, all declared columns are proportionally fitted into the
viewport and each header/cell is clipped to its own column. A trailing column
therefore never falls outside the visual or semantic surface, and long text
cannot bleed across the next column. Put a table in `ScrollView` when an
application instead needs horizontal exploration of intrinsically wide data.

```cpp
wui::Table release({{"name", "Name", 180, 80}, {"state", "State"}});
release.setRows({{"v1", {"WhatsUI", "Ready"}}}).maxVisibleRows(8);
```

## DataGrid

`DataGrid` is the interactive counterpart. Its roving cell focus uses arrows,
Home/End (Control+Home/End for first/final row), and scrolls the row window as
focus crosses the viewport. Enter/Space selects the focused enabled row.

- Mark a `TableColumn` as `sortable` to permit header pointer sorting.
- Default sorting is stable UTF-8 byte-order cell text, avoiding hidden OS
  locale behaviour. `onSort(column, direction)` delegates sorting to an
  application model when locale-aware or server sorting is needed.
- Selection is none, single (default), or multiple. Rows retain `id` so the
  internal default sort restores selected rows by identity rather than old
  indexes.
- Disabled rows are visible and navigable for review but cannot be selected.
- `SetValue("row-index")` exposes deterministic selection to generic
  accessibility adapters until virtual row/cell snapshot children are added.

```cpp
wui::DataGrid grid;
grid.setColumns({{"task", "Task", 220, 100, wui::TableColumnAlignment::Start, true},
                 {"state", "State", 0, 90, wui::TableColumnAlignment::Center, true}})
    .setRows(rows)
    .maxVisibleRows(10);
grid.selectionMode(wui::DataGridSelectionMode::Multiple);
grid.onSort([](std::size_t column, wui::TableSortDirection direction) {
    // Sort the application model, then call grid.setRows(...).
});
```

## Integration still required

`accessibilityEntries()` now materializes a virtual Header/Row/Cell tree for
the header and only the rendered row window. Each entry has a stable
`table.header.column-id`, `table.row.row-id`, or
`table.row.row-id.cell.column-id` automation id; it includes clipped bounds,
column association, header sort direction, disabled state, selected rows, and
the one roving focused grid cell. This avoids fake retained children for
thousands of cells.

The central accessibility bridge must append those entries below the owning
Table/DataGrid snapshot using a reserved virtual-path marker (as ListBox does
for virtual options). It must map `TableAccessibilityKind` to native
ColumnHeader/Row/Cell roles, and route virtual header Invoke to
`DataGrid::sortBy(column)`, plus virtual row/cell Invoke or SetValue to the
owning grid's row selection API. The generic visual-path dispatcher cannot
resolve virtual descendants by itself.

Behaviour and Software visual test sources are `fluent_table_tests.cpp` and
`fluent_table_visual_tests.cpp`; the visual matrix renders at 100%, 125%,
150%, and 200% through the central CMake test manifest.
