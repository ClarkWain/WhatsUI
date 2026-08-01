#include <iostream>
#include <memory>
#include <stdexcept>

#include "wui/accessibility.h"
#include "wui/app.h"
#include "wui/platform.h"
#include "wui/table.h"
#include "wui/widgets.h"

namespace {
void expect(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
class InspectTable final : public wui::TableNode {
public:
    using wui::TableNode::TableNode;
    using wui::TableNode::columnWidths;
    using wui::TableNode::columnAt;
    using wui::TableNode::headerHeight;
    using wui::TableNode::rowHeight;
};

class TestSurface final : public wui::RenderSurface {
public:
    [[nodiscard]] wui::CanvasBackend backend() const noexcept override { return wui::CanvasBackend::Software; }
    [[nodiscard]] wui::SizeF framebufferSize() const noexcept override { return {640, 480}; }
    void beginFrame() override {} void endFrame() override {} void resize(wui::SizeF) override {}
};
class TestClipboard final : public wui::Clipboard {
public:
    void setText(std::string_view value) override { value_ = value; }
    [[nodiscard]] std::string getText() const override { return value_; }
    [[nodiscard]] bool hasText() const override { return !value_.empty(); }
private:
    std::string value_;
};
class TestCursor final : public wui::CursorService { public: void setCursor(wui::CursorIcon) override {} };
class TestTextInput final : public wui::TextInputSession {
public:
    void activate() override {} void deactivate() override {} void setCaretRect(const wui::RectF&) override {}
    void setSurroundingText(std::string_view, std::size_t, std::size_t) override {}
};
class TestWindow final : public wui::PlatformWindow {
public:
    [[nodiscard]] wui::WindowId id() const noexcept override { return 73; }
    [[nodiscard]] wui::WindowMetrics metrics() const noexcept override { return {{640, 480}, {640, 480}, 1}; }
    void show() override {} void close() override {} [[nodiscard]] bool isOpen() const noexcept override { return true; }
    [[nodiscard]] bool isFocused() const noexcept override { return true; }
    void setTitle(std::string_view) override {} void requestRedraw() override { ++redraws; }
    [[nodiscard]] wui::RenderSurface& surface() override { return surface_; }
    [[nodiscard]] wui::Clipboard& clipboard() override { return clipboard_; }
    [[nodiscard]] wui::CursorService& cursor() override { return cursor_; }
    [[nodiscard]] wui::TextInputSession& textInput() override { return text_; }
    int redraws{0};
private:
    TestSurface surface_; TestClipboard clipboard_; TestCursor cursor_; TestTextInput text_;
};

std::vector<wui::TableColumn> columns()
{
    return {{"name", "Name", 180.0f, 80.0f, wui::TableColumnAlignment::Start, true},
            {"status", "Status", 0.0f, 80.0f, wui::TableColumnAlignment::Center, true},
            {"owner", "Owner", 0.0f, 80.0f, wui::TableColumnAlignment::End, false}};
}
std::vector<wui::TableRow> rows()
{
    return {{"a", {"Zebra", "Open", "Adele"}}, {"b", {"Alpha", "Closed", "Bert"}},
            {"c", {"Bravo", "Open", "Cleo"}}, {"d", {"Delta", "Open", "Dora"}},
            {"disabled", {"Zulu", "Blocked", "Nobody"}, false}};
}
void testPassiveTableWindowing()
{
    InspectTable table(columns()); table.setRows(rows()).maxVisibleRows(2).accessibleLabel("Release table");
    expect(table.headerHeight() == 32.0f && table.rowHeight() == 44.0f,
           "Medium Table must use a 32-DIP header and Figma 9249:10060 44-DIP cell content");
    table.layout({0, 0, 440, 116});
        expect(table.accessibleLabel() == "Release table" && table.firstVisibleRow() == 0 && table.lastVisibleRowExclusive() == 2,
            "Table must retain a stable passive data model and a true visible row window");
    expect(table.maximumScrollOffset() > 0.0f, "Table must calculate a deterministic viewport offset for oversized static data");
    table.onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Scroll, wui::MouseButton::None, {30, 80}, 0, {0, -100}});
    expect(table.scrollOffset() > 0.0f && table.firstVisibleRow() > 0,
           "Table must scroll only its windowed rows while preserving its header");
    expect(!table.onKeyEvent({0, wui::KeyAction::Down, 40}),
           "Passive Table must not silently acquire interactive DataGrid keyboard semantics");
    table.layout({0, 0, 150, 116});
    float widthSum = 0.0f; for (const float width : table.columnWidths()) widthSum += width;
    expect(widthSum <= 150.01f && table.columnAt({149, 18}) == 2,
           "Narrow tables must proportionally fit every declared column instead of clipping trailing semantic columns");
}
void testGridSortSelectionAndKeyboard()
{
    wui::DataGridNode grid; grid.setColumns(columns()).setRows(rows()).maxVisibleRows(2);
    grid.layout({0, 0, 440, 120});
    int selectionChanged = 0;
    grid.onSelectionChanged([&](const std::vector<std::size_t>&) { ++selectionChanged; });
    grid.sortBy(0);
    expect(grid.sortColumn() && *grid.sortColumn() == 0 && grid.sortDirection() == wui::TableSortDirection::Ascending &&
               grid.rows().front().id == "b", "DataGrid must provide deterministic sortable column state and default stable text sorting");
    expect(grid.onKeyEvent({0, wui::KeyAction::Down, 40}) && grid.focusedRow() == 1 && grid.scrollOffset() == 0.0f,
           "DataGrid Down must move the roving cell focus without prematurely scrolling");
    expect(grid.onKeyEvent({0, wui::KeyAction::Down, 40}) && grid.focusedRow() == 2 && grid.scrollOffset() > 0.0f,
           "DataGrid keyboard focus must scroll the windowed viewport when it crosses its lower edge");
    expect(grid.onKeyEvent({0, wui::KeyAction::Down, 32}) && grid.selectedRows().size() == 1 && selectionChanged == 1,
           "DataGrid Space must select the focused enabled row through the same deterministic model used by pointer input");
    grid.selectionMode(wui::DataGridSelectionMode::Multiple);
    expect(grid.onKeyEvent({0, wui::KeyAction::Down, 38}) && grid.onKeyEvent({0, wui::KeyAction::Down, 32}) && grid.selectedRows().size() == 2,
           "Multi-select DataGrid must allow independent row selection while retaining roving focus");
    expect(grid.performAccessibilityAction(wui::AccessibilityActionKind::SetValue, "1") == wui::AccessibilityActionStatus::Succeeded,
           "DataGrid must expose deterministic row selection through its control accessibility action");
}
void testDisabledRowsAndHeaderPointerSort()
{
    wui::DataGridNode grid; grid.setColumns(columns()).setRows(rows()); grid.layout({0, 0, 440, 240});
    expect(grid.onPointerEvent({0, wui::PointerType::Mouse, wui::PointerAction::Up, wui::MouseButton::Left, {50, 18}}) &&
               grid.sortDirection() == wui::TableSortDirection::Ascending,
           "DataGrid header hit testing must sort only explicitly sortable headers");
    grid.onKeyEvent({0, wui::KeyAction::Down, 35, wui::KeyModifierControl});
    expect(grid.focusedRow() == grid.rows().size() - 1 && grid.onKeyEvent({0, wui::KeyAction::Down, 32}) && grid.selectedRows().empty(),
           "Disabled DataGrid rows must remain focusable for review but cannot be selected");
}
void testVirtualAccessibilityWindow()
{
    wui::TableNode table(columns()); table.setAccessibilityId("release-table");
    table.setRows(rows()).maxVisibleRows(1); table.layout({0, 0, 240, 76});
    const auto entries = table.accessibilityEntries();
    std::size_t headers = 0, tableRows = 0, cells = 0;
    for (const auto& entry : entries) {
        if (entry.kind == wui::TableAccessibilityKind::ColumnHeader) {
            ++headers;
            expect(entry.properties.automationId.find("release-table.header.") == 0 && entry.properties.bounds.has_value(),
                   "Virtual headers need a stable table+column identity and physical bounds");
        } else if (entry.kind == wui::TableAccessibilityKind::Row) ++tableRows;
        else ++cells;
    }
    expect(headers == columns().size() && tableRows == 1 && cells == columns().size(),
           "Table semantic materialization must include headers plus only the visible row window and its cells");

    wui::DataGridNode grid; grid.setAccessibilityId("work-grid"); grid.setColumns(columns()).setRows(rows()).maxVisibleRows(2);
    grid.layout({0, 0, 440, 116}); grid.sortBy(0);
    grid.onKeyEvent({0, wui::KeyAction::Down, 40}); grid.onKeyEvent({0, wui::KeyAction::Down, 32});
    bool sortedHeader = false, selectedRow = false, focusedCell = false;
    for (const auto& entry : grid.accessibilityEntries()) {
        if (entry.kind == wui::TableAccessibilityKind::ColumnHeader && entry.column == 0) {
            sortedHeader = entry.properties.value == std::optional<std::string>{"ascending"} && entry.properties.actions.invoke;
        } else if (entry.kind == wui::TableAccessibilityKind::Row && entry.properties.checked == std::optional<bool>{true}) {
            selectedRow = entry.properties.automationId.find("work-grid.row.") == 0;
        } else if (entry.kind == wui::TableAccessibilityKind::Cell && entry.properties.focused) focusedCell = true;
    }
    expect(sortedHeader && selectedRow && focusedCell,
           "DataGrid virtual semantics must carry sortable headers, stable selected-row state and one focused cell");
}

    void testProviderBackedGridRequestsOnlyVisibleRows()
    {
        wui::DataGridNode grid;
        int requests = 0;
        int sortCalls = 0;
        grid.setColumns(columns()).setRowProvider(100000, [&requests](std::size_t row) {
         ++requests;
         return wui::TableRow{"row-" + std::to_string(row), {"Name " + std::to_string(row), "Open", "Owner"}};
        }, [](std::size_t row) { return row != 99999; });
        grid.maxVisibleRows(2);
        grid.onSort([&](std::size_t, wui::TableSortDirection) { ++sortCalls; });
        grid.layout({0, 0, 440, 116});

        requests = 0;
        const auto entries = grid.accessibilityEntries();
        expect(!entries.empty() && requests <= 2,
            "Provider-backed DataGrid accessibility should request only the visible row window");

        requests = 0;
        grid.setScrollOffset(44.0f * 50000.0f);
        const auto scrolledEntries = grid.accessibilityEntries();
        expect(!scrolledEntries.empty() && grid.firstVisibleRow() == 50000 && requests <= 2,
            "Provider-backed DataGrid large scroll should keep row requests bounded");

        requests = 0;
        expect(grid.performAccessibilityAction(wui::AccessibilityActionKind::SetValue, "99999") == wui::AccessibilityActionStatus::Succeeded &&
                   grid.selectedRows().empty() && requests == 0,
               "Provider-backed DataGrid SetValue should use enabled metadata without requesting row payloads");

        grid.sortBy(0);
        expect(sortCalls == 1 && grid.sortColumn() && *grid.sortColumn() == 0,
            "Provider-backed DataGrid sorting should delegate to onSort instead of sorting local rows");
    }

    void testProviderBackedGridUsesPhysicalViewportHeight()
    {
        wui::DataGridNode grid;
        int requests = 0;
        grid.setColumns(columns()).setRowProvider(100000, [&requests](std::size_t row) {
            ++requests;
            return wui::TableRow{"row-" + std::to_string(row), {"Name " + std::to_string(row), "Open", "Owner"}};
        });
        grid.maxVisibleRows(8);

        grid.layout({0, 0, 440, 32});
        requests = 0;
        const auto headerOnly = grid.accessibilityEntries();
        expect(!headerOnly.empty() && grid.firstVisibleRow() == grid.lastVisibleRowExclusive() && requests == 0,
               "Provider-backed DataGrid must not request rows when the physical body viewport is empty");

        grid.layout({0, 0, 440, 76});
        requests = 0;
        const auto oneRow = grid.accessibilityEntries();
        expect(!oneRow.empty() && grid.firstVisibleRow() == 0 && grid.lastVisibleRowExclusive() == 1 && requests <= 1,
               "Provider-backed DataGrid should request only physically visible rows, independent of maxVisibleRows");
    }

