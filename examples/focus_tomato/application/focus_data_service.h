#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "focus_repository.h"

namespace whatsui::focus_tomato {

enum class DataCommandStatus {
    Success,
    NoChange,
    InvalidArgument,
    NotFound,
    Conflict,
    ValidationRejected,
    PersistenceFailed,
};

struct DataCommandResult {
    DataCommandStatus status{DataCommandStatus::Success};
    ValidationReport validation;
    std::string message;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return status == DataCommandStatus::Success
            || status == DataCommandStatus::NoChange;
    }
};

struct StartSessionCommand {
    std::string sessionId;
    std::optional<std::string> taskId;
    SessionType type{SessionType::Focus};
    std::int64_t plannedDurationMs{25 * kMinuteMs};
    std::int64_t nowUtcMs{0};
    bool startPaused{false};
    std::optional<std::string> soundscapeId;
};

struct AddTaskCommand {
    std::string taskId;
    std::string title;
    int estimatedPomodoros{1};
    std::int64_t nowUtcMs{0};
    TaskExecutionPreferences execution;
};

struct UpdateTaskCommand {
    std::string taskId;
    std::string title;
    int estimatedPomodoros{1};
    std::int64_t expectedRevision{0};
    std::int64_t nowUtcMs{0};
    TaskExecutionPreferences execution;
};

class FocusDataService {
public:
    FocusDataService(FocusRepository& repository, FocusData initialData = {});

    [[nodiscard]] const FocusData& data() const noexcept;

    [[nodiscard]] DataCommandResult addTask(const AddTaskCommand& command);
    [[nodiscard]] DataCommandResult updateTask(
        const UpdateTaskCommand& command);
    [[nodiscard]] DataCommandResult archiveTask(const std::string& taskId,
                                                std::int64_t expectedRevision,
                                                std::int64_t nowUtcMs);
    [[nodiscard]] DataCommandResult restoreTask(
        const std::string& taskId,
        std::int64_t expectedRevision,
        std::int64_t nowUtcMs);
    [[nodiscard]] DataCommandResult setTaskCompletion(
        const std::string& taskId,
        bool completed,
        std::int64_t expectedRevision,
        std::int64_t nowUtcMs);
    [[nodiscard]] DataCommandResult updateSettings(FocusSettings settings);
    [[nodiscard]] DataCommandResult startSession(const StartSessionCommand& command);
    [[nodiscard]] DataCommandResult pauseSession(const std::string& sessionId,
                                                 std::int64_t nowUtcMs);
    [[nodiscard]] DataCommandResult resumeSession(const std::string& sessionId,
                                                  std::int64_t nowUtcMs);
    [[nodiscard]] DataCommandResult resetSession(const std::string& sessionId,
                                                 std::int64_t nowUtcMs);
    [[nodiscard]] DataCommandResult markDeadlineReached(const std::string& sessionId,
                                                        std::int64_t nowUtcMs);
    [[nodiscard]] DataCommandResult finalizeCompletion(const std::string& sessionId,
                                                       std::int64_t nowUtcMs);
    [[nodiscard]] DataCommandResult abortSession(const std::string& sessionId,
                                                 std::int64_t nowUtcMs);
    [[nodiscard]] DataCommandResult skipBreakSession(
        const std::string& sessionId,
        std::int64_t nowUtcMs);

private:
    [[nodiscard]] DataCommandResult commit(FocusData candidate);
    [[nodiscard]] FocusSessionRecord* findSession(FocusData& data,
                                                  const std::string& sessionId) const;
    [[nodiscard]] const FocusSessionRecord* findSession(const std::string& sessionId) const;

    FocusRepository& repository_;
    FocusData data_;
};

} // namespace whatsui::focus_tomato
