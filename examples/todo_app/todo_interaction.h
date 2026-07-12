#pragma once

// Presentation-neutral Todo interaction state.  This is the narrow bridge
// between TodoController's domain outcomes and a Fluent page's inline field
// validation / single-step Undo affordance; it deliberately creates no Nodes.

#include <optional>
#include <string>
#include <vector>

#include "todo_controller.h"

namespace whatsui::todo {

enum class TodoValidationField {
    None,
    NewTitle,
    EditTitle,
    DueDate,
};

struct TodoInlineValidation {
    TodoValidationField field{TodoValidationField::None};
    TodoActionStatus status{TodoActionStatus::Success};
    std::string message;

    [[nodiscard]] bool visible() const noexcept { return field != TodoValidationField::None && !message.empty(); }
};

struct TodoUndoPresentation {
    bool visible{false};
    std::string message;
    std::string actionLabel{"Undo"};
};

class TodoInteraction {
public:
    explicit TodoInteraction(TodoController& controller) noexcept;

    [[nodiscard]] const TodoInlineValidation& validation() const noexcept;
    [[nodiscard]] const TodoUndoPresentation& undoPresentation() const noexcept;
    void dismissValidation() noexcept;
    void dismissUndo() noexcept;

    [[nodiscard]] std::optional<int> editingId() const noexcept;
    [[nodiscard]] const std::string& editDraft() const noexcept;
    [[nodiscard]] TodoActionResult beginEdit(int id);
    void setEditDraft(std::string value);
    void cancelEdit() noexcept;
    [[nodiscard]] TodoActionResult commitEdit();

    [[nodiscard]] TodoActionResult addTask(std::string title);
    [[nodiscard]] TodoActionResult toggleTask(int id);
    [[nodiscard]] TodoActionResult removeTask(int id);
    [[nodiscard]] TodoActionResult clearCompleted();
    [[nodiscard]] TodoActionResult setImportant(int id, bool important);
    [[nodiscard]] TodoActionResult setDueDate(int id, std::optional<std::string> dueDateIso);
    [[nodiscard]] TodoActionResult undo();

    [[nodiscard]] std::vector<TodoRecord> filtered(TodoFilter filter, std::string query = {}) const;

private:
    [[nodiscard]] TodoActionResult present(TodoActionResult action, TodoValidationField validationField,
                                           std::string undoMessage);
    void showValidation(TodoValidationField field, const TodoActionResult& action);
    void clearValidation() noexcept;
    void showUndo(std::string message);

    TodoController* controller_{nullptr};
    std::optional<int> editingId_;
    std::string editDraft_;
    TodoInlineValidation validation_;
    TodoUndoPresentation undo_;
};

} // namespace whatsui::todo
