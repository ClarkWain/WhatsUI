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
    } catch (const std::exception& e) {
        std::cerr << "focus_tomato_migrator_tests: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
