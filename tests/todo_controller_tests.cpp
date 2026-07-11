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
}

} // namespace

int main()
{
    testAddTrimsAndRejectsDuplicates();
    testMutationsAndSingleStepUndoRestoreExactVector();
    testEditImportantDueDateAndClear();
    testFilterRetainsModelOrder();
    return 0;
}
