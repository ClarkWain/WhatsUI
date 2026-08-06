#include "focus_data_migrator.h"
#include "focus_data.h"

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

FocusData makeSample(int schemaVersion)
{
    FocusData data;
    data.schemaVersion = schemaVersion;
    TaskRecord task;
    task.id = "task-A";
    task.title = "sample";
    task.status = TaskStatus::Active;
    task.estimatedPomodoros = 2;
    task.completedPomodoros = 0;
    task.sortOrder = 1024;
    task.revision = 1;
    task.createdAtUtcMs = 1'000;
    task.updatedAtUtcMs = 1'000;
    data.tasks.push_back(task);
    return data;
}

void testIdentityWhenAlreadyAtTarget()
{
    const auto sample = makeSample(kCurrentSchemaVersion);
    const FocusDataMigrator migrator = makeDefaultMigrator();
    const auto result = migrator.migrateTo(sample, kCurrentSchemaVersion);
    expect(std::holds_alternative<FocusData>(result),
           "identity migration must succeed");
    expect(std::get<FocusData>(result) == sample,
           "identity migration must not mutate the payload");
}

void testNewerVersionRejected()
{
    auto data = makeSample(kCurrentSchemaVersion);
    data.schemaVersion = kCurrentSchemaVersion + 1;
    const FocusDataMigrator migrator = makeDefaultMigrator();
    const auto result = migrator.migrateTo(data, kCurrentSchemaVersion);
    const auto* err = std::get_if<MigrationError>(&result);
    expect(err != nullptr, "newer-than-current input must return an error");
    expect(err->kind == MigrationErrorKind::NewerThanCurrent,
           "kind must be NewerThanCurrent");
}

void testMissingPathRejected()
{
    auto data = makeSample(kCurrentSchemaVersion);
    data.schemaVersion = 0;
    const FocusDataMigrator migrator; // deliberately empty
    const auto result = migrator.migrateTo(data, kCurrentSchemaVersion);
    const auto* err = std::get_if<MigrationError>(&result);
    expect(err != nullptr, "missing migration path must return an error");
    expect(err->kind == MigrationErrorKind::NoPathToTarget,
           "kind must be NoPathToTarget");
}

void testChainedMigrationRunsInOrder()
{
    FocusDataMigrator migrator;
    FocusDataMigration v0toV1{
        0, 1, "v0_to_v1",
        [](FocusData d) -> MigrationResult {
            d.schemaVersion = 1;
            for (auto& task : d.tasks) task.title += "-v1";
            return d;
        },
    };
    FocusDataMigration v1toV2{
        1, 2, "v1_to_v2",
        [](FocusData d) -> MigrationResult {
            d.schemaVersion = 2;
            for (auto& task : d.tasks) task.title += "-v2";
            return d;
        },
    };
    // Register out of order to verify sort() keeps the chain deterministic.
    migrator.registerMigration(v1toV2);
    migrator.registerMigration(v0toV1);

    auto data = makeSample(0);
    const auto result = migrator.migrateTo(data, 2);
    const auto* out = std::get_if<FocusData>(&result);
    expect(out != nullptr, "chained migration must succeed");
    expect(out->schemaVersion == 2, "final schemaVersion must be 2");
    expect(out->tasks.at(0).title == "sample-v1-v2",
           "chained migrations must run in ascending order");
}

void testDuplicateFromVersionIgnoresLaterRegistration()
{
    FocusDataMigrator migrator;
    migrator.registerMigration({
        0, 1, "original",
        [](FocusData d) -> MigrationResult {
            d.schemaVersion = 1;
            for (auto& task : d.tasks) task.title += "-first";
            return d;
        },
    });
    migrator.registerMigration({
        0, 1, "shadow",
        [](FocusData d) -> MigrationResult {
            d.schemaVersion = 1;
            for (auto& task : d.tasks) task.title += "-second";
            return d;
        },
    });

    auto data = makeSample(0);
    const auto result = migrator.migrateTo(data, 1);
    const auto* out = std::get_if<FocusData>(&result);
    expect(out != nullptr, "single registered migration must run");
    expect(out->tasks.at(0).title == "sample-first",
           "duplicate fromVersion must not shadow the first registration");
    expect(migrator.migrations().size() == 1,
           "duplicate registrations must be discarded");
}

void testApplyErrorPropagates()
{
    FocusDataMigrator migrator;
    migrator.registerMigration({
        0, 1, "always_fails",
        [](FocusData) -> MigrationResult {
            return MigrationError{
                MigrationErrorKind::ApplyFailed, 0, 0, {},
                "simulated failure",
            };
        },
    });
    auto data = makeSample(0);
    const auto result = migrator.migrateTo(data, 1);
    const auto* err = std::get_if<MigrationError>(&result);
    expect(err != nullptr, "failing step must propagate as MigrationError");
    expect(err->stepName == "always_fails",
           "migrator must annotate the failing stepName");
    expect(err->fromVersion == 0 && err->toVersion == 1,
           "migrator must annotate the failing version range");
}

void testContractViolationDetected()
{
    FocusDataMigrator migrator;
    migrator.registerMigration({
        0, 1, "forgets_to_bump",
        [](FocusData d) -> MigrationResult {
            // Missing d.schemaVersion = 1.
            return d;
        },
    });
    auto data = makeSample(0);
    const auto result = migrator.migrateTo(data, 1);
    const auto* err = std::get_if<MigrationError>(&result);
    expect(err != nullptr, "contract violation must produce an error");
    expect(err->kind == MigrationErrorKind::ContractViolation,
           "kind must be ContractViolation");
}

