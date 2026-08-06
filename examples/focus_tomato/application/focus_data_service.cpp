#include "focus_data_service.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace whatsui::focus_tomato {
namespace {

DataCommandResult commandResult(DataCommandStatus status, std::string message = {})
{
    return {status, {}, std::move(message)};
}

std::string sessionTitle(const FocusData& data, const StartSessionCommand& command)
{
    if (command.taskId) {
        const auto task = std::find_if(data.tasks.begin(), data.tasks.end(),
                                       [&command](const TaskRecord& item) {
                                           return item.id == *command.taskId;
                                       });
        return task == data.tasks.end() ? std::string{} : task->title;
    }
    switch (command.type) {
    case SessionType::Focus: return "自由专注";
    case SessionType::ShortBreak: return "短休息";
    case SessionType::LongBreak: return "长休息";
    }
    return {};
}

} // namespace

FocusDataService::FocusDataService(FocusRepository& repository, FocusData initialData)
    : repository_(repository)
    , data_(std::move(initialData))
{
    const auto report = validateFocusData(data_);
    if (!report.ok()) {
        throw std::invalid_argument("FocusDataService received invalid initial data:\n"
                                    + report.summary());
    }
}

const FocusData& FocusDataService::data() const noexcept
{
    return data_;
}

DataCommandResult FocusDataService::addTask(const AddTaskCommand& command)
{
    if (command.taskId.empty() || command.nowUtcMs <= 0
        || command.estimatedPomodoros < 1 || command.estimatedPomodoros > 99) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Task ID, estimate, or creation time is invalid.");
    }
    const auto existing = std::find_if(data_.tasks.begin(), data_.tasks.end(),
                                       [&command](const TaskRecord& item) {
                                           return item.id == command.taskId;
                                       });
    if (existing != data_.tasks.end()) {
        if (existing->title == command.title
            && existing->estimatedPomodoros == command.estimatedPomodoros
            && existing->createdAtUtcMs == command.nowUtcMs
            && existing->execution == command.execution) {
            return commandResult(DataCommandStatus::NoChange,
                                 "This add-task command has already been applied.");
        }
        return commandResult(
            DataCommandStatus::Conflict,
            "The task ID is already associated with different task data.");
    }

    std::int64_t sortOrder = 1024;
    for (const auto& task : data_.tasks) {
        if (task.status != TaskStatus::Active) continue;
        if (task.sortOrder > std::numeric_limits<std::int64_t>::max() - 1024) {
            return commandResult(DataCommandStatus::Conflict,
                                 "Task ordering must be compacted before adding another task.");
        }
        sortOrder = std::max(sortOrder, task.sortOrder + 1024);
    }
    FocusData candidate = data_;
    candidate.tasks.push_back({
        command.taskId,
        command.title,
        TaskStatus::Active,
        command.estimatedPomodoros,
        0,
        sortOrder,
        1,
        command.nowUtcMs,
        command.nowUtcMs,
        command.execution,
    });
    return commit(std::move(candidate));
}

DataCommandResult FocusDataService::updateTask(
    const UpdateTaskCommand& command)
{
    if (command.taskId.empty() || command.expectedRevision < 1
        || command.nowUtcMs <= 0 || command.estimatedPomodoros < 1
        || command.estimatedPomodoros > 99) {
        return commandResult(
            DataCommandStatus::InvalidArgument,
            "Task ID, estimate, revision, or update time is invalid.");
    }
    FocusData candidate = data_;
    const auto task = std::find_if(
        candidate.tasks.begin(), candidate.tasks.end(),
        [&command](const TaskRecord& item) {
            return item.id == command.taskId;
        });
    if (task == candidate.tasks.end()) {
        return commandResult(DataCommandStatus::NotFound,
                             "Task was not found.");
    }
    if (isArchivedTaskStatus(task->status)) {
        return commandResult(
            DataCommandStatus::Conflict,
            "A deleted task must be restored before editing.");
    }
    if (task->revision != command.expectedRevision) {
        return commandResult(
            DataCommandStatus::Conflict,
            "Task changed after the editor was opened.");
    }
    if (command.nowUtcMs < task->updatedAtUtcMs) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Task update time must not move backwards.");
    }
    if (task->title == command.title
        && task->estimatedPomodoros == command.estimatedPomodoros
        && task->execution == command.execution) {
        return commandResult(DataCommandStatus::NoChange,
                             "Task already contains these values.");
    }
    task->title = command.title;
    task->estimatedPomodoros = command.estimatedPomodoros;
    task->execution = command.execution;
    ++task->revision;
    task->updatedAtUtcMs = command.nowUtcMs;
    return commit(std::move(candidate));
}

