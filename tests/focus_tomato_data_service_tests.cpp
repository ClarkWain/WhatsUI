#include "focus_data_service.h"

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

class RecordingRepository final : public FocusRepository {
public:
    RepositoryWriteResult save(const FocusData& data) override
    {
        ++saveCalls;
        const auto report = validateFocusData(data);
        if (!report.ok()) {
            return {RepositoryWriteStatus::ValidationRejected, report,
                    "repository rejected invalid aggregate"};
        }
        if (failNextSave) {
            failNextSave = false;
            return {RepositoryWriteStatus::IoError, {}, "simulated disk failure"};
        }
        persisted = data;
        return {};
    }

    int saveCalls{0};
    bool failNextSave{false};
    FocusData persisted;
};

FocusData dataWithTask()
{
    FocusData data;
    data.tasks.push_back({
        "task-1", "完成产品设计稿", TaskStatus::Active, 3, 0, 1024, 1, 1'000, 1'000,
    });
    return data;
}

StartSessionCommand focusCommand(std::int64_t nowUtcMs = 10'000)
{
    return {
        "session-1",
        std::string{"task-1"},
        SessionType::Focus,
        25 * kMinuteMs,
        nowUtcMs,
    };
}

void startIsOneValidatedCommit()
{
    RecordingRepository repository;
    FocusDataService service(repository, dataWithTask());
    const auto result = service.startSession(focusCommand());

    expect(result.status == DataCommandStatus::Success, "A valid start must succeed");
    expect(repository.saveCalls == 1, "Start must persist exactly one complete aggregate");
    expect(repository.persisted == service.data(), "Published state must equal persisted state");
    expect(service.data().activeSessionId == std::optional<std::string>{"session-1"},
           "Start must publish the only active session ID");
    expect(service.data().timerSnapshot
               && service.data().timerSnapshot->sessionId == "session-1",
           "Start must atomically create its recovery snapshot");
}

void taskAndSettingsMutationsUseTheSameValidationGateway()
{
    RecordingRepository repository;
    FocusDataService service(repository);

    const auto invalidTask = service.addTask(
        {"task-invalid", " \t\n ", 2, 1'000});
    expect(invalidTask.status == DataCommandStatus::ValidationRejected
               && invalidTask.validation.has(ValidationCode::EmptyTaskTitle),
           "Task validation errors must be returned by the shared commit gateway");
    expect(repository.saveCalls == 0,
           "A domain-invalid task must fail before repository I/O");

    expect(service.addTask({"task-1", "设计番茄钟", 3, 1'000}).status
               == DataCommandStatus::Success,
           "A valid task should be committed");
    expect(service.data().tasks.front().sortOrder == 1024
               && service.data().tasks.front().revision == 1,
           "Task creation must initialize sparse ordering and optimistic revision");
    expect(service.archiveTask("task-1", 99, 2'000).status
               == DataCommandStatus::Conflict,
           "Stale task editors must fail optimistic concurrency");
    expect(service.archiveTask("task-1", 1, 2'000).status
               == DataCommandStatus::Success,
           "Matching revisions should archive without deleting history");

    FocusSettings invalidSettings = service.data().settings;
    invalidSettings.longBreakEvery = 0;
    const auto invalidUpdate = service.updateSettings(invalidSettings);
    expect(invalidUpdate.status == DataCommandStatus::ValidationRejected
               && invalidUpdate.validation.has(ValidationCode::ValueOutOfRange),
           "Settings updates must pass storage-equivalent validation");
}

void invalidOrConflictingStartDoesNotWrite()
{
    RecordingRepository repository;
    FocusDataService service(repository, dataWithTask());

    auto invalid = focusCommand();
    invalid.plannedDurationMs = 0;
    expect(service.startSession(invalid).status == DataCommandStatus::InvalidArgument,
           "An invalid duration must fail before storage");
    expect(repository.saveCalls == 0, "Invalid commands must not touch persistence");

    expect(service.startSession(focusCommand()).status == DataCommandStatus::Success,
           "Setup start should succeed");
    auto second = focusCommand();
    second.sessionId = "session-2";
    expect(service.startSession(second).status == DataCommandStatus::Conflict,
           "A second active session must be a visible conflict");
    expect(repository.saveCalls == 1, "A conflict must not write a second session");
}

void pauseAndResumeUseDeadlineMath()
{
    RecordingRepository repository;
    FocusDataService service(repository, dataWithTask());
    expect(service.startSession(focusCommand()).succeeded(), "Setup start should succeed");

    const auto pause = service.pauseSession("session-1", 310'000);
    expect(pause.status == DataCommandStatus::Success, "Running session should pause");
    const auto& paused = service.data().sessions.front();
    expect(paused.status == SessionStatus::Paused
               && paused.remainingMs == 20 * kMinuteMs
               && !paused.targetEndAtUtcMs,
           "Pause must store target-now as remaining and clear the deadline");

    const auto resume = service.resumeSession("session-1", 500'000);
    expect(resume.status == DataCommandStatus::Success, "Paused session should resume");
    const auto& running = service.data().sessions.front();
    expect(running.status == SessionStatus::Running
               && running.targetEndAtUtcMs == std::optional<std::int64_t>{1'700'000},
           "Resume must create a new deadline from persisted remaining time");
    expect(service.data().timerSnapshot->targetEndAtUtcMs == running.targetEndAtUtcMs,
           "Snapshot and session deadline must be committed together");

    expect(service.resetSession("session-1", 600'000).status
               == DataCommandStatus::Success,
           "The running timer should support a data-safe reset");
    expect(service.data().sessions.front().targetEndAtUtcMs
               == std::optional<std::int64_t>{2'100'000}
               && service.data().sessions.front().remainingMs == 25 * kMinuteMs,
           "Reset must keep the same session and restore its planned duration");

    expect(service.pauseSession("session-1", 900'000).succeeded(),
           "Setup pause after reset should succeed");
    expect(service.resetSession("session-1", 910'000).succeeded()
               && service.data().sessions.front().status == SessionStatus::Paused
               && !service.data().sessions.front().targetEndAtUtcMs
               && service.data().sessions.front().remainingMs == 25 * kMinuteMs,
           "A paused reset must remain paused and clear the deadline");
}

void completionIsRecoverableAtomicAndIdempotent()
{
    RecordingRepository repository;
    FocusDataService service(repository, dataWithTask());
    expect(service.startSession(focusCommand()).succeeded(), "Setup start should succeed");
    const std::int64_t deadline = 10'000 + 25 * kMinuteMs;
    expect(service.markDeadlineReached("session-1", deadline).status == DataCommandStatus::Success,
           "Deadline must first persist a completion-pending checkpoint");
    expect(service.data().sessions.front().status == SessionStatus::CompletionPending
               && service.data().timerSnapshot,
           "Completion-pending must remain recoverable");

    repository.failNextSave = true;
    const auto failed = service.finalizeCompletion("session-1", deadline + 5);
    expect(failed.status == DataCommandStatus::PersistenceFailed,
           "A storage failure must be distinguishable from validation");
    expect(service.data().sessions.front().status == SessionStatus::CompletionPending
               && service.data().tasks.front().completedPomodoros == 0,
           "Failed completion must not publish a terminal session or task cache increment");

    expect(service.finalizeCompletion("session-1", deadline + 10).status
               == DataCommandStatus::Success,
           "Retry should atomically finalize the same session");
    expect(service.data().sessions.front().status == SessionStatus::Completed
               && service.data().tasks.front().completedPomodoros == 1
               && !service.data().activeSessionId
               && !service.data().timerSnapshot,
           "Successful completion must update facts, cache, active ID and snapshot together");

    const int writesAfterCompletion = repository.saveCalls;
    expect(service.finalizeCompletion("session-1", deadline + 20).status
               == DataCommandStatus::NoChange,
           "Repeated completion delivery must be idempotent");
    expect(repository.saveCalls == writesAfterCompletion
               && service.data().tasks.front().completedPomodoros == 1,
           "Idempotent completion must not write or increment twice");
}

void abortNeverIncrementsFocusFacts()
{
    RecordingRepository repository;
    FocusDataService service(repository, dataWithTask());
    expect(service.startSession(focusCommand()).succeeded(), "Setup start should succeed");
    expect(service.abortSession("session-1", 50'000).status == DataCommandStatus::Success,
           "Active sessions should support explicit abort");
    expect(service.data().sessions.front().status == SessionStatus::Aborted
               && service.data().sessions.front().completionReason == CompletionReason::UserAborted
               && service.data().tasks.front().completedPomodoros == 0
               && !service.data().timerSnapshot,
           "Abort must clear recovery state without creating focus statistics");
}

void breakSkipUsesItsOwnTerminalFact()
{
    RecordingRepository repository;
    FocusDataService service(repository, dataWithTask());
    StartSessionCommand command;
    command.sessionId = "break-1";
    command.type = SessionType::ShortBreak;
    command.plannedDurationMs = 5 * kMinuteMs;
    command.nowUtcMs = 10'000;
    expect(service.startSession(command).succeeded(),
           "A task-free short break should start");

    expect(service.skipBreakSession("break-1", 20'000).status
               == DataCommandStatus::Success,
           "An active break should support the explicit skip transition");
    const auto& skipped = service.data().sessions.front();
    expect(skipped.status == SessionStatus::Skipped
               && skipped.completionReason == CompletionReason::UserSkipped
               && skipped.endedAtUtcMs == std::optional<std::int64_t>{20'000}
               && !service.data().activeSessionId
               && !service.data().timerSnapshot,
           "Skip must retain a truthful terminal break fact and clear recovery state");

    expect(service.skipBreakSession("break-1", 30'000).status
               == DataCommandStatus::NoChange,
           "Repeated break skips should be idempotent");

    FocusDataService focusService(repository, dataWithTask());
    expect(focusService.startSession(focusCommand()).succeeded(),
           "Focus setup for skip rejection should succeed");
    expect(focusService.skipBreakSession("session-1", 20'000).status
               == DataCommandStatus::Conflict,
           "A focus session must never be mislabeled as a skipped break");
}

} // namespace

int main()
{
    try {
        startIsOneValidatedCommit();
        taskAndSettingsMutationsUseTheSameValidationGateway();
        invalidOrConflictingStartDoesNotWrite();
        pauseAndResumeUseDeadlineMath();
        completionIsRecoverableAtomicAndIdempotent();
        abortNeverIncrementsFocusFacts();
        breakSkipUsesItsOwnTerminalFact();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
