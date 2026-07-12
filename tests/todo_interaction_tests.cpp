#include <stdexcept>
#include <string>

#include "todo_interaction.h"

namespace {

using whatsui::todo::TodoActionStatus;
using whatsui::todo::TodoController;
using whatsui::todo::TodoInteraction;
using whatsui::todo::TodoValidationField;

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

void testAddValidationAndUndoPresentation()
{
    TodoController controller;
    TodoInteraction interaction(controller);
    expect(interaction.addTask("   ").status == TodoActionStatus::EmptyTitle,
           "Empty add should preserve controller validation status");
    expect(interaction.validation().visible() && interaction.validation().field == TodoValidationField::NewTitle,
           "Empty add should surface an inline new-title validation message");
    expect(interaction.addTask("Review capture").status == TodoActionStatus::Success,
           "Valid add should create a task");
    expect(!interaction.validation().visible() && interaction.undoPresentation().visible,
           "Successful add should clear validation and expose one Undo presentation");
    expect(interaction.undoPresentation().message == "Task added" && interaction.undoPresentation().actionLabel == "Undo",
           "Undo presentation should use a stable Fluent action contract");
    (void)interaction.undo();
    expect(controller.records().empty() && !interaction.undoPresentation().visible,
           "Undo should restore the exact previous vector and consume its presentation");
}

void testEditDraftAndInlineDuplicateError()
{
    TodoController controller;
    (void)controller.add("First");
    (void)controller.add("Second");
    TodoInteraction interaction(controller);
    expect(interaction.beginEdit(1).succeeded() && interaction.editDraft() == "First",
           "Begin edit should hydrate the controlled draft from the model");
    interaction.setEditDraft(" second ");
    expect(interaction.commitEdit().status == TodoActionStatus::DuplicateTitle,
           "Edit should retain duplicate validation from TodoController");
    expect(interaction.editingId() && *interaction.editingId() == 1
           && interaction.validation().field == TodoValidationField::EditTitle,
           "Failed edit should retain editing context and show an inline edit error");
    interaction.setEditDraft("First revised");
    expect(interaction.commitEdit().status == TodoActionStatus::Success && !interaction.editingId(),
           "Successful edit should close editing state and allow an undo presentation");
    expect(interaction.undoPresentation().visible, "Successful edit should be undoable in the presentation contract");
}

void testDueDateValidationDoesNotOfferUndo()
{
    TodoController controller;
    const int id = controller.add("Plan release").recordId;
    TodoInteraction interaction(controller);
    expect(interaction.setDueDate(id, std::string("2023-02-29")).status == TodoActionStatus::InvalidDueDate,
           "Invalid due date should remain a domain validation error");
    expect(interaction.validation().field == TodoValidationField::DueDate && !interaction.undoPresentation().visible,
           "Invalid due date should be inline-only and must not advertise Undo");
    expect(interaction.setDueDate(id, std::string("2024-02-29")).status == TodoActionStatus::Success,
           "Valid date should pass through the interaction state");
    expect(interaction.undoPresentation().visible, "Successful due-date update should expose Undo");
}

} // namespace

int main()
{
    testAddValidationAndUndoPresentation();
    testEditDraftAndInlineDuplicateError();
    testDueDateValidationDoesNotOfferUndo();
    return 0;
}
