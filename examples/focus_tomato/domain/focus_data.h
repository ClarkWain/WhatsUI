#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace whatsui::focus_tomato {

inline constexpr int kCurrentSchemaVersion = 2;
inline constexpr std::int64_t kMinuteMs = 60'000;

enum class TaskStatus {
    Active,
    Done,
    Archived,
    ArchivedDone,
};

enum class TaskSoundPreference {
    Inherit,
    Off,
    Soundscape,
};

struct TaskExecutionPreferences {
    // Empty means the task follows FocusSettings::focusMinutes.
    std::optional<int> focusMinutes;
    TaskSoundPreference sound{TaskSoundPreference::Inherit};
    // Required only when sound == Soundscape.
    std::string soundscapeId;

    [[nodiscard]] bool operator==(
        const TaskExecutionPreferences& other) const noexcept;
};

[[nodiscard]] inline constexpr bool isArchivedTaskStatus(
    TaskStatus status) noexcept
{
    return status == TaskStatus::Archived
        || status == TaskStatus::ArchivedDone;
}

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

// ADR-008: interruption events attached to a session. See
// doc/whatsui/ADR-008-focus-tomato-interruption-entity.md.
enum class InterruptionReason {
    UserPause,
    UserAway,
    Meeting,
    Emergency,
    SystemLock,
    ApplicationClose,
    NetworkOffline,
    Other,
};

enum class InterruptionSource {
    User,
    System,
    Application,
};

struct InterruptionEvent {
    InterruptionReason reason{InterruptionReason::UserPause};
    std::string note;
    std::int64_t occurredAtUtcMs{0};
    std::int64_t detectedAtUtcMs{0};
    InterruptionSource source{InterruptionSource::User};

    [[nodiscard]] bool operator==(
        const InterruptionEvent& other) const noexcept;
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
    TaskExecutionPreferences execution;

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
    // Resolved at session start. Empty means this session is intentionally silent.
    std::optional<std::string> soundscapeIdSnapshot;
    // ADR-008: append-only list of interruption events. Every Running->Paused
    // transition writes one entry; presentation layer aggregates for display.
    std::vector<InterruptionEvent> interruptions;

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
    // Empty means sound is globally disabled.
    std::string defaultSoundscapeId{"rain"};

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
[[nodiscard]] int effectiveFocusMinutes(
    const TaskRecord& task,
    const FocusSettings& settings) noexcept;
[[nodiscard]] std::optional<std::string> effectiveSoundscapeId(
    const TaskRecord& task,
    const FocusSettings& settings);

} // namespace whatsui::focus_tomato
