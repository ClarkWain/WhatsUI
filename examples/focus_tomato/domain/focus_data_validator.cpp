#include "focus_data_validator.h"

#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace whatsui::focus_tomato {
namespace {

void issue(ValidationReport& report,
           ValidationCode code,
           std::string entityType,
           std::string entityId,
           std::string field,
           std::string message,
           ValidationSeverity severity = ValidationSeverity::Error)
{
    report.add({severity, code, std::move(entityType), std::move(entityId),
                std::move(field), std::move(message)});
}

bool validTaskStatus(TaskStatus value) noexcept
{
    switch (value) {
    case TaskStatus::Active:
    case TaskStatus::Done:
    case TaskStatus::Archived:
        return true;
    }
    return false;
}

bool validSessionType(SessionType value) noexcept
{
    switch (value) {
    case SessionType::Focus:
    case SessionType::ShortBreak:
    case SessionType::LongBreak:
        return true;
    }
    return false;
}

bool validSessionStatus(SessionStatus value) noexcept
{
    switch (value) {
    case SessionStatus::Pending:
    case SessionStatus::Running:
    case SessionStatus::Paused:
    case SessionStatus::CompletionPending:
    case SessionStatus::Completed:
    case SessionStatus::Aborted:
    case SessionStatus::Skipped:
        return true;
    }
    return false;
}

bool validCompletionReason(CompletionReason value) noexcept
{
    switch (value) {
    case CompletionReason::None:
    case CompletionReason::Natural:
    case CompletionReason::Manual:
    case CompletionReason::Recovered:
    case CompletionReason::UserAborted:
    case CompletionReason::UserSkipped:
        return true;
    }
    return false;
}

struct TextScan {
    bool validUtf8{true};
    bool forbiddenControl{false};
    std::size_t visibleCodepoints{0};
};

bool isContinuation(unsigned char byte) noexcept
{
    return (byte & 0xC0U) == 0x80U;
}

bool isForbiddenControl(std::uint32_t codepoint) noexcept
{
    if (codepoint < 0x20U && codepoint != '\t' && codepoint != '\n' && codepoint != '\r') {
        return true;
    }
    if (codepoint >= 0x7FU && codepoint <= 0x9FU) return true;
    if (codepoint == 0x200EU || codepoint == 0x200FU) return true;
    if (codepoint >= 0x202AU && codepoint <= 0x202EU) return true;
    return codepoint >= 0x2066U && codepoint <= 0x2069U;
}

TextScan scanText(std::string_view value)
{
    TextScan scan;
    std::size_t index = 0;
    while (index < value.size()) {
        const auto lead = static_cast<unsigned char>(value[index]);
        std::uint32_t codepoint = 0;
        std::size_t length = 0;
        if (lead <= 0x7FU) {
            codepoint = lead;
            length = 1;
        } else if (lead >= 0xC2U && lead <= 0xDFU) {
            if (index + 1 >= value.size()
                || !isContinuation(static_cast<unsigned char>(value[index + 1]))) {
                scan.validUtf8 = false;
                break;
            }
            codepoint = ((lead & 0x1FU) << 6)
                | (static_cast<unsigned char>(value[index + 1]) & 0x3FU);
            length = 2;
        } else if (lead >= 0xE0U && lead <= 0xEFU) {
            if (index + 2 >= value.size()) {
                scan.validUtf8 = false;
                break;
            }
            const auto second = static_cast<unsigned char>(value[index + 1]);
            const auto third = static_cast<unsigned char>(value[index + 2]);
            if (!isContinuation(second) || !isContinuation(third)
                || (lead == 0xE0U && second < 0xA0U)
                || (lead == 0xEDU && second >= 0xA0U)) {
                scan.validUtf8 = false;
                break;
            }
            codepoint = ((lead & 0x0FU) << 12) | ((second & 0x3FU) << 6) | (third & 0x3FU);
            length = 3;
        } else if (lead >= 0xF0U && lead <= 0xF4U) {
            if (index + 3 >= value.size()) {
                scan.validUtf8 = false;
                break;
            }
            const auto second = static_cast<unsigned char>(value[index + 1]);
            const auto third = static_cast<unsigned char>(value[index + 2]);
            const auto fourth = static_cast<unsigned char>(value[index + 3]);
            if (!isContinuation(second) || !isContinuation(third) || !isContinuation(fourth)
                || (lead == 0xF0U && second < 0x90U)
                || (lead == 0xF4U && second >= 0x90U)) {
                scan.validUtf8 = false;
                break;
            }
            codepoint = ((lead & 0x07U) << 18) | ((second & 0x3FU) << 12)
                | ((third & 0x3FU) << 6) | (fourth & 0x3FU);
            length = 4;
        } else {
            scan.validUtf8 = false;
            break;
        }

        scan.forbiddenControl = scan.forbiddenControl || isForbiddenControl(codepoint);
        if (codepoint != ' ' && codepoint != '\t' && codepoint != '\r' && codepoint != '\n') {
            ++scan.visibleCodepoints;
        }
        index += length;
    }
    return scan;
}

void validateSettings(const FocusSettings& settings, ValidationReport& report)
{
    const auto duration = [&report](int value, const char* field) {
        if (value < 1 || value > 180) {
            issue(report, ValidationCode::ValueOutOfRange, "settings", "global", field,
                  "Duration must be between 1 and 180 minutes.");
        }
    };
    duration(settings.focusMinutes, "focusMinutes");
    duration(settings.shortBreakMinutes, "shortBreakMinutes");
    duration(settings.longBreakMinutes, "longBreakMinutes");
    if (settings.longBreakEvery < 1 || settings.longBreakEvery > 12) {
        issue(report, ValidationCode::ValueOutOfRange, "settings", "global", "longBreakEvery",
              "Long-break interval must be between 1 and 12 completed focus sessions.");
    }
    if (settings.soundVolumePercent < 0 || settings.soundVolumePercent > 100) {
        issue(report, ValidationCode::ValueOutOfRange, "settings", "global", "soundVolumePercent",
              "Sound volume must be between 0 and 100 percent.");
    }
}

void validateTask(const TaskRecord& task, ValidationReport& report)
{
    if (task.id.empty()) {
        issue(report, ValidationCode::MissingId, "task", {}, "id", "Task ID must not be empty.");
    }
    if (!validTaskStatus(task.status)) {
        issue(report, ValidationCode::InvalidEnumValue, "task", task.id, "status",
              "Task status is not a supported enum value.");
    }

    const TextScan title = scanText(task.title);
    if (!title.validUtf8) {
        issue(report, ValidationCode::InvalidUtf8, "task", task.id, "title",
              "Task title is not valid UTF-8.");
    } else {
        if (title.visibleCodepoints == 0) {
            issue(report, ValidationCode::EmptyTaskTitle, "task", task.id, "title",
                  "Task title must contain visible text.");
        }
        if (title.visibleCodepoints > 80) {
            issue(report, ValidationCode::ValueOutOfRange, "task", task.id, "title",
                  "Task title must contain at most 80 visible code points.");
        }
        if (title.forbiddenControl) {
            issue(report, ValidationCode::ForbiddenTextControl, "task", task.id, "title",
                  "Task title contains a control that can hide or reorder text.");
        }
    }

    if (task.estimatedPomodoros < 1 || task.estimatedPomodoros > 100) {
        issue(report, ValidationCode::ValueOutOfRange, "task", task.id, "estimatedPomodoros",
              "Estimated pomodoros must be between 1 and 100.");
    }
    if (task.completedPomodoros < 0) {
        issue(report, ValidationCode::ValueOutOfRange, "task", task.id, "completedPomodoros",
              "Completed pomodoros must not be negative.");
    }
    if (task.revision < 1) {
        issue(report, ValidationCode::ValueOutOfRange, "task", task.id, "revision",
              "Task revision must be positive.");
    }
    if (task.createdAtUtcMs <= 0 || task.updatedAtUtcMs < task.createdAtUtcMs) {
        issue(report, ValidationCode::InvalidTimestampOrder, "task", task.id, "updatedAtUtcMs",
              "Task timestamps must be positive and updatedAt must not precede createdAt.");
    }
}

void validateSessionFields(const FocusSessionRecord& session, ValidationReport& report)
{
    if (session.id.empty()) {
        issue(report, ValidationCode::MissingId, "session", {}, "id", "Session ID must not be empty.");
    }
    if (!validSessionType(session.type)) {
        issue(report, ValidationCode::InvalidEnumValue, "session", session.id, "type",
              "Session type is not a supported enum value.");
    }
    if (!validSessionStatus(session.status)) {
        issue(report, ValidationCode::InvalidEnumValue, "session", session.id, "status",
              "Session status is not a supported enum value.");
        return;
    }
    if (!validCompletionReason(session.completionReason)) {
        issue(report, ValidationCode::InvalidEnumValue, "session", session.id, "completionReason",
              "Completion reason is not a supported enum value.");
    }
    const TextScan title = scanText(session.titleSnapshot);
    if (!title.validUtf8) {
        issue(report, ValidationCode::InvalidUtf8, "session", session.id, "titleSnapshot",
              "Session title snapshot is not valid UTF-8.");
    } else {
        if (title.visibleCodepoints == 0) {
            issue(report, ValidationCode::EmptySessionTitleSnapshot, "session", session.id,
                  "titleSnapshot", "Session title snapshot must contain visible text.");
        }
        if (title.visibleCodepoints > 80) {
            issue(report, ValidationCode::ValueOutOfRange, "session", session.id, "titleSnapshot",
                  "Session title snapshot must contain at most 80 visible code points.");
        }
        if (title.forbiddenControl) {
            issue(report, ValidationCode::ForbiddenTextControl, "session", session.id,
                  "titleSnapshot",
                  "Session title snapshot contains a control that can hide or reorder text.");
        }
    }
    if (session.plannedDurationMs < kMinuteMs || session.plannedDurationMs > 180 * kMinuteMs) {
        issue(report, ValidationCode::ValueOutOfRange, "session", session.id, "plannedDurationMs",
              "Planned duration must be between 1 and 180 minutes.");
    }
    if (session.remainingMs < 0 || session.remainingMs > session.plannedDurationMs) {
        issue(report, ValidationCode::ValueOutOfRange, "session", session.id, "remainingMs",
              "Remaining time must be within the planned duration.");
    }
    if (session.idempotencyKey != session.id || session.idempotencyKey.empty()) {
        issue(report, ValidationCode::InvalidIdempotencyKey, "session", session.id, "idempotencyKey",
              "Session idempotency key must equal the session ID.");
    }

    bool validState = true;
    bool validReason = true;
    switch (session.status) {
    case SessionStatus::Pending:
        validState = session.startedAtUtcMs == 0
            && !session.targetEndAtUtcMs && !session.endedAtUtcMs
            && session.remainingMs == session.plannedDurationMs;
        validReason = session.completionReason == CompletionReason::None;
        break;
    case SessionStatus::Running:
        validState = session.startedAtUtcMs > 0
            && session.targetEndAtUtcMs
            && *session.targetEndAtUtcMs > session.startedAtUtcMs
            && !session.endedAtUtcMs
            && session.remainingMs > 0;
        validReason = session.completionReason == CompletionReason::None;
        break;
    case SessionStatus::Paused:
        validState = session.startedAtUtcMs > 0
            && !session.targetEndAtUtcMs && !session.endedAtUtcMs
            && session.remainingMs > 0;
        validReason = session.completionReason == CompletionReason::None;
        break;
    case SessionStatus::CompletionPending:
        validState = session.startedAtUtcMs > 0
            && !session.targetEndAtUtcMs && !session.endedAtUtcMs
            && session.remainingMs == 0;
        validReason = session.completionReason == CompletionReason::Natural
            || session.completionReason == CompletionReason::Recovered;
        break;
    case SessionStatus::Completed:
        validState = session.startedAtUtcMs > 0
            && !session.targetEndAtUtcMs && session.endedAtUtcMs
            && *session.endedAtUtcMs >= session.startedAtUtcMs
            && session.remainingMs == 0;
        validReason = session.completionReason == CompletionReason::Natural
            || session.completionReason == CompletionReason::Manual
            || session.completionReason == CompletionReason::Recovered;
        break;
    case SessionStatus::Aborted:
        validState = session.startedAtUtcMs > 0
            && !session.targetEndAtUtcMs && session.endedAtUtcMs
            && *session.endedAtUtcMs >= session.startedAtUtcMs;
        validReason = session.completionReason == CompletionReason::UserAborted;
        break;
    case SessionStatus::Skipped:
        validState = session.type != SessionType::Focus
            && session.startedAtUtcMs > 0
            && !session.targetEndAtUtcMs && session.endedAtUtcMs
            && *session.endedAtUtcMs >= session.startedAtUtcMs;
        validReason = session.completionReason == CompletionReason::UserSkipped;
        break;
    }
    if (!validState) {
        issue(report, ValidationCode::InvalidSessionState, "session", session.id, "status",
              "Session status conflicts with its timing fields.");
    }
    if (!validReason) {
        issue(report, ValidationCode::InvalidCompletionReason, "session", session.id, "completionReason",
              "Completion reason is not valid for the session status.");
    }
}

void validateSnapshot(const TimerSnapshot& snapshot,
                      const FocusSessionRecord* activeSession,
                      ValidationReport& report)
{
    if (snapshot.schemaVersion != kCurrentSchemaVersion) {
        issue(report, ValidationCode::UnsupportedSchemaVersion, "timerSnapshot",
              snapshot.sessionId, "schemaVersion", "Timer snapshot schema version is unsupported.");
    }
    if (!validSessionStatus(snapshot.status) || !isActiveSessionStatus(snapshot.status)) {
        issue(report, ValidationCode::InvalidSessionState, "timerSnapshot",
              snapshot.sessionId, "status", "Timer snapshot must describe an active session.");
        return;
    }
    if (snapshot.savedAtUtcMs <= 0) {
        issue(report, ValidationCode::InvalidTimestampOrder, "timerSnapshot",
              snapshot.sessionId, "savedAtUtcMs", "Timer snapshot save time must be positive.");
    }
    if (activeSession == nullptr) return;

    if (snapshot.sessionId != activeSession->id
        || snapshot.status != activeSession->status
        || snapshot.targetEndAtUtcMs != activeSession->targetEndAtUtcMs
        || snapshot.remainingMs != activeSession->remainingMs) {
        issue(report, ValidationCode::TimerSnapshotMismatch, "timerSnapshot",
              snapshot.sessionId, "sessionId",
              "Timer snapshot must exactly match the active session checkpoint.");
    }
}

} // namespace

