#include "focus_view_model.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace whatsui::focus_tomato::presentation {

FocusViewModel::FocusViewModel(FocusDataService& service, NowProvider nowProvider,
                               IdProvider idProvider,
                               MonotonicProvider monotonicProvider)
    : service_(&service)
    , nowProvider_(std::move(nowProvider))
    , idProvider_(std::move(idProvider))
    , monotonicProvider_(std::move(monotonicProvider))
{
    restoreSelectedTask();
    captureClockCheckpoint();
    refreshRemainingText();
}

const FocusData& FocusViewModel::data() const noexcept
{
    return service_->data();
}

const TaskRecord* FocusViewModel::selectedTask() const noexcept
{
    if (!selectedTaskId_) return nullptr;
    const auto& tasks = data().tasks;
    const auto task = std::find_if(tasks.begin(), tasks.end(), [this](const TaskRecord& item) {
        return item.id == *selectedTaskId_;
    });
    return task == tasks.end() ? nullptr : &*task;
}

const FocusSessionRecord* FocusViewModel::activeSession() const noexcept
{
    if (!data().activeSessionId) return nullptr;
    const auto& sessions = data().sessions;
    const auto session = std::find_if(
        sessions.begin(), sessions.end(), [this](const FocusSessionRecord& item) {
            return item.id == *data().activeSessionId;
        });
    return session == sessions.end() ? nullptr : &*session;
}

const std::optional<std::string>& FocusViewModel::selectedTaskId() const noexcept
{
    return selectedTaskId_;
}

bool FocusViewModel::isRunning() const noexcept
{
    const auto* session = activeSession();
    return session != nullptr && session->status == SessionStatus::Running;
}

TaskFilter FocusViewModel::taskFilter() const noexcept
{
    return taskFilter_;
}

bool FocusViewModel::isTaskVisible(const TaskRecord& task) const noexcept
{
    switch (taskFilter_) {
    case TaskFilter::All: return !isArchivedTaskStatus(task.status);
    case TaskFilter::Active: return task.status == TaskStatus::Active;
    case TaskFilter::Completed: return task.status == TaskStatus::Done;
    case TaskFilter::Deleted: return isArchivedTaskStatus(task.status);
    }
    return false;
}

FocusStatistics FocusViewModel::todayStatistics() const
{
    const auto range = localDayUtcRange(nowUtcMs());
    return calculateFocusStatistics(
        data(), range.fromUtcMs, range.toUtcMs);
}

void FocusViewModel::selectTask(std::string taskId)
{
    selectedTaskId_ = std::move(taskId);
}

void FocusViewModel::setTaskFilter(TaskFilter filter) noexcept
{
    taskFilter_ = filter;
}

DataCommandResult FocusViewModel::toggleTaskCompletion(
    const std::string& taskId)
{
    const auto task = std::find_if(
        data().tasks.begin(), data().tasks.end(),
        [&taskId](const TaskRecord& item) { return item.id == taskId; });
    if (task == data().tasks.end()) {
        DataCommandResult result{
            DataCommandStatus::NotFound, {}, "任务已不存在，请刷新列表。"};
        publishResult(result);
        return result;
    }
    const auto result = service_->setTaskCompletion(
        task->id,
        task->status != TaskStatus::Done,
        task->revision,
        nowUtcMs());
    publishResult(result);
    return result;
}

DataCommandResult FocusViewModel::addTask(std::string title,
                                          int estimatedPomodoros,
                                          TaskExecutionPreferences execution)
{
    const auto result = service_->addTask({
        idProvider_(),
        std::move(title),
        estimatedPomodoros,
        nowUtcMs(),
        std::move(execution),
    });
    publishResult(result);
    return result;
}