DataCommandResult FocusDataService::archiveTask(const std::string& taskId,
                                                std::int64_t expectedRevision,
                                                std::int64_t nowUtcMs)
{
    if (taskId.empty() || expectedRevision < 1 || nowUtcMs <= 0) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Task ID, expected revision, or update time is invalid.");
    }
    FocusData candidate = data_;
    const auto task = std::find_if(candidate.tasks.begin(), candidate.tasks.end(),
                                   [&taskId](const TaskRecord& item) {
                                       return item.id == taskId;
                                   });
    if (task == candidate.tasks.end()) {
        return commandResult(DataCommandStatus::NotFound, "Task was not found.");
    }
    if (isArchivedTaskStatus(task->status)) {
        return commandResult(DataCommandStatus::NoChange,
                             "Task has already been archived.");
    }
    if (task->revision != expectedRevision) {
        return commandResult(DataCommandStatus::Conflict,
                             "Task changed after the editor was opened.");
    }
    const auto activeSession = std::find_if(
        candidate.sessions.begin(), candidate.sessions.end(),
        [&candidate, &taskId](const FocusSessionRecord& session) {
            return candidate.activeSessionId
                && session.id == *candidate.activeSessionId
                && session.taskId
                && *session.taskId == taskId;
        });
    if (activeSession != candidate.sessions.end()) {
        return commandResult(
            DataCommandStatus::Conflict,
            "A task used by the active timer cannot be deleted.");
    }
    if (nowUtcMs < task->updatedAtUtcMs) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Task update time must not move backwards.");
    }
    task->status = task->status == TaskStatus::Done
        ? TaskStatus::ArchivedDone
        : TaskStatus::Archived;
    ++task->revision;
    task->updatedAtUtcMs = nowUtcMs;
    return commit(std::move(candidate));
}

DataCommandResult FocusDataService::restoreTask(
    const std::string& taskId,
    std::int64_t expectedRevision,
    std::int64_t nowUtcMs)
{
    if (taskId.empty() || expectedRevision < 1 || nowUtcMs <= 0) {
        return commandResult(
            DataCommandStatus::InvalidArgument,
            "Task ID, expected revision, or update time is invalid.");
    }
    FocusData candidate = data_;
    const auto task = std::find_if(
        candidate.tasks.begin(), candidate.tasks.end(),
        [&taskId](const TaskRecord& item) { return item.id == taskId; });
    if (task == candidate.tasks.end()) {
        return commandResult(DataCommandStatus::NotFound,
                             "Task was not found.");
    }
    if (!isArchivedTaskStatus(task->status)) {
        return commandResult(DataCommandStatus::NoChange,
                             "Task is already visible in the task list.");
    }
    if (task->revision != expectedRevision) {
        return commandResult(
            DataCommandStatus::Conflict,
            "Task changed after the deleted list was displayed.");
    }
    if (nowUtcMs < task->updatedAtUtcMs) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Task update time must not move backwards.");
    }
    std::int64_t sortOrder = 1024;
    for (const auto& item : candidate.tasks) {
        if (item.status != TaskStatus::Active) continue;
        if (item.sortOrder > std::numeric_limits<std::int64_t>::max() - 1024) {
            return commandResult(
                DataCommandStatus::Conflict,
                "Task ordering must be compacted before restoring this task.");
        }
        sortOrder = std::max(sortOrder, item.sortOrder + 1024);
    }
    task->status = task->status == TaskStatus::ArchivedDone
        ? TaskStatus::Done
        : TaskStatus::Active;
    task->sortOrder = sortOrder;
    ++task->revision;
    task->updatedAtUtcMs = nowUtcMs;
    return commit(std::move(candidate));
}