void ValidationReport::add(ValidationIssue issue)
{
    issues_.push_back(std::move(issue));
}

bool ValidationReport::ok() const noexcept
{
    return errorCount() == 0;
}

bool ValidationReport::has(ValidationCode code) const noexcept
{
    return std::any_of(issues_.begin(), issues_.end(),
                       [code](const ValidationIssue& item) { return item.code == code; });
}

std::size_t ValidationReport::errorCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        issues_.begin(), issues_.end(),
        [](const ValidationIssue& item) { return item.severity == ValidationSeverity::Error; }));
}

std::size_t ValidationReport::warningCount() const noexcept
{
    return issues_.size() - errorCount();
}

const std::vector<ValidationIssue>& ValidationReport::issues() const noexcept
{
    return issues_;
}

std::string ValidationReport::summary() const
{
    if (issues_.empty()) return "validation passed";
    std::ostringstream output;
    output << errorCount() << " error(s), " << warningCount() << " warning(s)";
    for (const auto& item : issues_) {
        output << "\n[" << (item.severity == ValidationSeverity::Error ? "error" : "warning")
               << "] " << validationCodeName(item.code) << ' ' << item.entityType;
        if (!item.entityId.empty()) output << '#' << item.entityId;
        if (!item.field.empty()) output << '.' << item.field;
        output << ": " << item.message;
    }
    return output.str();
}