DataCommandResult FocusViewModel::updateTask(
    const std::string& taskId,
    std::string title,
    int estimatedPomodoros,
    TaskExecutionPreferences execution,
    std::int64_t expectedRevision)
{
    const auto result = service_->updateTask({
        taskId,
        std::move(title),
        estimatedPomodoros,
        expectedRevision,
        nowUtcMs(),
        std::move(execution),
    });
    publishResult(result);
    return result;
}

DataCommandResult FocusViewModel::deleteTask(
    const std::string& taskId,
    std::int64_t expectedRevision)
{
    const auto result = service_->archiveTask(
        taskId, expectedRevision, nowUtcMs());
    publishResult(result);
    if (result.succeeded() && selectedTaskId_ == taskId) {
        selectedTaskId_.reset();
    }
    return result;
}

DataCommandResult FocusViewModel::restoreTask(
    const std::string& taskId,
    std::int64_t expectedRevision)
{
    const auto result = service_->restoreTask(
        taskId, expectedRevision, nowUtcMs());
    publishResult(result);
    return result;
}

DataCommandResult FocusViewModel::startSelectedFocus()
{
    const TaskRecord* task = selectedTask();
    if (task == nullptr) {
        DataCommandResult result{
            DataCommandStatus::NotFound, {}, "请选择一个仍在进行中的任务。"};
        publishResult(result);
        return result;
    }
    const std::int64_t duration = static_cast<std::int64_t>(
        effectiveFocusMinutes(*task, data().settings)) * kMinuteMs;
    const auto result = service_->startSession({
        idProvider_(),
        task->id,
        SessionType::Focus,
        duration,
        nowUtcMs(),
        false,
        effectiveSoundscapeId(*task, data().settings),
    });
    publishResult(result);
    if (result.succeeded()) captureClockCheckpoint();
    refreshRemainingText();
    return result;
}

DataCommandResult FocusViewModel::startFreeFocus()
{
    const std::int64_t duration =
        static_cast<std::int64_t>(data().settings.focusMinutes) * kMinuteMs;
    const std::optional<std::string> soundscape =
        data().settings.defaultSoundscapeId.empty()
        ? std::nullopt
        : std::optional<std::string>{data().settings.defaultSoundscapeId};
    const auto result = service_->startSession({
        idProvider_(),
        std::nullopt,
        SessionType::Focus,
        duration,
        nowUtcMs(),
        false,
        soundscape,
    });
    publishResult(result);
    if (result.succeeded()) captureClockCheckpoint();
    refreshRemainingText();
    return result;
}

DataCommandResult FocusViewModel::continueLastFocus()
{
    const auto session = std::find_if(
        data().sessions.rbegin(), data().sessions.rend(),
        [](const FocusSessionRecord& item) {
            return item.type == SessionType::Focus
                && item.status == SessionStatus::Completed;
        });
    if (session != data().sessions.rend() && session->taskId) {
        const auto task = std::find_if(
            data().tasks.begin(), data().tasks.end(),
            [&session](const TaskRecord& item) {
                return item.id == *session->taskId
                    && item.status == TaskStatus::Active;
            });
        if (task != data().tasks.end()) {
            selectedTaskId_ = task->id;
            return startSelectedFocus();
        }
    }
    return startFreeFocus();
}

DataCommandResult FocusViewModel::startBreak()
{
    const SessionType type = recommendedBreakType();
    const int minutes = type == SessionType::LongBreak
        ? data().settings.longBreakMinutes
        : data().settings.shortBreakMinutes;
    const std::int64_t duration = static_cast<std::int64_t>(minutes) * kMinuteMs;
    const auto result = service_->startSession(
        {
            idProvider_(),
            std::nullopt,
            type,
            duration,
            nowUtcMs(),
            !data().settings.autoStartBreak,
        });
    publishResult(result);
    if (result.succeeded()) captureClockCheckpoint();
    refreshRemainingText();
    return result;
}

