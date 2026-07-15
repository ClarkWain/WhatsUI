#pragma once

// UI-independent Todo workflow rules. The controller owns one in-memory
// record vector and exposes explicit operation outcomes so a view can show an
// inline error without inferring failure from mutations or exception text.

#include <optional>
#include <string>
#include <vector>

#include "todo_model.h"

namespace whatsui::todo {

enum class TodoActionStatus {
    Success,
    NoChange,
    EmptyTitle,
    DuplicateTitle,
    NotFound,
    InvalidDueDate,
    NothingToUndo,
};

struct TodoActionResult {
    TodoActionStatus status{TodoActionStatus::Success};
    int recordId{0};
    std::string message;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return status == TodoActionStatus::Success || status == TodoActionStatus::NoChange;
    }
};

enum class TodoFilter {
    All,
    Active,
    Completed,
};

class TodoController {
public:
    explicit TodoController(std::vector<TodoRecord> records = {});

    [[nodiscard]] const std::vector<TodoRecord>& records() const noexcept;
    // Replaces the current backing model, for example after a storage load.
    // This intentionally clears any undo from the prior model.
    void setRecords(std::vector<TodoRecord> records);

    [[nodiscard]] TodoActionResult add(std::string title);
    [[nodiscard]] TodoActionResult toggle(int id);
    [[nodiscard]] TodoActionResult remove(int id);
    [[nodiscard]] TodoActionResult clearCompleted();
    [[nodiscard]] TodoActionResult editTitle(int id, std::string title);
    // Validates and commits all editable task details as one mutation. This
    // prevents a dialog Save from partially updating fields and gives the
    // complete edit one Undo checkpoint.
    [[nodiscard]] TodoActionResult updateDetails(int id, std::string title, bool important,
                                                 std::optional<std::string> dueDateIso);
    [[nodiscard]] TodoActionResult setImportant(int id, bool important);
    // A missing value clears the due date. Present dates must be calendar-only
    // ISO-8601 values in YYYY-MM-DD form.
    [[nodiscard]] TodoActionResult setDueDate(int id, std::optional<std::string> dueDateIso);

    // Restores exactly the vector before the most recent successful mutation.
    // Undo itself consumes that single checkpoint.
    [[nodiscard]] TodoActionResult undo();

    [[nodiscard]] std::vector<TodoRecord> filter(TodoFilter filter = TodoFilter::All,
                                                  std::string query = {}) const;

private:
    struct UndoSnapshot {
        std::vector<TodoRecord> records;
        int nextId{1};
    };

    [[nodiscard]] static std::string trim(std::string value);
    [[nodiscard]] static std::string canonicalTitle(const std::string& value);
    [[nodiscard]] static bool isDueDateValid(const std::string& value) noexcept;
    [[nodiscard]] bool titleExists(const std::string& title, int exceptId = 0) const;
    [[nodiscard]] std::vector<TodoRecord>::iterator find(int id);
    void rememberForUndo();
    void resetNextId();

    std::vector<TodoRecord> records_;
    int nextId_{1};
    std::optional<UndoSnapshot> undo_;
};

} // namespace whatsui::todo
