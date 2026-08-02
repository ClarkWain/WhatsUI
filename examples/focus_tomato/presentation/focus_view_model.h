#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "../application/focus_data_service.h"
#include "wui/state.h"

namespace whatsui::focus_tomato::presentation {

class FocusViewModel {
public:
    using NowProvider = std::function<std::int64_t()>;
    using IdProvider = std::function<std::string()>;

    FocusViewModel(FocusDataService& service, NowProvider nowProvider,
                   IdProvider idProvider);

    [[nodiscard]] const FocusData& data() const noexcept;
    [[nodiscard]] const TaskRecord* selectedTask() const noexcept;
    [[nodiscard]] const FocusSessionRecord* activeSession() const noexcept;
    [[nodiscard]] const std::optional<std::string>& selectedTaskId() const noexcept;
    [[nodiscard]] bool isRunning() const noexcept;

    void selectTask(std::string taskId);
    [[nodiscard]] DataCommandResult addTask(std::string title,
                                            int estimatedPomodoros = 1);
    [[nodiscard]] DataCommandResult startSelectedFocus();
    [[nodiscard]] DataCommandResult startShortBreak();
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

    FocusDataService* service_{nullptr};
    NowProvider nowProvider_;
    IdProvider idProvider_;
    std::optional<std::string> selectedTaskId_;
    wui::State<std::string> remainingText_{"25:00"};
    wui::State<std::string> operationMessage_;
    wui::State<bool> hasOperationMessage_{false};
};

} // namespace whatsui::focus_tomato::presentation
