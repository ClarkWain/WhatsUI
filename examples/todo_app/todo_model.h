#pragma once

#include <optional>
#include <string>

namespace whatsui::todo {

// The product model intentionally has no UI dependency.  The optional date is
// persisted as a calendar-only ISO-8601 value (YYYY-MM-DD); formatting it for
// the Windows locale belongs to the Todo presentation layer.
struct TodoRecord {
    int id{0};
    std::string title;
    bool completed{false};
    bool important{false};
    std::optional<std::string> dueDateIso;

    [[nodiscard]] bool operator==(const TodoRecord& other) const noexcept
    {
        return id == other.id
            && title == other.title
            && completed == other.completed
            && important == other.important
            && dueDateIso == other.dueDateIso;
    }

    [[nodiscard]] bool operator!=(const TodoRecord& other) const noexcept
    {
        return !(*this == other);
    }
};

} // namespace whatsui::todo