const char* validationCodeName(ValidationCode code) noexcept
{
    switch (code) {
    case ValidationCode::UnsupportedSchemaVersion: return "unsupported_schema_version";
    case ValidationCode::InvalidEnumValue: return "invalid_enum_value";
    case ValidationCode::MissingId: return "missing_id";
    case ValidationCode::DuplicateId: return "duplicate_id";
    case ValidationCode::EmptyTaskTitle: return "empty_task_title";
    case ValidationCode::EmptySessionTitleSnapshot: return "empty_session_title_snapshot";
    case ValidationCode::InvalidUtf8: return "invalid_utf8";
    case ValidationCode::ForbiddenTextControl: return "forbidden_text_control";
    case ValidationCode::ValueOutOfRange: return "value_out_of_range";
    case ValidationCode::InvalidTimestampOrder: return "invalid_timestamp_order";
    case ValidationCode::DuplicateActiveSortOrder: return "duplicate_active_sort_order";
    case ValidationCode::DanglingTaskReference: return "dangling_task_reference";
    case ValidationCode::InvalidSessionState: return "invalid_session_state";
    case ValidationCode::InvalidCompletionReason: return "invalid_completion_reason";
    case ValidationCode::InvalidIdempotencyKey: return "invalid_idempotency_key";
    case ValidationCode::MultipleActiveSessions: return "multiple_active_sessions";
    case ValidationCode::ActiveSessionIdMismatch: return "active_session_id_mismatch";
    case ValidationCode::MissingTimerSnapshot: return "missing_timer_snapshot";
    case ValidationCode::UnexpectedTimerSnapshot: return "unexpected_timer_snapshot";
    case ValidationCode::TimerSnapshotMismatch: return "timer_snapshot_mismatch";
    case ValidationCode::TaskCompletionCacheMismatch: return "task_completion_cache_mismatch";
    }
    return "unknown_validation_error";
}

