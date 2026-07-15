#include <stdexcept>
#include <string>

#include "todo_interaction.h"

namespace {

using whatsui::todo::TodoActionStatus;
using whatsui::todo::TodoController;
using whatsui::todo::TodoFilter;
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

void testMetadataPresentationNoChangeAndUndo()
{
    TodoController controller({{1, "Metadata presentation", false, false, std::nullopt}});
    TodoInteraction interaction(controller);

    expect(interaction.setImportant(1, true).status == TodoActionStatus::Success,
           "Interaction should forward a successful important update");
    expect(controller.records()[0].important
               && interaction.undoPresentation().visible
               && interaction.undoPresentation().message == "Task marked important",
           "Important updates should mutate the model and expose their specific Undo message");
    expect(interaction.setImportant(1, true).status == TodoActionStatus::NoChange,
           "Repeating the current important state should report NoChange");
    expect(interaction.undoPresentation().visible
               && interaction.undoPresentation().message == "Task marked important",
           "An important NoChange should not replace the preceding valid Undo presentation");
    expect(interaction.undo().status == TodoActionStatus::Success
               && !controller.records()[0].important
               && !interaction.undoPresentation().visible,
           "Interaction Undo should restore important metadata and consume its presentation");

    expect(interaction.setDueDate(1, std::string(" 2028-02-29 ")).status == TodoActionStatus::Success,
           "Interaction should accept a trimmed valid due date");
    expect(controller.records()[0].dueDateIso == std::optional<std::string>{"2028-02-29"}
               && interaction.undoPresentation().message == "Due date updated",
           "A due-date update should store its canonical value and expose Undo");
    expect(interaction.setDueDate(1, std::string("2028-02-29")).status == TodoActionStatus::NoChange,
           "Repeating a due date should report NoChange");
    expect(interaction.setDueDate(1, std::string("2028-04-31")).status == TodoActionStatus::InvalidDueDate,
           "Interaction should expose invalid calendar dates");
    expect(interaction.validation().field == TodoValidationField::DueDate
               && interaction.undoPresentation().visible
               && interaction.undoPresentation().message == "Due date updated",
           "Invalid metadata input must retain the prior successful Undo action while showing inline validation");
    expect(interaction.undo().status == TodoActionStatus::Success
               && !controller.records()[0].dueDateIso
               && !interaction.validation().visible(),
           "Undo after invalid input should restore the previous due date and clear validation");
}

void testFilterPresentationAndEditCancellation()
{
    TodoController controller;
    const int activeId = controller.add("Draft Windows search").recordId;
    const int doneId = controller.add("Ship Windows Todo").recordId;
    (void)controller.toggle(doneId);
    TodoInteraction interaction(controller);

    const auto activeSearch = interaction.filtered(TodoFilter::Active, "SEARCH");
    const auto completed = interaction.filtered(TodoFilter::Completed);
    expect(activeSearch.size() == 1 && activeSearch.front().id == activeId,
           "Interaction filtering should preserve the controller's active search result");
    expect(completed.size() == 1 && completed.front().id == doneId,
           "Interaction filtering should expose the completed view without changing model order");

    expect(interaction.setImportant(activeId, true).status == TodoActionStatus::Success,
           "Metadata setup should succeed before filtering");
    expect(interaction.setDueDate(activeId, std::string("2027-05-06")).status == TodoActionStatus::Success,
           "Due-date setup should succeed before filtering");
    const auto metadataSearch = interaction.filtered(TodoFilter::Active, "draft");
    expect(metadataSearch.size() == 1 && metadataSearch.front().important
               && metadataSearch.front().dueDateIso == std::optional<std::string>{"2027-05-06"},
           "Interaction filtering should preserve important and due-date presentation metadata");

    (void)interaction.beginEdit(activeId);
    interaction.setEditDraft("temporary draft");
    interaction.cancelEdit();
    expect(!interaction.editingId() && interaction.editDraft().empty() && !interaction.validation().visible(),
           "Cancelling an edit must discard only presentation draft and validation state");
}

void testDetailsEditIsAtomicAndKeepsInvalidDraftOpen()
{
    TodoController controller({{1, "Original", false, false, std::nullopt}});
    TodoInteraction interaction(controller);
    expect(interaction.beginEdit(1).succeeded(), "Details fixture should enter edit mode");
    interaction.setEditDraft("Revised");
    expect(interaction.commitEditDetails(true, std::string("2023-02-29")).status
               == TodoActionStatus::InvalidDueDate,
           "Invalid dialog details should surface due-date validation");
    expect(interaction.editingId() == std::optional<int>{1}
               && interaction.editDraft() == "Revised"
               && interaction.validation().field == TodoValidationField::DueDate,
           "Invalid due date should keep the complete edit draft and dialog context active");
    expect(controller.records()[0].title == "Original" && !controller.records()[0].important
               && !controller.records()[0].dueDateIso,
           "Invalid dialog details must not partially mutate the model");

    expect(interaction.commitEditDetails(true, std::string("2026-07-15")).status
               == TodoActionStatus::Success,
           "Corrected dialog details should commit successfully");
    expect(!interaction.editingId() && interaction.undoPresentation().visible
               && interaction.undoPresentation().message == "Task details updated",
           "Successful detail Save should close editing and expose one coherent Undo action");
    (void)interaction.undo();
    expect(controller.records()[0].title == "Original" && !controller.records()[0].important
               && !controller.records()[0].dueDateIso,
           "Detail-edit Undo should restore all fields together");
}

} // namespace

int main()
{
    testAddValidationAndUndoPresentation();
    testEditDraftAndInlineDuplicateError();
    testDueDateValidationDoesNotOfferUndo();
    testMetadataPresentationNoChangeAndUndo();
    testFilterPresentationAndEditCancellation();
    testDetailsEditIsAtomicAndKeepsInvalidDraftOpen();
    return 0;
}