DataCommandResult FocusDataService::updateSettings(FocusSettings settings)
{
    if (settings == data_.settings) {
        return commandResult(DataCommandStatus::NoChange,
                             "Settings already contain these values.");
    }
    FocusData candidate = data_;
    candidate.settings = std::move(settings);
    return commit(std::move(candidate));
}

DataCommandResult FocusDataService::setTaskCompletion(
    const std::string& taskId,
    bool completed,
    std::int64_t expectedRevision,
    std::int64_t nowUtcMs)
{
    if (taskId.empty() || expectedRevision < 1 || nowUtcMs <= 0) {
        return commandResult(
            DataCommandStatus::InvalidArgument,
            "Task ID, expected revision, or update time is invalid.");
    }
    FocusData candidate = data_;
    const auto task = std::find_if(
        candidate.tasks.begin(), candidate.tasks.end(),
        [&taskId](const TaskRecord& item) { return item.id == taskId; });
    if (task == candidate.tasks.end()) {
        return commandResult(DataCommandStatus::NotFound, "Task was not found.");
    }
    if (isArchivedTaskStatus(task->status)) {
        return commandResult(
            DataCommandStatus::Conflict,
            "An archived task must be restored before changing completion.");
    }
    const TaskStatus target = completed ? TaskStatus::Done : TaskStatus::Active;
    if (task->status == target) {
        return commandResult(DataCommandStatus::NoChange,
                             "Task completion already has this value.");
    }
    if (task->revision != expectedRevision) {
        return commandResult(DataCommandStatus::Conflict,
                             "Task changed after the row was displayed.");
    }
    task->status = target;
    ++task->revision;
    task->updatedAtUtcMs = nowUtcMs;
    return commit(std::move(candidate));
}

DataCommandResult FocusDataService::startSession(const StartSessionCommand& command)
{
    if (command.sessionId.empty() || command.nowUtcMs <= 0
        || command.plannedDurationMs < kMinuteMs
        || command.plannedDurationMs > 180 * kMinuteMs
        || command.nowUtcMs > std::numeric_limits<std::int64_t>::max()
                                      - command.plannedDurationMs) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Session ID, start time, or duration is invalid.");
    }
    if (const FocusSessionRecord* existing = findSession(command.sessionId)) {
        if (existing->taskId == command.taskId
            && existing->type == command.type
            && existing->plannedDurationMs == command.plannedDurationMs
            && existing->startedAtUtcMs == command.nowUtcMs
            && existing->idempotencyKey == command.sessionId
            && existing->soundscapeIdSnapshot == command.soundscapeId) {
            return commandResult(DataCommandStatus::NoChange,
                                 "This start command has already been applied.");
        }
        return commandResult(
            DataCommandStatus::Conflict,
            "The session ID is already associated with a different start command.");
    }
    if (data_.activeSessionId) {
        return commandResult(DataCommandStatus::Conflict,
                             "Another focus or break session is already active.");
    }
    if (command.taskId) {
        const auto task = std::find_if(data_.tasks.begin(), data_.tasks.end(),
                                       [&command](const TaskRecord& item) {
                                           return item.id == *command.taskId;
                                       });
        if (task == data_.tasks.end()) {
            return commandResult(DataCommandStatus::NotFound,
                                 "The selected task no longer exists.");
        }
        if (task->status != TaskStatus::Active) {
            return commandResult(DataCommandStatus::Conflict,
                                 "Only an active task can start a new focus session.");
        }
    }
    if (command.type != SessionType::Focus && command.taskId) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Break sessions must not modify a task.");
    }

    FocusData candidate = data_;
    const std::optional<std::int64_t> targetEndAt = command.startPaused
        ? std::nullopt
        : std::optional<std::int64_t>{
            command.nowUtcMs + command.plannedDurationMs};
    const SessionStatus initialStatus = command.startPaused
        ? SessionStatus::Paused
        : SessionStatus::Running;
    candidate.sessions.push_back({
        command.sessionId,
        command.taskId,
        sessionTitle(candidate, command),
        command.type,
        command.plannedDurationMs,
        command.nowUtcMs,
        targetEndAt,
        command.plannedDurationMs,
        initialStatus,
        std::nullopt,
        CompletionReason::None,
        command.sessionId,
        command.soundscapeId,
    });
    candidate.activeSessionId = command.sessionId;
    candidate.timerSnapshot = TimerSnapshot{
        kCurrentSchemaVersion,
        command.sessionId,
        initialStatus,
        command.nowUtcMs,
        targetEndAt,
        command.plannedDurationMs,
    };
    return commit(std::move(candidate));
}