ValidationReport validateFocusData(const FocusData& data)
{
    ValidationReport report;
    if (data.schemaVersion != kCurrentSchemaVersion) {
        issue(report, ValidationCode::UnsupportedSchemaVersion, "focusData", "root", "schemaVersion",
              "Focus data schema version is unsupported.");
    }
    validateSettings(data.settings, report);

    std::unordered_set<std::string> taskIds;
    std::unordered_set<std::int64_t> activeSortOrders;
    for (const auto& task : data.tasks) {
        validateTask(task, report);
        if (!task.id.empty() && !taskIds.insert(task.id).second) {
            issue(report, ValidationCode::DuplicateId, "task", task.id, "id",
                  "Task IDs must be unique.");
        }
        if (task.status == TaskStatus::Active && !activeSortOrders.insert(task.sortOrder).second) {
            issue(report, ValidationCode::DuplicateActiveSortOrder, "task", task.id, "sortOrder",
                  "Active tasks must have unique sort orders.");
        }
    }

    std::unordered_set<std::string> sessionIds;
    std::unordered_map<std::string, int> completedFocusCounts;
    std::vector<const FocusSessionRecord*> activeSessions;
    for (const auto& session : data.sessions) {
        validateSessionFields(session, report);
        if (!session.id.empty() && !sessionIds.insert(session.id).second) {
            issue(report, ValidationCode::DuplicateId, "session", session.id, "id",
                  "Session IDs must be unique.");
        }
        if (session.taskId && taskIds.find(*session.taskId) == taskIds.end()) {
            issue(report, ValidationCode::DanglingTaskReference, "session", session.id, "taskId",
                  "Session taskId does not reference a stored task.");
        }
        if (isActiveSessionStatus(session.status)) activeSessions.push_back(&session);
        if (session.status == SessionStatus::Completed
            && session.type == SessionType::Focus && session.taskId) {
            ++completedFocusCounts[*session.taskId];
        }
    }

    if (activeSessions.size() > 1) {
        issue(report, ValidationCode::MultipleActiveSessions, "focusData", "root", "activeSessionId",
              "At most one session may be running, paused, or awaiting completion.");
    }
    const FocusSessionRecord* activeSession =
        activeSessions.size() == 1 ? activeSessions.front() : nullptr;
    if ((activeSession == nullptr && data.activeSessionId)
        || (activeSession != nullptr
            && (!data.activeSessionId || *data.activeSessionId != activeSession->id))) {
        issue(report, ValidationCode::ActiveSessionIdMismatch, "focusData", "root", "activeSessionId",
              "activeSessionId must identify the only active session.");
    }

    if (activeSession != nullptr && !data.timerSnapshot) {
        issue(report, ValidationCode::MissingTimerSnapshot, "focusData", "root", "timerSnapshot",
              "An active session requires a recovery snapshot.");
    } else if (activeSession == nullptr && data.timerSnapshot) {
        issue(report, ValidationCode::UnexpectedTimerSnapshot, "focusData", "root", "timerSnapshot",
              "A timer snapshot must not exist without one active session.");
    }
    if (data.timerSnapshot) {
        validateSnapshot(*data.timerSnapshot, activeSession, report);
    }

    for (const auto& task : data.tasks) {
        const int factCount = completedFocusCounts[task.id];
        if (task.completedPomodoros != factCount) {
            issue(report, ValidationCode::TaskCompletionCacheMismatch, "task", task.id,
                  "completedPomodoros",
                  "Cached task completion count differs from completed focus-session facts.");
        }
    }

    return report;
}

} // namespace whatsui::focus_tomato
