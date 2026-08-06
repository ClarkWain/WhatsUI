#pragma once

#include <filesystem>
#include <string>
#include <system_error>

namespace whatsui::focus_tomato {

// ADR-010 §4 disk transaction helper. Wraps the .pre-migration-v<N>.bak
// convention so callers do not open-code the backup filename or the
// commit/rollback ordering. Never touches the target file directly; the
// caller is responsible for writing the migrated payload via
// file_focus_repository's atomic replace path.
class MigrationTransaction {
public:
    MigrationTransaction(std::filesystem::path targetFile, int fromVersion);

    // Copies the current bytes of the target file to the .bak sidecar. Safe
    // to call more than once: subsequent calls verify the existing .bak is
    // identical to the target and never overwrite it, so a crash between
    // prepareBackup and commit does not corrupt the safety net.
    [[nodiscard]] bool prepareBackup(std::error_code& ec) noexcept;

    // Marks the transaction as successfully committed. The .bak sidecar is
    // *not* deleted so the user (or a maintenance UI) can restore it later.
    void commit() noexcept { committed_ = true; }

    // Explicitly abandons the transaction. Equivalent to letting the object
    // destruct without commit(); provided so the intent is obvious at the
    // call site.
    void rollback() noexcept { committed_ = false; }

    [[nodiscard]] const std::filesystem::path& targetPath() const noexcept
    {
        return target_;
    }

    [[nodiscard]] const std::filesystem::path& backupPath() const noexcept
    {
        return backup_;
    }

    [[nodiscard]] int fromVersion() const noexcept { return fromVersion_; }
    [[nodiscard]] bool committed() const noexcept { return committed_; }

private:
    std::filesystem::path target_;
    std::filesystem::path backup_;
    int fromVersion_{0};
    bool committed_{false};
};

// Builds the ".pre-migration-v<N>.bak" sidecar path next to a target file.
[[nodiscard]] std::filesystem::path migrationBackupPath(
    const std::filesystem::path& target, int fromVersion);

} // namespace whatsui::focus_tomato