DataCommandResult FocusDataService::pauseSession(const std::string& sessionId,
                                                 std::int64_t nowUtcMs)
{
    if (nowUtcMs <= 0) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Pause time must be positive.");
    }
    FocusData candidate = data_;
    FocusSessionRecord* session = findSession(candidate, sessionId);
    if (session == nullptr) {
        return commandResult(DataCommandStatus::NotFound, "Session was not found.");
    }
    if (!candidate.activeSessionId || *candidate.activeSessionId != sessionId
        || session->status != SessionStatus::Running || !session->targetEndAtUtcMs) {
        return commandResult(DataCommandStatus::Conflict,
                             "Only the current running session can be paused.");
    }
    const std::int64_t remaining = std::max<std::int64_t>(
        0, *session->targetEndAtUtcMs - nowUtcMs);
    if (remaining == 0) {
        return commandResult(DataCommandStatus::Conflict,
                             "The deadline has already been reached.");
    }
    session->status = SessionStatus::Paused;
    session->remainingMs = remaining;
    session->targetEndAtUtcMs.reset();
    candidate.timerSnapshot = TimerSnapshot{
        kCurrentSchemaVersion,
        sessionId,
        SessionStatus::Paused,
        nowUtcMs,
        std::nullopt,
        remaining,
    };
    return commit(std::move(candidate));
}

DataCommandResult FocusDataService::resumeSession(const std::string& sessionId,
                                                  std::int64_t nowUtcMs)
{
    if (nowUtcMs <= 0) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Resume time must be positive.");
    }
    FocusData candidate = data_;
    FocusSessionRecord* session = findSession(candidate, sessionId);
    if (session == nullptr) {
        return commandResult(DataCommandStatus::NotFound, "Session was not found.");
    }
    if (!candidate.activeSessionId || *candidate.activeSessionId != sessionId
        || session->status != SessionStatus::Paused) {
        return commandResult(DataCommandStatus::Conflict,
                             "Only the current paused session can be resumed.");
    }
    if (nowUtcMs > std::numeric_limits<std::int64_t>::max() - session->remainingMs) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Resume deadline would overflow storage.");
    }
    const std::int64_t targetEndAt = nowUtcMs + session->remainingMs;
    session->status = SessionStatus::Running;
    session->targetEndAtUtcMs = targetEndAt;
    candidate.timerSnapshot = TimerSnapshot{
        kCurrentSchemaVersion,
        sessionId,
        SessionStatus::Running,
        nowUtcMs,
        targetEndAt,
        session->remainingMs,
    };
    return commit(std::move(candidate));
}

