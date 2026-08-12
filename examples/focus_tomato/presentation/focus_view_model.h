#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "../application/focus_data_service.h"
#include "../domain/clock_drift.h"
#include "../domain/focus_statistics.h"
#include "wui/state.h"

namespace whatsui::focus_tomato::presentation {

enum class TaskFilter {
    All,
    Active,
    Completed,
    Deleted,
};

class FocusViewModel {
public:
    using NowProvider = std::function<std::int64_t()>;
    using IdProvider = std::function<std::string()>;
    using MonotonicProvider = std::function<std::int64_t()>;

    FocusViewModel(FocusDataService& service, NowProvider nowProvider,
                   IdProvider idProvider,
                   MonotonicProvider monotonicProvider = {});

    [[nodiscard]] const FocusData& data() const noexcept;
    [[nodiscard]] const TaskRecord* selectedTask() const noexcept;
    [[nodiscard]] const FocusSessionRecord* activeSession() const noexcept;
    [[nodiscard]] const std::optional<std::string>& selectedTaskId() const noexcept;
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] TaskFilter taskFilter() const noexcept;
    [[nodiscard]] bool isTaskVisible(const TaskRecord& task) const noexcept;
    [[nodiscard]] FocusStatistics todayStatistics() const;

    void selectTask(std::string taskId);
    void setTaskFilter(TaskFilter filter) noexcept;
    [[nodiscard]] DataCommandResult toggleTaskCompletion(
        const std::string& taskId);
    [[nodiscard]] DataCommandResult addTask(std::string title,
                                            int estimatedPomodoros = 1,
                                            TaskExecutionPreferences execution = {});
    [[nodiscard]] DataCommandResult updateTask(
        const std::string& taskId,
        std::string title,
        int estimatedPomodoros,
        TaskExecutionPreferences execution,
        std::int64_t expectedRevision);
    [[nodiscard]] DataCommandResult deleteTask(
        const std::string& taskId,
        std::int64_t expectedRevision);
    [[nodiscard]] DataCommandResult restoreTask(
        const std::string& taskId,
        std::int64_t expectedRevision);
    [[nodiscard]] DataCommandResult startSelectedFocus();
    [[nodiscard]] DataCommandResult startFreeFocus();
    [[nodiscard]] DataCommandResult continueLastFocus();
    [[nodiscard]] DataCommandResult startBreak();
    [[nodiscard]] SessionType recommendedBreakType() const noexcept;
    [[nodiscard]] DataCommandResult toggleActiveSession();
    [[nodiscard]] DataCommandResult resetActiveSession();
    [[nodiscard]] DataCommandResult abortActiveSession();
    [[nodiscard]] DataCommandResult skipActiveBreak();
    [[nodiscard]] bool updateClock();

    [[nodiscard]] wui::State<std::string>& remainingText() noexcept;
    [[nodiscard]] wui::State<std::string>& operationMessage() noexcept;
    [[nodiscard]] wui::State<bool>& hasOperationMessage() noexcept;

private:
    [[nodiscard]] std::int64_t nowUtcMs() const;
    void publishResult(const DataCommandResult& result);
    void refreshRemainingText();
    [[nodiscard]] static std::string formatRemaining(std::int64_t milliseconds);
    void restoreSelectedTask();
    void captureClockCheckpoint();
    [[nodiscard]] std::optional<std::int64_t> driftSafeNow(
        std::int64_t wallNowUtcMs);
    void publishClockRecoveryMessage(ClockDriftStatus status);

    FocusDataService* service_{nullptr};
    NowProvider nowProvider_;
    IdProvider idProvider_;
    MonotonicProvider monotonicProvider_;
    std::optional<ClockCheckpoint> clockCheckpoint_;
    std::optional<std::string> selectedTaskId_;
    TaskFilter taskFilter_{TaskFilter::All};
    wui::State<std::string> remainingText_{"25:00"};
    wui::State<std::string> operationMessage_;
    wui::State<bool> hasOperationMessage_{false};
};

} // namespace whatsui::focus_tomato::presentation
