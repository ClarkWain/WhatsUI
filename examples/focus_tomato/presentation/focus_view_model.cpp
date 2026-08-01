#include "focus_view_model.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace whatsui::focus_tomato::presentation {

FocusViewModel::FocusViewModel(FocusDataService& service, NowProvider nowProvider,
                               IdProvider idProvider)
    : service_(&service)
    , nowProvider_(std::move(nowProvider))
    , idProvider_(std::move(idProvider))
{
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

void FocusViewModel::selectTask(std::string taskId)
{
    selectedTaskId_ = std::move(taskId);
}

DataCommandResult FocusViewModel::addTask(std::string title,
                                          int estimatedPomodoros)
{
    const auto result = service_->addTask(
        {idProvider_(), std::move(title), estimatedPomodoros, nowUtcMs()});
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
    const std::int64_t duration =
        static_cast<std::int64_t>(data().settings.focusMinutes) * kMinuteMs;
    const auto result = service_->startSession(
        {idProvider_(), task->id, SessionType::Focus, duration, nowUtcMs()});
    publishResult(result);
    refreshRemainingText();
    return result;
}

DataCommandResult FocusViewModel::startShortBreak()
{
    const std::int64_t duration =
        static_cast<std::int64_t>(data().settings.shortBreakMinutes)
        * kMinuteMs;
    const auto result = service_->startSession(
        {
            idProvider_(),
            std::nullopt,
            SessionType::ShortBreak,
            duration,
            nowUtcMs(),
        });
    publishResult(result);
    refreshRemainingText();
    return result;
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
    if (session == nullptr || session->status != SessionStatus::Running
        || !session->targetEndAtUtcMs) {
        refreshRemainingText();
        return false;
    }

    const std::int64_t now = nowUtcMs();
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
