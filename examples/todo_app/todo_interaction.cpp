#include "todo_interaction.h"

#include <algorithm>
#include <utility>

namespace whatsui::todo {

TodoInteraction::TodoInteraction(TodoController& controller) noexcept
    : controller_(&controller)
{
}

const TodoInlineValidation& TodoInteraction::validation() const noexcept { return validation_; }
const TodoUndoPresentation& TodoInteraction::undoPresentation() const noexcept { return undo_; }
void TodoInteraction::dismissValidation() noexcept { clearValidation(); }
void TodoInteraction::dismissUndo() noexcept { undo_ = {}; }
std::optional<int> TodoInteraction::editingId() const noexcept { return editingId_; }
const std::string& TodoInteraction::editDraft() const noexcept { return editDraft_; }

TodoActionResult TodoInteraction::beginEdit(int id)
{
    const auto found = std::find_if(controller_->records().begin(), controller_->records().end(), [id](const TodoRecord& item) {
        return item.id == id;
    });
    if (found == controller_->records().end()) {
        const TodoActionResult action{TodoActionStatus::NotFound, id, "This task no longer exists."};
        showValidation(TodoValidationField::EditTitle, action);
        return action;
    }
    editingId_ = id;
    editDraft_ = found->title;
    clearValidation();
    return {TodoActionStatus::Success, id, {}};
}

void TodoInteraction::setEditDraft(std::string value)
{
    editDraft_ = std::move(value);
    if (validation_.field == TodoValidationField::EditTitle) clearValidation();
}

void TodoInteraction::cancelEdit() noexcept
{
    editingId_.reset();
    editDraft_.clear();
    if (validation_.field == TodoValidationField::EditTitle) clearValidation();
}

TodoActionResult TodoInteraction::commitEdit()
{
    if (!editingId_) {
        const TodoActionResult action{TodoActionStatus::NotFound, 0, "Choose a task to edit."};
        showValidation(TodoValidationField::EditTitle, action);
        return action;
    }
    const TodoActionResult action = controller_->editTitle(*editingId_, editDraft_);
    const auto presented = present(action, TodoValidationField::EditTitle, "Task title updated");
    if (presented.status == TodoActionStatus::Success || presented.status == TodoActionStatus::NoChange) cancelEdit();
    return presented;
}

TodoActionResult TodoInteraction::commitEditDetails(bool important,
                                                    std::optional<std::string> dueDateIso)
{
    if (!editingId_) {
        const TodoActionResult action{TodoActionStatus::NotFound, 0, "Choose a task to edit."};
        showValidation(TodoValidationField::EditTitle, action);
        return action;
    }
    const TodoActionResult action = controller_->updateDetails(
        *editingId_, editDraft_, important, std::move(dueDateIso));
    const TodoValidationField field = action.status == TodoActionStatus::InvalidDueDate
        ? TodoValidationField::DueDate
        : TodoValidationField::EditTitle;
    const auto presented = present(action, field, "Task details updated");
    if (presented.status == TodoActionStatus::Success || presented.status == TodoActionStatus::NoChange) {
        cancelEdit();
    }
    return presented;
}

TodoActionResult TodoInteraction::addTask(std::string title)
{
    return present(controller_->add(std::move(title)), TodoValidationField::NewTitle, "Task added");
}

TodoActionResult TodoInteraction::toggleTask(int id)
{
    return present(controller_->toggle(id), TodoValidationField::None, "Task completion changed");
}

TodoActionResult TodoInteraction::removeTask(int id)
{
    if (editingId_ && *editingId_ == id) cancelEdit();
    return present(controller_->remove(id), TodoValidationField::None, "Task removed");
}

TodoActionResult TodoInteraction::clearCompleted()
{
    return present(controller_->clearCompleted(), TodoValidationField::None, "Completed tasks cleared");
}

TodoActionResult TodoInteraction::setImportant(int id, bool important)
{
    return present(controller_->setImportant(id, important), TodoValidationField::None,
                   important ? "Task marked important" : "Task removed from important");
}

TodoActionResult TodoInteraction::setDueDate(int id, std::optional<std::string> dueDateIso)
{
    return present(controller_->setDueDate(id, std::move(dueDateIso)), TodoValidationField::DueDate,
                   "Due date updated");
}

TodoActionResult TodoInteraction::undo()
{
    const TodoActionResult action = controller_->undo();
    if (action.status == TodoActionStatus::Success) {
        undo_ = {};
        clearValidation();
    }
    return action;
}

std::vector<TodoRecord> TodoInteraction::filtered(TodoFilter filter, std::string query) const
{
    return controller_->filter(filter, std::move(query));
}

TodoActionResult TodoInteraction::present(TodoActionResult action, TodoValidationField validationField,
                                          std::string undoMessage)
{
    if (action.status == TodoActionStatus::Success) {
        clearValidation();
        showUndo(std::move(undoMessage));
    } else if (action.status == TodoActionStatus::NoChange) {
        clearValidation();
    } else if (validationField != TodoValidationField::None) {
        showValidation(validationField, action);
    }
    return action;
}

void TodoInteraction::showValidation(TodoValidationField field, const TodoActionResult& action)
{
    validation_ = {field, action.status, action.message};
}

void TodoInteraction::clearValidation() noexcept
{
    validation_ = {};
}

void TodoInteraction::showUndo(std::string message)
{
    undo_.visible = true;
    undo_.message = std::move(message);
    undo_.actionLabel = "Undo";
}

} // namespace whatsui::todo
