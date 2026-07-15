#include <stdexcept>
#include <string>
#include <vector>

#include "todo_controller.h"

namespace {

using whatsui::todo::TodoActionStatus;
using whatsui::todo::TodoController;
using whatsui::todo::TodoFilter;
using whatsui::todo::TodoRecord;

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

void testAddTrimsAndRejectsDuplicates()
{
    TodoController controller;
    const auto first = controller.add("  Prepare release notes \t");
    expect(first.status == TodoActionStatus::Success && first.recordId == 1,
           "Add should trim a valid title and assign a stable positive id");
    expect(controller.records().front().title == "Prepare release notes", "Add should persist the trimmed title");
    expect(controller.add("prepare RELEASE notes").status == TodoActionStatus::DuplicateTitle,
           "Duplicate validation should be case-insensitive for desktop text entry");
    expect(controller.add(" \r\n ").status == TodoActionStatus::EmptyTitle,
           "Whitespace-only tasks should produce an inline-ready empty-title error");
}

void testMutationsAndSingleStepUndoRestoreExactVector()
{
    const std::vector<TodoRecord> initial{{4, "Ship", false, true, std::string("2026-07-12")},
                                          {9, "Review", true, false, std::nullopt}};
    TodoController controller(initial);
    const auto beforeToggle = controller.records();
    expect(controller.toggle(4).status == TodoActionStatus::Success, "Toggle should find an existing item");
    expect(controller.records()[0].completed, "Toggle should change completion state");
    expect(controller.undo().status == TodoActionStatus::Success && controller.records() == beforeToggle,
           "Undo should restore the complete exact pre-mutation vector");
    expect(controller.undo().status == TodoActionStatus::NothingToUndo,
           "Undo is deliberately one-step and consumes its checkpoint");
    expect(controller.remove(123).status == TodoActionStatus::NotFound, "Remove should report a missing item explicitly");
}

void testEditImportantDueDateAndClear()
{
    TodoController controller({{1, "One", false, false, std::nullopt}, {2, "Two", true, false, std::nullopt}});
    expect(controller.editTitle(1, "  One revised ").status == TodoActionStatus::Success,
           "EditTitle should trim and update valid content");
    expect(controller.editTitle(1, "two").status == TodoActionStatus::DuplicateTitle,
           "EditTitle should apply duplicate validation excluding only itself");
    expect(controller.setImportant(1, true).status == TodoActionStatus::Success && controller.records()[0].important,
           "SetImportant should update priority state");
    expect(controller.setDueDate(1, std::string("2024-02-29")).status == TodoActionStatus::Success,
           "Leap-year due dates should be accepted");
    expect(controller.setDueDate(1, std::string("2023-02-29")).status == TodoActionStatus::InvalidDueDate,
           "Invalid calendar dates should return an explicit validation state");
    expect(controller.clearCompleted().status == TodoActionStatus::Success && controller.records().size() == 1,
           "ClearCompleted should remove all completed records as one operation");
}

void testImportantNoChangePreservesUndoCheckpoint()
{
    const std::vector<TodoRecord> initial{{1, "Important contract", false, false, std::nullopt}};
    TodoController controller(initial);

    expect(controller.setImportant(1, true).status == TodoActionStatus::Success,
           "Changing important state should succeed");
    expect(controller.setImportant(1, true).status == TodoActionStatus::NoChange,
           "Writing the current important state should report NoChange");
    expect(controller.records()[0].important,
           "An important NoChange should leave the current metadata intact");
    expect(controller.undo().status == TodoActionStatus::Success && controller.records() == initial,
           "An important NoChange must not replace the preceding successful Undo checkpoint");
}

void testDueDateValidationNoChangeClearAndUndo()
{
    TodoController controller({{1, "Date contract", false, false, std::nullopt}});

    expect(controller.setDueDate(1, std::string(" 2028-02-29\t")).status == TodoActionStatus::Success,
           "A valid due date should be trimmed and accepted");
    expect(controller.records()[0].dueDateIso == std::optional<std::string>{"2028-02-29"},
           "The controller should store the canonical trimmed due date");
    expect(controller.setDueDate(1, std::string("2028-02-29")).status == TodoActionStatus::NoChange,
           "Writing the current due date should report NoChange");
    expect(controller.setDueDate(1, std::string("2027-02-29")).status == TodoActionStatus::InvalidDueDate,
           "A non-leap February 29 should be rejected");
    expect(controller.setDueDate(1, std::string("2028-13-01")).status == TodoActionStatus::InvalidDueDate,
           "An out-of-range month should be rejected");
    expect(controller.setDueDate(1, std::string("2028-02-00")).status == TodoActionStatus::InvalidDueDate,
           "An out-of-range day should be rejected");
    expect(controller.setDueDate(1, std::string("2028-2-9")).status == TodoActionStatus::InvalidDueDate,
           "A due date must use the fixed YYYY-MM-DD representation");
    expect(controller.undo().status == TodoActionStatus::Success && !controller.records()[0].dueDateIso,
           "NoChange and invalid due dates must preserve the Undo checkpoint for the last successful update");

    TodoController clearController({{2, "Clear date", false, true, std::string("2026-12-31")}});
    const auto beforeClear = clearController.records();
    expect(clearController.setDueDate(2, std::nullopt).status == TodoActionStatus::Success
               && !clearController.records()[0].dueDateIso,
           "Clearing an existing due date should succeed");
    expect(clearController.setDueDate(2, std::nullopt).status == TodoActionStatus::NoChange,
           "Clearing an already empty due date should report NoChange");
    expect(clearController.undo().status == TodoActionStatus::Success && clearController.records() == beforeClear,
           "Undo should restore a cleared due date and all sibling metadata exactly");
}

void testFilterRetainsModelOrder()
{
    TodoController controller({{1, "Draft Windows guide", false, false, std::nullopt},
                               {2, "Ship release", true, true, std::nullopt},
                               {3, "Review guide", false, false, std::nullopt}});
    const auto all = controller.filter();
    const auto activeGuide = controller.filter(TodoFilter::Active, "GUIDE");
    const auto completed = controller.filter(TodoFilter::Completed);
    expect(all.size() == 3, "All filter should retain all records");
    expect(activeGuide.size() == 2 && activeGuide[0].id == 1 && activeGuide[1].id == 3,
           "Filter should combine state/query matching while preserving source order");
    expect(completed.size() == 1 && completed[0].id == 2, "Completed filter should select only complete tasks");
    expect(completed[0].important,
           "Filtering should preserve important metadata on copied presentation records");

    controller.setRecords({{4, "Dated guide", false, true, std::string("2027-03-04")}});
    const auto dated = controller.filter(TodoFilter::Active, "dated");
    expect(dated.size() == 1 && dated[0].important
               && dated[0].dueDateIso == std::optional<std::string>{"2027-03-04"},
           "Filtering should preserve due-date and important metadata exactly");
}

void testDetailsUpdateIsAtomicAndUsesOneUndoCheckpoint()
{
    TodoController controller({{1, "Original", false, false, std::nullopt},
                               {2, "Existing", false, false, std::nullopt}});
    const auto before = controller.records();
    expect(controller.updateDetails(1, "Revised", true, std::string("2023-02-29")).status
               == TodoActionStatus::InvalidDueDate,
           "Invalid details must reject the complete edit before any field changes");
    expect(controller.records() == before,
           "An invalid due date must not partially commit title or priority changes");
    expect(controller.updateDetails(1, "existing", true, std::string("2026-07-15")).status
               == TodoActionStatus::DuplicateTitle,
           "Atomic details validation must retain duplicate-title protection");
    expect(controller.records() == before, "A duplicate title must leave every detail unchanged");

    expect(controller.updateDetails(1, "Revised", true, std::string("2026-07-15")).status
               == TodoActionStatus::Success,
           "A valid details edit should commit all fields together");
    expect(controller.records()[0].title == "Revised" && controller.records()[0].important
               && controller.records()[0].dueDateIso == std::optional<std::string>{"2026-07-15"},
           "A successful details edit must publish title, priority, and due date together");
    expect(controller.undo().status == TodoActionStatus::Success && controller.records() == before,
           "One Undo must restore the complete pre-dialog task record");
}

} // namespace

int main()
{
    testAddTrimsAndRejectsDuplicates();
    testMutationsAndSingleStepUndoRestoreExactVector();
    testEditImportantDueDateAndClear();
    testImportantNoChangePreservesUndoCheckpoint();
    testDueDateValidationNoChangeClearAndUndo();
    testFilterRetainsModelOrder();
    testDetailsUpdateIsAtomicAndUsesOneUndoCheckpoint();
    return 0;
}
