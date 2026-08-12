#pragma once

#include <filesystem>
#include <string>

#include "../application/focus_repository.h"

namespace whatsui::focus_tomato {

enum class RepositoryLoadStatus {
    Loaded,
    Missing,
    RecoveredBackup,
    RejectedCorrupt,
    MigrationFailed,
    IoError,
};

struct RepositoryLoadResult {
    RepositoryLoadStatus status{RepositoryLoadStatus::Missing};
    FocusData data;
    ValidationReport validation;
    std::filesystem::path recoveryPath;
    std::string message;

    [[nodiscard]] bool hasUsableData() const noexcept
    {
        return status == RepositoryLoadStatus::Loaded
            || status == RepositoryLoadStatus::RecoveredBackup
            || status == RepositoryLoadStatus::Missing;
    }
};

// Portable local persistence for the first WhatsUI implementation slice.
// The aggregate format is versioned and replace-only. A future SQLite backend
// can implement FocusRepository without changing domain/application code.
class FileFocusRepository final : public FocusRepository {
public:
    explicit FileFocusRepository(std::filesystem::path filePath);

    [[nodiscard]] const std::filesystem::path& filePath() const noexcept;
    [[nodiscard]] RepositoryLoadResult load() const;
    [[nodiscard]] RepositoryWriteResult save(const FocusData& data) override;

private:
    [[nodiscard]] std::filesystem::path temporaryPath() const;
    [[nodiscard]] std::filesystem::path backupPath() const;
    [[nodiscard]] std::filesystem::path nextCorruptPath() const;

    std::filesystem::path filePath_;
};

} // namespace whatsui::focus_tomato
