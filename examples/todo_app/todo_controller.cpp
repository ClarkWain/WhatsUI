#include "todo_controller.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <utility>

namespace whatsui::todo {
namespace {

TodoActionResult result(TodoActionStatus status, int recordId = 0)
{
    switch (status) {
    case TodoActionStatus::Success: return {status, recordId, {}};
    case TodoActionStatus::NoChange: return {status, recordId, "No changes to save."};
    case TodoActionStatus::EmptyTitle: return {status, recordId, "Enter a task title."};
    case TodoActionStatus::DuplicateTitle: return {status, recordId, "A task with this title already exists."};
    case TodoActionStatus::NotFound: return {status, recordId, "This task no longer exists."};
    case TodoActionStatus::InvalidDueDate: return {status, recordId, "Use a valid date in YYYY-MM-DD format."};
    case TodoActionStatus::NothingToUndo: return {status, recordId, "Nothing to undo."};
    }
    return {TodoActionStatus::NoChange, recordId, "No changes to save."};
}

[[nodiscard]] bool isSpace(unsigned char value) noexcept
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\f' || value == '\v';
}

[[nodiscard]] bool isLeapYear(int year) noexcept
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

} // namespace

TodoController::TodoController(std::vector<TodoRecord> records)
    : records_(std::move(records))
{
    resetNextId();
}

const std::vector<TodoRecord>& TodoController::records() const noexcept
{
    return records_;
}

void TodoController::setRecords(std::vector<TodoRecord> records)
{
    records_ = std::move(records);
    undo_.reset();
    resetNextId();
}

TodoActionResult TodoController::add(std::string title)
{
    title = trim(std::move(title));
    if (title.empty()) return result(TodoActionStatus::EmptyTitle);
    if (titleExists(title)) return result(TodoActionStatus::DuplicateTitle);
    rememberForUndo();
    const int id = nextId_++;
    records_.push_back({id, std::move(title), false, false, std::nullopt});
    return result(TodoActionStatus::Success, id);
}

TodoActionResult TodoController::toggle(int id)
{
    const auto item = find(id);
    if (item == records_.end()) return result(TodoActionStatus::NotFound, id);
    rememberForUndo();
    item->completed = !item->completed;
    return result(TodoActionStatus::Success, id);
}

TodoActionResult TodoController::remove(int id)
{
    const auto item = find(id);
    if (item == records_.end()) return result(TodoActionStatus::NotFound, id);
    rememberForUndo();
    records_.erase(item);
    return result(TodoActionStatus::Success, id);
}

TodoActionResult TodoController::clearCompleted()
{
    const auto completed = std::find_if(records_.begin(), records_.end(), [](const TodoRecord& item) {
        return item.completed;
    });
    if (completed == records_.end()) return result(TodoActionStatus::NoChange);
    rememberForUndo();
    records_.erase(std::remove_if(records_.begin(), records_.end(), [](const TodoRecord& item) {
        return item.completed;
    }), records_.end());
    return result(TodoActionStatus::Success);
}

TodoActionResult TodoController::editTitle(int id, std::string title)
{
    title = trim(std::move(title));
    if (title.empty()) return result(TodoActionStatus::EmptyTitle, id);
    const auto item = find(id);
    if (item == records_.end()) return result(TodoActionStatus::NotFound, id);
    if (canonicalTitle(item->title) == canonicalTitle(title)) return result(TodoActionStatus::NoChange, id);
    if (titleExists(title, id)) return result(TodoActionStatus::DuplicateTitle, id);
    rememberForUndo();
    item->title = std::move(title);
    return result(TodoActionStatus::Success, id);
}

TodoActionResult TodoController::updateDetails(int id, std::string title, bool important,
                                               std::optional<std::string> dueDateIso)
{
    title = trim(std::move(title));
    if (title.empty()) return result(TodoActionStatus::EmptyTitle, id);
    const auto item = find(id);
    if (item == records_.end()) return result(TodoActionStatus::NotFound, id);
    if (titleExists(title, id)) return result(TodoActionStatus::DuplicateTitle, id);
    if (dueDateIso) {
        *dueDateIso = trim(std::move(*dueDateIso));
        if (!isDueDateValid(*dueDateIso)) return result(TodoActionStatus::InvalidDueDate, id);
    }
    if (item->title == title && item->important == important && item->dueDateIso == dueDateIso) {
        return result(TodoActionStatus::NoChange, id);
    }
    rememberForUndo();
    item->title = std::move(title);
    item->important = important;
    item->dueDateIso = std::move(dueDateIso);
    return result(TodoActionStatus::Success, id);
}