SessionType FocusViewModel::recommendedBreakType() const noexcept
{
    int completedSinceLongBreak = 0;
    for (auto session = data().sessions.rbegin();
         session != data().sessions.rend(); ++session) {
        if (session->type == SessionType::LongBreak
            && isTerminalSessionStatus(session->status)) {
            break;
        }
        if (session->type == SessionType::Focus
            && session->status == SessionStatus::Completed) {
            ++completedSinceLongBreak;
        }
    }
    if (completedSinceLongBreak >= data().settings.longBreakEvery) {
        return SessionType::LongBreak;
    }
    return SessionType::ShortBreak;
}

DataCommandResult FocusViewModel::toggleActiveSession()
{
    const auto* session = activeSession();
    if (session == nullptr) {
        DataCommandResult result{
            DataCommandStatus::NotFound, {}, "当前没有可控制的专注会话。"};
        publishResult(result);
        return result;
    }
    const auto result = session->status == SessionStatus::Running
        ? service_->pauseSession(session->id, nowUtcMs())
        : service_->resumeSession(session->id, nowUtcMs());
    publishResult(result);
    if (result.succeeded()) captureClockCheckpoint();
    refreshRemainingText();
    return result;
}

DataCommandResult FocusViewModel::resetActiveSession()
{
    const auto* session = activeSession();
    if (session == nullptr) {
        DataCommandResult result{
            DataCommandStatus::NotFound, {}, "当前没有可重置的专注会话。"};
        publishResult(result);
        return result;
    }
    const auto result = service_->resetSession(session->id, nowUtcMs());
    publishResult(result);
    refreshRemainingText();
    return result;
}

DataCommandResult FocusViewModel::abortActiveSession()
{
    const auto* session = activeSession();
    if (session == nullptr) {
        DataCommandResult result{
            DataCommandStatus::NotFound, {}, "当前没有可结束的专注会话。"};
        publishResult(result);
        return result;
    }
    const auto result = service_->abortSession(session->id, nowUtcMs());
    publishResult(result);
    refreshRemainingText();
    return result;
}

DataCommandResult FocusViewModel::skipActiveBreak()
{
    const auto* session = activeSession();
    if (session == nullptr) {
        DataCommandResult result{
            DataCommandStatus::NotFound, {}, "当前没有可跳过的休息会话。"};
        publishResult(result);
        return result;
    }
    const auto result =
        service_->skipBreakSession(session->id, nowUtcMs());
    publishResult(result);
    refreshRemainingText();
    return result;
}

bool FocusViewModel::updateClock()
{
    const auto* session = activeSession();
    if (session == nullptr) {
        refreshRemainingText();
        return false;
    }

    std::int64_t now = nowUtcMs();
    if (session->status == SessionStatus::CompletionPending) {
        const auto completed = service_->finalizeCompletion(session->id, now);
        publishResult(completed);
        refreshRemainingText();
        return completed.succeeded();
    }
    if (session->status != SessionStatus::Running
        || !session->targetEndAtUtcMs) {
        refreshRemainingText();
        return false;
    }
    if (const auto safeNow = driftSafeNow(now)) {
        const ClockDriftStatus driftStatus =
            now < *safeNow ? ClockDriftStatus::WallClockJumpedBackward
                           : ClockDriftStatus::WallClockJumpedForward;
        now = *safeNow;
        if (now < *session->targetEndAtUtcMs) {
            const auto paused = service_->pauseSession(session->id, now);
            publishResult(paused);
            if (paused.succeeded()) publishClockRecoveryMessage(driftStatus);
            refreshRemainingText();
            return false;
        }
    }
    if (now < *session->targetEndAtUtcMs) {
        refreshRemainingText();
        return false;
    }
    const std::string sessionId = session->id;
    auto pending = service_->markDeadlineReached(sessionId, now);
    publishResult(pending);
    if (!pending.succeeded()) return false;
    auto completed = service_->finalizeCompletion(sessionId, now);
    publishResult(completed);
    refreshRemainingText();
    return completed.succeeded();
}

