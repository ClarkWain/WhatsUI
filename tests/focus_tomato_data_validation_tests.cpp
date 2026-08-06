#include "focus_data_validator.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace whatsui::focus_tomato;

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

FocusData validPausedData()
{
    FocusData data;
    data.tasks.push_back({
        "task-1", "完成产品设计稿", TaskStatus::Active, 3, 0, 1024, 1, 1'000, 1'000,
    });
    data.sessions.push_back({
        "session-1",
        std::string{"task-1"},
        "完成产品设计稿",
        SessionType::Focus,
        25 * kMinuteMs,
        2'000,
        std::nullopt,
        12 * kMinuteMs,
        SessionStatus::Paused,
        std::nullopt,
        CompletionReason::None,
        "session-1",
    });
    data.activeSessionId = "session-1";
    data.timerSnapshot = TimerSnapshot{
        kCurrentSchemaVersion,
        "session-1",
        SessionStatus::Paused,
        3'000,
        std::nullopt,
        12 * kMinuteMs,
    };
    return data;
}

void validAggregatePasses()
{
    const auto report = validateFocusData(validPausedData());
    expect(report.ok(), "A consistent paused-session aggregate must pass validation: " + report.summary());
    expect(report.issues().empty(), "Valid data should not produce warnings that hide real failures");
}

void fieldErrorsHaveStableDiagnosticCodes()
{
    {
        auto data = validPausedData();
        data.schemaVersion = 99;
        expect(validateFocusData(data).has(ValidationCode::UnsupportedSchemaVersion),
               "Unsupported schema versions must be explicit");
    }
    {
        auto data = validPausedData();
        data.tasks.front().title = " \t\r\n ";
        expect(validateFocusData(data).has(ValidationCode::EmptyTaskTitle),
               "Whitespace-only task titles must not reach the application layer");
    }
    {
        auto data = validPausedData();
        data.tasks.front().title = std::string{"broken"} + static_cast<char>(0xC3);
        expect(validateFocusData(data).has(ValidationCode::InvalidUtf8),
               "Malformed UTF-8 must be diagnosed before persistence");
    }
    {
        auto data = validPausedData();
        data.sessions.front().titleSnapshot = " \r\n ";
        expect(validateFocusData(data).has(ValidationCode::EmptySessionTitleSnapshot),
               "Historical title snapshots must not become invisible or unusable");
    }
    {
        auto data = validPausedData();
        data.tasks.front().title = "safe\xE2\x80\xAEhidden";
        expect(validateFocusData(data).has(ValidationCode::ForbiddenTextControl),
               "Bidi override controls must be rejected with a dedicated code");
    }
    {
        auto data = validPausedData();
        data.settings.focusMinutes = 0;
        data.settings.soundVolumePercent = 101;
        const auto report = validateFocusData(data);
        expect(report.has(ValidationCode::ValueOutOfRange) && report.errorCount() >= 2,
               "Every invalid setting must be reported, not just the first");
    }
    {
        auto data = validPausedData();
        data.tasks.front().execution.focusMinutes = 181;
        expect(validateFocusData(data).has(ValidationCode::ValueOutOfRange),
               "Task-specific focus duration must use the shared duration range");
    }
    {
        auto data = validPausedData();
        data.tasks.front().execution.sound =
            TaskSoundPreference::Soundscape;
        expect(validateFocusData(data).has(ValidationCode::ValueOutOfRange),
               "Selecting sound without a stable catalog ID must be rejected");
    }
    {
        auto data = validPausedData();
        data.tasks.front().execution.sound = TaskSoundPreference::Off;
        data.tasks.front().execution.soundscapeId = "rain";
        expect(validateFocusData(data).has(ValidationCode::InvalidEnumValue),
               "Disabled sound must not retain a hidden soundscape ID");
    }
    {
        auto data = validPausedData();
        data.tasks.front().updatedAtUtcMs = data.tasks.front().createdAtUtcMs - 1;
        expect(validateFocusData(data).has(ValidationCode::InvalidTimestampOrder),
               "Impossible timestamp order must be visible to callers");
    }
}