DataCommandResult FocusDataService::resetSession(const std::string& sessionId,
                                                 std::int64_t nowUtcMs)
{
    if (nowUtcMs <= 0) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Reset time must be positive.");
    }
    FocusData candidate = data_;
    FocusSessionRecord* session = findSession(candidate, sessionId);
    if (session == nullptr) {
        return commandResult(DataCommandStatus::NotFound, "Session was not found.");
    }
    if (!candidate.activeSessionId || *candidate.activeSessionId != sessionId
        || (session->status != SessionStatus::Running
            && session->status != SessionStatus::Paused)) {
        return commandResult(DataCommandStatus::Conflict,
                             "Only a running or paused active session can be reset.");
    }

    session->remainingMs = session->plannedDurationMs;
    std::optional<std::int64_t> targetEndAt;
    if (session->status == SessionStatus::Running) {
        if (nowUtcMs > std::numeric_limits<std::int64_t>::max()
                           - session->plannedDurationMs) {
            return commandResult(DataCommandStatus::InvalidArgument,
                                 "Reset deadline would overflow storage.");
        }
        targetEndAt = nowUtcMs + session->plannedDurationMs;
        session->targetEndAtUtcMs = targetEndAt;
    } else {
        session->targetEndAtUtcMs.reset();
    }
    candidate.timerSnapshot = TimerSnapshot{
        kCurrentSchemaVersion,
        sessionId,
        session->status,
        nowUtcMs,
        targetEndAt,
        session->plannedDurationMs,
    };
    return commit(std::move(candidate));
}

DataCommandResult FocusDataService::markDeadlineReached(const std::string& sessionId,
                                                        std::int64_t nowUtcMs)
{
    if (nowUtcMs <= 0) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Deadline observation time must be positive.");
    }
    FocusData candidate = data_;
    FocusSessionRecord* session = findSession(candidate, sessionId);
    if (session == nullptr) {
        return commandResult(DataCommandStatus::NotFound, "Session was not found.");
    }
    if (session->status == SessionStatus::CompletionPending
        || session->status == SessionStatus::Completed) {
        return commandResult(DataCommandStatus::NoChange,
                             "The deadline has already been handled.");
    }
    if (!candidate.activeSessionId || *candidate.activeSessionId != sessionId
        || session->status != SessionStatus::Running || !session->targetEndAtUtcMs) {
        return commandResult(DataCommandStatus::Conflict,
                             "Only the current running session can reach its deadline.");
    }
    if (nowUtcMs < *session->targetEndAtUtcMs) {
        return commandResult(DataCommandStatus::Conflict,
                             "The session deadline has not been reached.");
    }

    session->status = SessionStatus::CompletionPending;
    session->targetEndAtUtcMs.reset();
    session->remainingMs = 0;
    session->completionReason = CompletionReason::Natural;
    candidate.timerSnapshot = TimerSnapshot{
        kCurrentSchemaVersion,
        sessionId,
        SessionStatus::CompletionPending,
        nowUtcMs,
        std::nullopt,
        0,
    };
    return commit(std::move(candidate));
}

DataCommandResult FocusDataService::finalizeCompletion(const std::string& sessionId,
                                                       std::int64_t nowUtcMs)
{
    if (nowUtcMs <= 0) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Completion time must be positive.");
    }
    const FocusSessionRecord* current = findSession(sessionId);
    if (current == nullptr) {
        return commandResult(DataCommandStatus::NotFound, "Session was not found.");
    }
    if (current->status == SessionStatus::Completed) {
        return commandResult(DataCommandStatus::NoChange,
                             "Completion has already been committed.");
    }
    if (!data_.activeSessionId || *data_.activeSessionId != sessionId
        || current->status != SessionStatus::CompletionPending
        || nowUtcMs < current->startedAtUtcMs) {
        return commandResult(DataCommandStatus::Conflict,
                             "Session is not ready for completion.");
    }

    FocusData candidate = data_;
    FocusSessionRecord* session = findSession(candidate, sessionId);
    session->status = SessionStatus::Completed;
    session->endedAtUtcMs = nowUtcMs;
    candidate.activeSessionId.reset();
    candidate.timerSnapshot.reset();
    if (session->type == SessionType::Focus && session->taskId) {
        const auto task = std::find_if(candidate.tasks.begin(), candidate.tasks.end(),
                                       [session](const TaskRecord& item) {
                                           return item.id == *session->taskId;
                                       });
        if (task == candidate.tasks.end()) {
            return commandResult(DataCommandStatus::NotFound,
                                 "The session task no longer exists.");
        }
        ++task->completedPomodoros;
        ++task->revision;
        task->updatedAtUtcMs = nowUtcMs;
    }
    return commit(std::move(candidate));
}