void FocusViewModel::restoreSelectedTask()
{
    const auto* session = activeSession();
    if (session == nullptr || !session->taskId) return;
    const auto task = std::find_if(
        data().tasks.begin(), data().tasks.end(),
        [session](const TaskRecord& item) {
            return item.id == *session->taskId;
        });
    if (task != data().tasks.end()) selectedTaskId_ = task->id;
}

void FocusViewModel::captureClockCheckpoint()
{
    if (!monotonicProvider_) return;
    const std::int64_t wall = nowUtcMs();
    const std::int64_t monotonic = monotonicProvider_();
    if (wall > 0 && monotonic >= 0) {
        clockCheckpoint_ = ClockCheckpoint{wall, monotonic};
    }
}

std::optional<std::int64_t> FocusViewModel::driftSafeNow(
    std::int64_t wallNowUtcMs)
{
    if (!monotonicProvider_) return std::nullopt;
    const ClockCheckpoint current{
        wallNowUtcMs,
        monotonicProvider_(),
    };
    if (!clockCheckpoint_) {
        clockCheckpoint_ = current;
        return std::nullopt;
    }
    const ClockCheckpoint previous = *clockCheckpoint_;
    clockCheckpoint_ = current;
    const auto drift = detectClockDrift(previous, current);
    if (drift.status == ClockDriftStatus::Normal) return std::nullopt;
    if (drift.status == ClockDriftStatus::InvalidCheckpoint) {
        return std::nullopt;
    }
    return previous.wallUtcMs
        + (current.monotonicMs - previous.monotonicMs);
}

void FocusViewModel::publishClockRecoveryMessage(ClockDriftStatus status)
{
    operationMessage_.set(
        status == ClockDriftStatus::WallClockJumpedBackward
            ? "检测到系统时间向后变化，计时已按实际经过时间暂停。请确认后继续。"
            : "检测到系统时间向前变化，计时已按实际经过时间暂停。请确认后继续。");
    hasOperationMessage_.set(true);
}

wui::State<std::string>& FocusViewModel::remainingText() noexcept
{
    return remainingText_;
}

wui::State<std::string>& FocusViewModel::operationMessage() noexcept
{
    return operationMessage_;
}

wui::State<bool>& FocusViewModel::hasOperationMessage() noexcept
{
    return hasOperationMessage_;
}

std::int64_t FocusViewModel::nowUtcMs() const
{
    return nowProvider_ ? nowProvider_() : 0;
}

void FocusViewModel::publishResult(const DataCommandResult& result)
{
    if (result.status == DataCommandStatus::Success
        || result.status == DataCommandStatus::NoChange) {
        hasOperationMessage_.set(false);
        operationMessage_.set({});
        return;
    }
    std::string message = result.message;
    if (!result.validation.ok()) {
        message += message.empty() ? result.validation.summary()
                                   : "\n" + result.validation.summary();
    }
    operationMessage_.set(std::move(message));
    hasOperationMessage_.set(true);
}

void FocusViewModel::refreshRemainingText()
{
    const auto* session = activeSession();
    if (session == nullptr) {
        remainingText_.set(formatRemaining(
            static_cast<std::int64_t>(data().settings.focusMinutes) * kMinuteMs));
        return;
    }
    std::int64_t remaining = session->remainingMs;
    if (session->status == SessionStatus::Running && session->targetEndAtUtcMs) {
        remaining = std::max<std::int64_t>(0, *session->targetEndAtUtcMs - nowUtcMs());
    }
    remainingText_.set(formatRemaining(remaining));
}

std::string FocusViewModel::formatRemaining(std::int64_t milliseconds)
{
    const std::int64_t totalSeconds =
        std::max<std::int64_t>(0, milliseconds + 999) / 1000;
    const std::int64_t minutes = totalSeconds / 60;
    const std::int64_t seconds = totalSeconds % 60;
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << minutes
           << ':' << std::setw(2) << seconds;
    return output.str();
}

} // namespace whatsui::focus_tomato::presentation