void testIdempotencyWhenRerun()
{
    FocusDataMigrator migrator;
    int applyCalls = 0;
    migrator.registerMigration({
        0, 1, "counter",
        [&applyCalls](FocusData d) -> MigrationResult {
            ++applyCalls;
            d.schemaVersion = 1;
            for (auto& task : d.tasks) task.title += "-x";
            return d;
        },
    });
    auto data = makeSample(0);
    const auto first = migrator.migrateTo(data, 1);
    expect(std::holds_alternative<FocusData>(first), "first migration must succeed");

    const auto secondRun = migrator.migrateTo(
        std::get<FocusData>(first), 1);
    const auto* out = std::get_if<FocusData>(&secondRun);
    expect(out != nullptr, "second run at the same version must succeed");
    expect(applyCalls == 1,
           "identity short-circuit must not re-apply the migration");
    expect(out->tasks.at(0).title == "sample-x",
           "second run at the same version must not double-apply");
}

void testBuiltinsAtCurrentVersionAreIdempotent()
{
    const auto migrator = makeDefaultMigrator();
    const auto data = makeSample(kCurrentSchemaVersion);
    const auto result = migrator.migrateToCurrent(data);
    expect(std::holds_alternative<FocusData>(result),
           "default migrator must accept current-version input");
    expect(std::get<FocusData>(result) == data,
           "default migrator must not mutate current-version input");
}

void testV1ToV2MigrationBumpsSchemaAndPreservesData()
{
    FocusData data = makeSample(1);
    // Populate a session so we can verify interruptions default to empty
    // instead of being lost or fabricated by the migration.
    FocusSessionRecord session;
    session.id = "session-1";
    session.taskId = "task-A";
    session.titleSnapshot = "sample";
    session.type = SessionType::Focus;
    session.plannedDurationMs = 25 * kMinuteMs;
    session.startedAtUtcMs = 1'000;
    session.targetEndAtUtcMs = 1'000 + 25 * kMinuteMs;
    session.remainingMs = 25 * kMinuteMs;
    session.status = SessionStatus::Running;
    session.completionReason = CompletionReason::None;
    session.idempotencyKey = "session-1";
    data.sessions.push_back(session);
    data.activeSessionId = "session-1";
    // Simulate a v1 timer snapshot: schemaVersion is 1 in the file.
    TimerSnapshot snapshot;
    snapshot.schemaVersion = 1;
    snapshot.sessionId = "session-1";
    snapshot.status = SessionStatus::Paused;
    snapshot.savedAtUtcMs = 2'000;
    snapshot.remainingMs = 20 * kMinuteMs;
    data.timerSnapshot = snapshot;

    const auto migrator = makeDefaultMigrator();
    const auto result = migrator.migrateToCurrent(std::move(data));
    const auto* out = std::get_if<FocusData>(&result);
    expect(out != nullptr, "v1 -> v2 migration must succeed");
    expect(out->schemaVersion == 2,
           "schema version must reach the current value");
    expect(out->sessions.size() == 1
            && out->sessions.at(0).interruptions.empty(),
           "existing sessions must default to empty interruptions");
    expect(out->timerSnapshot.has_value()
            && out->timerSnapshot->schemaVersion == 2,
           "timer snapshot schemaVersion must be bumped with the aggregate");
    expect(out->tasks.at(0).title == "sample",
           "task titles must not be touched by v1 -> v2");
}

void testV2InputIsIdempotentOnDefault()
{
    // Build a v2 FocusData with an interruption already recorded.
    FocusData data = makeSample(kCurrentSchemaVersion);
    FocusSessionRecord session;
    session.id = "session-1";
    session.titleSnapshot = "existing";
    session.plannedDurationMs = 25 * kMinuteMs;
    session.startedAtUtcMs = 1'000;
    session.remainingMs = 25 * kMinuteMs;
    session.status = SessionStatus::Paused;
    session.idempotencyKey = "session-1";
    session.interruptions.push_back({
        InterruptionReason::Meeting,
        "team standup",
        1'000, 1'050,
        InterruptionSource::User,
    });
    data.sessions.push_back(session);

    const auto migrator = makeDefaultMigrator();
    const auto original = data;
    const auto result = migrator.migrateToCurrent(std::move(data));
    const auto* out = std::get_if<FocusData>(&result);
    expect(out != nullptr, "v2 input must remain accepted");
    expect(*out == original,
           "v2 input must be returned untouched (identity short-circuit)");
    expect(out->sessions.at(0).interruptions.size() == 1,
           "existing interruption events must not be dropped");
}

} // namespace

int main()
{
    try {
        testIdentityWhenAlreadyAtTarget();
        testNewerVersionRejected();
        testMissingPathRejected();
        testChainedMigrationRunsInOrder();
        testDuplicateFromVersionIgnoresLaterRegistration();
        testApplyErrorPropagates();
        testContractViolationDetected();
        testIdempotencyWhenRerun();
        testBuiltinsAtCurrentVersionAreIdempotent();
        testV1ToV2MigrationBumpsSchemaAndPreservesData();
        testV2InputIsIdempotentOnDefault();
    } catch (const std::exception& e) {
        std::cerr << "focus_tomato_migrator_tests: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
