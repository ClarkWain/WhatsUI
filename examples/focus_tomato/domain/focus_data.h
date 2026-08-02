#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace whatsui::focus_tomato {

inline constexpr int kCurrentSchemaVersion = 1;
inline constexpr std::int64_t kMinuteMs = 60'000;

enum class TaskStatus {
    Active,
    Done,
    Archived,
};

enum class SessionType {
    Focus,
    ShortBreak,
    LongBreak,
};

enum class SessionStatus {
    Pending,
    Running,
    Paused,
    CompletionPending,
    Completed,
    Aborted,
    Skipped,
};

enum class CompletionReason {
    None,
    Natural,
    Manual,
    Recovered,
    UserAborted,
    UserSkipped,
};

struct TaskRecord {
    std::string id;
    std::string title;
    TaskStatus status{TaskStatus::Active};
    int estimatedPomodoros{1};
    int completedPomodoros{0};
    std::int64_t sortOrder{0};
    std::int64_t revision{1};
    std::int64_t createdAtUtcMs{0};
    std::int64_t updatedAtUtcMs{0};

    [[nodiscard]] bool operator==(const TaskRecord& other) const noexcept;
};

struct FocusSessionRecord {
    std::string id;
    std::optional<std::string> taskId;
    std::string titleSnapshot;
    SessionType type{SessionType::Focus};
    std::int64_t plannedDurationMs{25 * kMinuteMs};
    std::int64_t startedAtUtcMs{0};
    std::optional<std::int64_t> targetEndAtUtcMs;
    std::int64_t remainingMs{25 * kMinuteMs};
    SessionStatus status{SessionStatus::Pending};
    std::optional<std::int64_t> endedAtUtcMs;
    CompletionReason completionReason{CompletionReason::None};
    std::string idempotencyKey;

    [[nodiscard]] bool operator==(const FocusSessionRecord& other) const noexcept;
};

struct TimerSnapshot {
    int schemaVersion{kCurrentSchemaVersion};
    std::string sessionId;
    SessionStatus status{SessionStatus::Paused};
    std::int64_t savedAtUtcMs{0};
    std::optional<std::int64_t> targetEndAtUtcMs;
    std::int64_t remainingMs{0};

    [[nodiscard]] bool operator==(const TimerSnapshot& other) const noexcept;
};

struct FocusSettings {
    int focusMinutes{25};
    int shortBreakMinutes{5};
    int longBreakMinutes{15};
    int longBreakEvery{4};
    int soundVolumePercent{70};
    bool autoStartBreak{false};
    bool launchAtLogin{false};

    [[nodiscard]] bool operator==(const FocusSettings& other) const noexcept;
};

struct FocusData {
    int schemaVersion{kCurrentSchemaVersion};
    FocusSettings settings;
    std::vector<TaskRecord> tasks;
    std::vector<FocusSessionRecord> sessions;
    std::optional<std::string> activeSessionId;
    std::optional<TimerSnapshot> timerSnapshot;

    [[nodiscard]] bool operator==(const FocusData& other) const noexcept;
};

[[nodiscard]] bool isActiveSessionStatus(SessionStatus status) noexcept;
[[nodiscard]] bool isTerminalSessionStatus(SessionStatus status) noexcept;

} // namespace whatsui::focus_tomato