TodoActionResult TodoController::setImportant(int id, bool important)
{
    const auto item = find(id);
    if (item == records_.end()) return result(TodoActionStatus::NotFound, id);
    if (item->important == important) return result(TodoActionStatus::NoChange, id);
    rememberForUndo();
    item->important = important;
    return result(TodoActionStatus::Success, id);
}

TodoActionResult TodoController::setDueDate(int id, std::optional<std::string> dueDateIso)
{
    const auto item = find(id);
    if (item == records_.end()) return result(TodoActionStatus::NotFound, id);
    if (dueDateIso) {
        *dueDateIso = trim(std::move(*dueDateIso));
        if (!isDueDateValid(*dueDateIso)) return result(TodoActionStatus::InvalidDueDate, id);
    }
    if (item->dueDateIso == dueDateIso) return result(TodoActionStatus::NoChange, id);
    rememberForUndo();
    item->dueDateIso = std::move(dueDateIso);
    return result(TodoActionStatus::Success, id);
}

TodoActionResult TodoController::undo()
{
    if (!undo_) return result(TodoActionStatus::NothingToUndo);
    records_ = std::move(undo_->records);
    nextId_ = undo_->nextId;
    undo_.reset();
    return result(TodoActionStatus::Success);
}

std::vector<TodoRecord> TodoController::filter(TodoFilter filter, std::string query) const
{
    const std::string needle = canonicalTitle(trim(std::move(query)));
    std::vector<TodoRecord> result;
    result.reserve(records_.size());
    for (const TodoRecord& item : records_) {
        if (filter == TodoFilter::Active && item.completed) continue;
        if (filter == TodoFilter::Completed && !item.completed) continue;
        if (!needle.empty() && canonicalTitle(item.title).find(needle) == std::string::npos) continue;
        result.push_back(item);
    }
    return result;
}

std::string TodoController::trim(std::string value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return isSpace(c); });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return isSpace(c); }).base();
    return first >= last ? std::string{} : std::string(first, last);
}

std::string TodoController::canonicalTitle(const std::string& value)
{
    std::string result;
    result.reserve(value.size());
    for (const unsigned char character : value) {
        result.push_back(static_cast<char>(character <= 0x7F ? std::tolower(character) : character));
    }
    return result;
}

bool TodoController::isDueDateValid(const std::string& value) noexcept
{
    if (value.size() != 10 || value[4] != '-' || value[7] != '-') return false;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index == 4 || index == 7) continue;
        if (value[index] < '0' || value[index] > '9') return false;
    }
    const int year = std::stoi(value.substr(0, 4));
    const int month = std::stoi(value.substr(5, 2));
    const int day = std::stoi(value.substr(8, 2));
    if (year < 1 || month < 1 || month > 12) return false;
    static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int maximumDay = days[month - 1];
    if (month == 2 && isLeapYear(year)) maximumDay = 29;
    return day >= 1 && day <= maximumDay;
}

bool TodoController::titleExists(const std::string& title, int exceptId) const
{
    const std::string canonical = canonicalTitle(title);
    return std::any_of(records_.begin(), records_.end(), [&canonical, exceptId](const TodoRecord& item) {
        return item.id != exceptId && canonicalTitle(item.title) == canonical;
    });
}

std::vector<TodoRecord>::iterator TodoController::find(int id)
{
    return std::find_if(records_.begin(), records_.end(), [id](const TodoRecord& item) { return item.id == id; });
}

void TodoController::rememberForUndo()
{
    undo_ = UndoSnapshot{records_, nextId_};
}

void TodoController::resetNextId()
{
    nextId_ = 1;
    for (const TodoRecord& item : records_) {
        if (item.id >= nextId_ && item.id < std::numeric_limits<int>::max()) nextId_ = item.id + 1;
    }
}

} // namespace whatsui::todo