void testCentralSnapshotAndVirtualActionRouting()
{
    // Verify the public UiWindow boundary, not only Table's internal semantic
    // materializer. Native adapters receive this exact snapshot and return
    // paths to this exact action router.
    wui::UiWindow window(std::make_unique<TestWindow>());
    auto root = std::make_unique<wui::BoxNode>();
    auto grid = std::make_unique<wui::DataGridNode>();
    wui::DataGridNode* raw = grid.get();
    grid->setAccessibilityId("work-grid");
    grid->accessibleLabel("Work items");
    grid->setColumns(columns()).setRows(rows()).maxVisibleRows(2);
    root->appendChild(std::move(grid));
    window.setRoot(std::move(root));
    window.layout();
    window.focusManager().setFocused(raw);

    const auto beforeSort = window.accessibilitySnapshot();
    const auto gridRoot = std::find_if(beforeSort.begin(), beforeSort.end(), [](const auto& entry) {
        return entry.properties.role == wui::AccessibilityRole::DataGrid && entry.properties.label == "Work items";
    });
    const auto nameHeader = std::find_if(beforeSort.begin(), beforeSort.end(), [](const auto& entry) {
        return entry.properties.role == wui::AccessibilityRole::ColumnHeader &&
               entry.properties.automationId == "work-grid.header.name";
    });
    const auto tableSnapshot = wui::snapshotAccessibilityTree(*raw, raw);
    bool exposedDataGridRow = false, exposedDataGridCell = false;
    for (const auto& entry : tableSnapshot) {
        exposedDataGridRow = exposedDataGridRow || entry.properties.role == wui::AccessibilityRole::DataGridRow;
        exposedDataGridCell = exposedDataGridCell || entry.properties.role == wui::AccessibilityRole::DataGridCell;
    }
    expect(gridRoot != beforeSort.end() && gridRoot->properties.actions.focus &&
               nameHeader != beforeSort.end() && nameHeader->properties.actions.invoke &&
               exposedDataGridRow && exposedDataGridCell,
           "Central accessibility snapshots must expose the interactive DataGrid root plus virtual header, row and cell roles");

    expect(window.performAccessibilityAction({wui::AccessibilityActionKind::Invoke, nameHeader->path,
                                              wui::AccessibilityRole::ColumnHeader,
                                              nameHeader->properties.automationId, nameHeader->properties.label, {}}) ==
               wui::AccessibilityActionStatus::Succeeded &&
               raw->sortColumn() == std::optional<std::size_t>{0} && raw->rows().front().id == "b",
           "A retained virtual DataGrid header path must safely route Invoke to its owning grid sort operation");

    const auto afterSort = window.accessibilitySnapshot();
    const auto selectableRow = std::find_if(afterSort.begin(), afterSort.end(), [](const auto& entry) {
        return entry.properties.role == wui::AccessibilityRole::DataGridRow && entry.properties.enabled;
    });
    expect(selectableRow != afterSort.end() && selectableRow->properties.actions.invoke &&
               window.performAccessibilityAction({wui::AccessibilityActionKind::Invoke, selectableRow->path,
                                                  wui::AccessibilityRole::DataGridRow,
                                                  selectableRow->properties.automationId, selectableRow->properties.label, {}}) ==
                   wui::AccessibilityActionStatus::Succeeded &&
               raw->selectedRows() == std::vector<std::size_t>{0},
           "A current virtual DataGrid row path must safely route Invoke to stable row selection");

    wui::TableNode passive(columns());
    passive.setRows(rows());
    passive.layout({0, 0, 440, 116});
    const auto passiveSnapshot = wui::snapshotAccessibilityTree(passive, &passive);
    expect(!passiveSnapshot.empty() && passiveSnapshot.front().properties.role == wui::AccessibilityRole::Table &&
               !passiveSnapshot.front().properties.actions.focus,
           "A passive Table must expose table semantics without claiming keyboard focusability");
}
}
int main()
{
    try { testPassiveTableWindowing(); testGridSortSelectionAndKeyboard(); testDisabledRowsAndHeaderPointerSort(); testVirtualAccessibilityWindow(); testProviderBackedGridRequestsOnlyVisibleRows(); testProviderBackedGridUsesPhysicalViewportHeight(); testCentralSnapshotAndVirtualActionRouting(); std::cout << "Fluent table/data-grid tests passed\n"; return 0; }
    catch (const std::exception& error) { std::cerr << "Fluent table/data-grid test failure: " << error.what() << '\n'; return 1; }
}
