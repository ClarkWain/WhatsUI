#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "todo_model.h"

namespace whatsui::todo {

enum class TodoLoadStatus {
    Loaded,
    Missing,
    // A prior interrupted replacement left a complete backup. It is restored
    // before parsing so callers still receive a normal Todo list.
    RecoveredBackup,
    // The unreadable source is retained next to the store as *.corrupt[.N];
    // callers receive an empty list and can safely start a new one.
    RecoveredMalformed,
    IoError,
};

struct TodoLoadResult {
    TodoLoadStatus status{TodoLoadStatus::Missing};
    std::vector<TodoRecord> records;
    std::filesystem::path recoveryPath;
    std::string message;
};

// Versioned, local-only persistence. The caller injects the exact file path,
// which keeps this module portable and lets the Windows application choose
// LocalAppData without embedding platform policy in the model.
class TodoStorage {
public:
    explicit TodoStorage(std::filesystem::path filePath);

    [[nodiscard]] const std::filesystem::path& filePath() const noexcept;
    [[nodiscard]] TodoLoadResult load() const;

    // Writes a complete replacement through <file>.tmp and <file>.bak. The
    // old file is first moved to backup, then the fully closed temp file takes
    // its place. If that second rename cannot complete, the old file is put
    // back. A next launch restores a surviving backup automatically.
    [[nodiscard]] bool save(const std::vector<TodoRecord>& records, std::string* error = nullptr) const;

private:
    [[nodiscard]] std::filesystem::path temporaryPath() const;
    [[nodiscard]] std::filesystem::path backupPath() const;
    [[nodiscard]] std::filesystem::path nextCorruptPath() const;

    std::filesystem::path filePath_;
};

} // namespace whatsui::todo
