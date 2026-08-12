#pragma once

#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "../domain/focus_data.h"

namespace whatsui::focus_tomato {

// ADR-010: single-step chained migration for the FocusData file.
// TimerSnapshot and standalone FocusSettings files remain out of scope
// until they need a version bump on their own tracks.

enum class MigrationErrorKind {
    NoPathToTarget,
    NewerThanCurrent,
    ApplyFailed,
    ContractViolation,
};

struct MigrationError {
    MigrationErrorKind kind{MigrationErrorKind::ApplyFailed};
    int fromVersion{0};
    int toVersion{0};
    std::string stepName;
    std::string message;
};

using MigrationResult = std::variant<FocusData, MigrationError>;

struct FocusDataMigration {
    int fromVersion{0};
    int toVersion{0};
    std::string name;
    std::function<MigrationResult(FocusData)> apply;
};

class FocusDataMigrator {
public:
    FocusDataMigrator();

    void registerMigration(FocusDataMigration migration);

    [[nodiscard]] MigrationResult migrateTo(
        FocusData input, int targetVersion) const;

    [[nodiscard]] MigrationResult migrateToCurrent(FocusData input) const
    {
        return migrateTo(std::move(input), kCurrentSchemaVersion);
    }

    [[nodiscard]] const std::vector<FocusDataMigration>&
    migrations() const noexcept { return migrations_; }

private:
    std::vector<FocusDataMigration> migrations_;
};

// Registers every built-in migration in order. Currently empty because
// kCurrentSchemaVersion is 1; ADR-008 (interruption entity) will register the
// first real v1->v2 patch. Split out to keep test fixtures independent from
// production wiring.
void registerBuiltinMigrations(FocusDataMigrator& migrator);

// Convenience factory used by production code paths.
[[nodiscard]] FocusDataMigrator makeDefaultMigrator();

} // namespace whatsui::focus_tomato