DataCommandResult FocusDataService::abortSession(const std::string& sessionId,
                                                 std::int64_t nowUtcMs)
{
    if (nowUtcMs <= 0) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Abort time must be positive.");
    }
    const FocusSessionRecord* current = findSession(sessionId);
    if (current == nullptr) {
        return commandResult(DataCommandStatus::NotFound, "Session was not found.");
    }
    if (current->status == SessionStatus::Aborted) {
        return commandResult(DataCommandStatus::NoChange,
                             "Session has already been aborted.");
    }
    if (!data_.activeSessionId || *data_.activeSessionId != sessionId
        || (current->status != SessionStatus::Running
            && current->status != SessionStatus::Paused)
        || nowUtcMs < current->startedAtUtcMs) {
        return commandResult(DataCommandStatus::Conflict,
                             "Only the current active session can be aborted.");
    }

    FocusData candidate = data_;
    FocusSessionRecord* session = findSession(candidate, sessionId);
    session->status = SessionStatus::Aborted;
    session->targetEndAtUtcMs.reset();
    session->endedAtUtcMs = nowUtcMs;
    session->completionReason = CompletionReason::UserAborted;
    candidate.activeSessionId.reset();
    candidate.timerSnapshot.reset();
    return commit(std::move(candidate));
}

DataCommandResult FocusDataService::skipBreakSession(
    const std::string& sessionId,
    std::int64_t nowUtcMs)
{
    if (sessionId.empty() || nowUtcMs <= 0) {
        return commandResult(
            DataCommandStatus::InvalidArgument,
            "Session ID or skip time is invalid.");
    }
    const FocusSessionRecord* current = findSession(sessionId);
    if (current == nullptr) {
        return commandResult(
            DataCommandStatus::NotFound,
            "The break session was not found.");
    }
    if (current->type == SessionType::Focus) {
        return commandResult(
            DataCommandStatus::Conflict,
            "A focus session cannot use the break-only skip transition.");
    }
    if (current->status == SessionStatus::Skipped) {
        return commandResult(
            DataCommandStatus::NoChange,
            "The break session has already been skipped.");
    }
    if (!data_.activeSessionId || *data_.activeSessionId != sessionId
        || (current->status != SessionStatus::Running
            && current->status != SessionStatus::Paused)
        || nowUtcMs < current->startedAtUtcMs) {
        return commandResult(
            DataCommandStatus::Conflict,
            "Only the current active break can be skipped.");
    }

    FocusData candidate = data_;
    FocusSessionRecord* session = findSession(candidate, sessionId);
    session->status = SessionStatus::Skipped;
    session->targetEndAtUtcMs.reset();
    session->endedAtUtcMs = nowUtcMs;
    session->completionReason = CompletionReason::UserSkipped;
    candidate.activeSessionId.reset();
    candidate.timerSnapshot.reset();
    return commit(std::move(candidate));
}

