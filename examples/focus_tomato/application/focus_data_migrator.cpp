#include "focus_data_migrator.h"

#include <algorithm>
#include <utility>

namespace whatsui::focus_tomato {

FocusDataMigrator::FocusDataMigrator() = default;

void FocusDataMigrator::registerMigration(FocusDataMigration migration)
{
    if (migration.fromVersion >= migration.toVersion) {
        migration.apply = nullptr;
    }
    const auto duplicate = std::any_of(
        migrations_.begin(), migrations_.end(),
        [&migration](const FocusDataMigration& existing) {
            return existing.fromVersion == migration.fromVersion;
        });
    if (duplicate) return;
    migrations_.push_back(std::move(migration));
    std::sort(
        migrations_.begin(), migrations_.end(),
        [](const FocusDataMigration& a, const FocusDataMigration& b) {
            return a.fromVersion < b.fromVersion;
        });
}

MigrationResult FocusDataMigrator::migrateTo(
    FocusData input, int targetVersion) const
{
    if (input.schemaVersion == targetVersion) {
        return input;
    }
    if (input.schemaVersion > targetVersion) {
        return MigrationError{
            MigrationErrorKind::NewerThanCurrent,
            input.schemaVersion,
            targetVersion,
            {},
            "input schemaVersion is newer than the target",
        };
    }

    FocusData current = std::move(input);
    while (current.schemaVersion < targetVersion) {
        const auto step = std::find_if(
            migrations_.begin(), migrations_.end(),
            [&current](const FocusDataMigration& m) {
                return m.fromVersion == current.schemaVersion;
            });
        if (step == migrations_.end() || !step->apply) {
            return MigrationError{
                MigrationErrorKind::NoPathToTarget,
                current.schemaVersion,
                targetVersion,
                {},
                "no migration registered for the current version",
            };
        }
        const int expectedNext = step->toVersion;
        auto applied = step->apply(std::move(current));
        if (auto* err = std::get_if<MigrationError>(&applied)) {
            if (err->stepName.empty()) err->stepName = step->name;
            if (err->fromVersion == 0) err->fromVersion = step->fromVersion;
            if (err->toVersion == 0) err->toVersion = step->toVersion;
            return *err;
        }
        current = std::move(std::get<FocusData>(applied));
        if (current.schemaVersion != expectedNext) {
            return MigrationError{
                MigrationErrorKind::ContractViolation,
                step->fromVersion,
                expectedNext,
                step->name,
                "migration did not set schemaVersion to its declared "
                "toVersion",
            };
        }
    }
    return current;
}

void registerBuiltinMigrations(FocusDataMigrator& /*migrator*/)
{
    // kCurrentSchemaVersion == 1: no built-in migrations yet.
    // ADR-008 interruption entity will register the first v1->v2 patch here.
}

FocusDataMigrator makeDefaultMigrator()
{
    FocusDataMigrator migrator;
    registerBuiltinMigrations(migrator);
    return migrator;
}

} // namespace whatsui::focus_tomato