void duplicateAndCrossRecordErrorsAreDetected()
{
    {
        auto data = validPausedData();
        auto duplicate = data.tasks.front();
        duplicate.sortOrder += 100;
        data.tasks.push_back(duplicate);
        expect(validateFocusData(data).has(ValidationCode::DuplicateId),
               "Duplicate task IDs must be rejected");
    }
    {
        auto data = validPausedData();
        auto second = data.tasks.front();
        second.id = "task-2";
        data.tasks.push_back(second);
        expect(validateFocusData(data).has(ValidationCode::DuplicateActiveSortOrder),
               "Active task ordering must not contain collisions");
    }
    {
        auto data = validPausedData();
        data.sessions.front().taskId = "missing-task";
        expect(validateFocusData(data).has(ValidationCode::DanglingTaskReference),
               "A session may not silently point at a missing task");
    }
    {
        auto data = validPausedData();
        auto second = data.sessions.front();
        second.id = "session-2";
        second.idempotencyKey = "session-2";
        data.sessions.push_back(second);
        expect(validateFocusData(data).has(ValidationCode::MultipleActiveSessions),
               "The database aggregate must enforce one active session");
    }
}

void sessionStateCombinationsAreValidated()
{
    {
        auto data = validPausedData();
        auto& session = data.sessions.front();
        session.status = SessionStatus::Running;
        session.targetEndAtUtcMs.reset();
        expect(validateFocusData(data).has(ValidationCode::InvalidSessionState),
               "Running sessions require a target end timestamp");
    }
    {
        auto data = validPausedData();
        auto& session = data.sessions.front();
        session.targetEndAtUtcMs = 99'999;
        expect(validateFocusData(data).has(ValidationCode::InvalidSessionState),
               "Paused sessions must persist remaining time instead of a target end");
    }
    {
        auto data = validPausedData();
        auto& session = data.sessions.front();
        session.status = SessionStatus::Skipped;
        session.endedAtUtcMs = 4'000;
        session.remainingMs = 0;
        session.completionReason = CompletionReason::UserSkipped;
        data.activeSessionId.reset();
        data.timerSnapshot.reset();
        expect(validateFocusData(data).has(ValidationCode::InvalidSessionState),
               "Focus sessions cannot use the break-only skipped terminal");
    }
    {
        auto data = validPausedData();
        data.sessions.front().idempotencyKey = "another-key";
        expect(validateFocusData(data).has(ValidationCode::InvalidIdempotencyKey),
               "Completion idempotency must be tied to session ID");
    }
}

void activeSessionAndSnapshotMustDescribeTheSameFact()
{
    {
        auto data = validPausedData();
        data.activeSessionId = "session-does-not-exist";
        expect(validateFocusData(data).has(ValidationCode::ActiveSessionIdMismatch),
               "activeSessionId must identify the actual active record");
    }
    {
        auto data = validPausedData();
        data.timerSnapshot.reset();
        expect(validateFocusData(data).has(ValidationCode::MissingTimerSnapshot),
               "An active session without a recovery snapshot must fail validation");
    }
    {
        auto data = validPausedData();
        data.timerSnapshot->remainingMs -= 1;
        expect(validateFocusData(data).has(ValidationCode::TimerSnapshotMismatch),
               "Snapshot and session state must not drift silently");
    }
    {
        auto data = validPausedData();
        auto& session = data.sessions.front();
        session.status = SessionStatus::Completed;
        session.remainingMs = 0;
        session.endedAtUtcMs = 4'000;
        session.completionReason = CompletionReason::Natural;
        data.activeSessionId.reset();
        expect(validateFocusData(data).has(ValidationCode::UnexpectedTimerSnapshot),
               "Terminal data must not retain an active timer snapshot");
    }
}

void denormalizedTaskCacheIsCheckedAgainstSessionFacts()
{
    auto data = validPausedData();
    auto completed = data.sessions.front();
    completed.id = "session-completed";
    completed.idempotencyKey = completed.id;
    completed.status = SessionStatus::Completed;
    completed.targetEndAtUtcMs.reset();
    completed.remainingMs = 0;
    completed.endedAtUtcMs = 8'000;
    completed.completionReason = CompletionReason::Natural;
    data.sessions.push_back(completed);

    const auto report = validateFocusData(data);
    expect(report.has(ValidationCode::TaskCompletionCacheMismatch),
           "Task counters must be checked against completed focus sessions, the source of truth");
}

} // namespace

int main()
{
    try {
        validAggregatePasses();
        fieldErrorsHaveStableDiagnosticCodes();
        duplicateAndCrossRecordErrorsAreDetected();
        sessionStateCombinationsAreValidated();
        activeSessionAndSnapshotMustDescribeTheSameFact();
        denormalizedTaskCacheIsCheckedAgainstSessionFacts();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
