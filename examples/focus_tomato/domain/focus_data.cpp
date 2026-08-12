#include "focus_data.h"

namespace whatsui::focus_tomato {

bool TaskExecutionPreferences::operator==(
    const TaskExecutionPreferences& other) const noexcept
{
    return focusMinutes == other.focusMinutes
        && sound == other.sound
        && soundscapeId == other.soundscapeId;
}

bool TaskRecord::operator==(const TaskRecord& other) const noexcept
{
    return id == other.id
        && title == other.title
        && status == other.status
        && estimatedPomodoros == other.estimatedPomodoros
        && completedPomodoros == other.completedPomodoros
        && sortOrder == other.sortOrder
        && revision == other.revision
        && createdAtUtcMs == other.createdAtUtcMs
        && updatedAtUtcMs == other.updatedAtUtcMs
        && execution == other.execution;
}

bool FocusSessionRecord::operator==(const FocusSessionRecord& other) const noexcept
{
    return id == other.id
        && taskId == other.taskId
        && titleSnapshot == other.titleSnapshot
        && type == other.type
        && plannedDurationMs == other.plannedDurationMs
        && startedAtUtcMs == other.startedAtUtcMs
        && targetEndAtUtcMs == other.targetEndAtUtcMs
        && remainingMs == other.remainingMs
        && status == other.status
        && endedAtUtcMs == other.endedAtUtcMs
        && completionReason == other.completionReason
        && idempotencyKey == other.idempotencyKey
        && soundscapeIdSnapshot == other.soundscapeIdSnapshot
        && interruptions == other.interruptions;
}

bool InterruptionEvent::operator==(
    const InterruptionEvent& other) const noexcept
{
    return reason == other.reason
        && note == other.note
        && occurredAtUtcMs == other.occurredAtUtcMs
        && detectedAtUtcMs == other.detectedAtUtcMs
        && source == other.source;
}

bool TimerSnapshot::operator==(const TimerSnapshot& other) const noexcept
{
    return schemaVersion == other.schemaVersion
        && sessionId == other.sessionId
        && status == other.status
        && savedAtUtcMs == other.savedAtUtcMs
        && targetEndAtUtcMs == other.targetEndAtUtcMs
        && remainingMs == other.remainingMs;
}

bool FocusSettings::operator==(const FocusSettings& other) const noexcept
{
    return focusMinutes == other.focusMinutes
        && shortBreakMinutes == other.shortBreakMinutes
        && longBreakMinutes == other.longBreakMinutes
        && longBreakEvery == other.longBreakEvery
        && soundVolumePercent == other.soundVolumePercent
        && autoStartBreak == other.autoStartBreak
        && launchAtLogin == other.launchAtLogin
        && defaultSoundscapeId == other.defaultSoundscapeId;
}

bool FocusData::operator==(const FocusData& other) const noexcept
{
    return schemaVersion == other.schemaVersion
        && settings == other.settings
        && tasks == other.tasks
        && sessions == other.sessions
        && activeSessionId == other.activeSessionId
        && timerSnapshot == other.timerSnapshot;
}

bool isActiveSessionStatus(SessionStatus status) noexcept
{
    return status == SessionStatus::Running
        || status == SessionStatus::Paused
        || status == SessionStatus::CompletionPending;
}

bool isTerminalSessionStatus(SessionStatus status) noexcept
{
    return status == SessionStatus::Completed
        || status == SessionStatus::Aborted
        || status == SessionStatus::Skipped;
}

int effectiveFocusMinutes(
    const TaskRecord& task,
    const FocusSettings& settings) noexcept
{
    return task.execution.focusMinutes.value_or(settings.focusMinutes);
}

std::optional<std::string> effectiveSoundscapeId(
    const TaskRecord& task,
    const FocusSettings& settings)
{
    switch (task.execution.sound) {
    case TaskSoundPreference::Inherit:
        if (!settings.defaultSoundscapeId.empty()) {
            return settings.defaultSoundscapeId;
        }
        return std::nullopt;
    case TaskSoundPreference::Off:
        return std::nullopt;
    case TaskSoundPreference::Soundscape:
        return task.execution.soundscapeId;
    }
    return std::nullopt;
}

} // namespace whatsui::focus_tomato