DataCommandResult FocusDataService::recordInterruption(
    const RecordInterruptionCommand& command)
{
    if (command.sessionId.empty() || command.nowUtcMs <= 0) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Session ID or record time is invalid.");
    }
    if (command.event.occurredAtUtcMs <= 0
        || command.event.detectedAtUtcMs <= 0
        || command.event.detectedAtUtcMs < command.event.occurredAtUtcMs) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Interruption timestamps must be positive and "
                             "detectedAt must not precede occurredAt.");
    }
    const FocusSessionRecord* current = findSession(command.sessionId);
    if (current == nullptr) {
        return commandResult(DataCommandStatus::NotFound,
                             "Session was not found.");
    }
    if (!data_.activeSessionId || *data_.activeSessionId != command.sessionId
        || current->status != SessionStatus::Paused) {
        return commandResult(DataCommandStatus::Conflict,
                             "Only the current paused session can record an "
                             "interruption.");
    }
    if (command.decision == ResumeDecision::SkipRest
        && current->type == SessionType::Focus) {
        return commandResult(DataCommandStatus::Conflict,
                             "SkipRest is only valid for break sessions.");
    }
    if (command.decision == ResumeDecision::Continue
        && command.nowUtcMs > std::numeric_limits<std::int64_t>::max()
            - current->remainingMs) {
        return commandResult(DataCommandStatus::InvalidArgument,
                             "Resume deadline would overflow storage.");
    }
    if (command.nowUtcMs < current->startedAtUtcMs) {
        return commandResult(DataCommandStatus::Conflict,
                             "Now cannot precede the session start.");
    }

    FocusData candidate = data_;
    FocusSessionRecord* session = findSession(candidate, command.sessionId);
    session->interruptions.push_back(command.event);
    switch (command.decision) {
    case ResumeDecision::Continue: {
        const std::int64_t targetEndAt = command.nowUtcMs + session->remainingMs;
        session->status = SessionStatus::Running;
        session->targetEndAtUtcMs = targetEndAt;
        candidate.timerSnapshot = TimerSnapshot{
            kCurrentSchemaVersion,
            command.sessionId,
            SessionStatus::Running,
            command.nowUtcMs,
            targetEndAt,
            session->remainingMs,
        };
        break;
    }
    case ResumeDecision::EndSession:
        session->status = SessionStatus::Aborted;
        session->targetEndAtUtcMs.reset();
        session->endedAtUtcMs = command.nowUtcMs;
        session->completionReason = CompletionReason::UserAborted;
        candidate.activeSessionId.reset();
        candidate.timerSnapshot.reset();
        break;
    case ResumeDecision::SkipRest:
        session->status = SessionStatus::Skipped;
        session->targetEndAtUtcMs.reset();
        session->endedAtUtcMs = command.nowUtcMs;
        session->completionReason = CompletionReason::UserSkipped;
        candidate.activeSessionId.reset();
        candidate.timerSnapshot.reset();
        break;
    }
    return commit(std::move(candidate));
}

DataCommandResult FocusDataService::commit(FocusData candidate)
{
    ValidationReport report = validateFocusData(candidate);
    if (!report.ok()) {
        return {DataCommandStatus::ValidationRejected, std::move(report),
                "Domain validation rejected the complete mutation."};
    }

    RepositoryWriteResult write = repository_.save(candidate);
    if (!write.succeeded()) {
        const DataCommandStatus status =
            write.status == RepositoryWriteStatus::ValidationRejected
                ? DataCommandStatus::ValidationRejected
                : DataCommandStatus::PersistenceFailed;
        return {status, std::move(write.validation), std::move(write.message)};
    }
    data_ = std::move(candidate);
    return commandResult(DataCommandStatus::Success);
}

FocusSessionRecord* FocusDataService::findSession(FocusData& data,
                                                  const std::string& sessionId) const
{
    const auto session = std::find_if(data.sessions.begin(), data.sessions.end(),
                                      [&sessionId](const FocusSessionRecord& item) {
                                          return item.id == sessionId;
                                      });
    return session == data.sessions.end() ? nullptr : &*session;
}

const FocusSessionRecord* FocusDataService::findSession(const std::string& sessionId) const
{
    const auto session = std::find_if(data_.sessions.begin(), data_.sessions.end(),
                                      [&sessionId](const FocusSessionRecord& item) {
                                          return item.id == sessionId;
                                      });
    return session == data_.sessions.end() ? nullptr : &*session;
}

} // namespace whatsui::focus_tomato
