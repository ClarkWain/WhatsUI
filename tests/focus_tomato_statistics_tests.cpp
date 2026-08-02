#include "focus_statistics.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

using namespace whatsui::focus_tomato;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

FocusSessionRecord session(std::string id,
                           SessionType type,
                           SessionStatus status,
                           std::optional<std::string> taskId,
                           std::int64_t endedAt,
                           CompletionReason reason = CompletionReason::Natural)
{
    return {
        std::move(id),
        std::move(taskId),
        "Snapshot title",
        type,
        25 * kMinuteMs,
        endedAt - 25 * kMinuteMs,
        std::nullopt,
        0,
        status,
        endedAt,
        reason,
        {},
    };
}

void statisticsUseOnlyCompletedFocusFacts()
{
    FocusData data;
    auto taskFocus = session("focus-task", SessionType::Focus,
                             SessionStatus::Completed, std::string{"archived-task"}, 2'000'000);
    taskFocus.idempotencyKey = taskFocus.id;
    auto freeFocus = session("focus-free", SessionType::Focus,
                             SessionStatus::Completed, std::nullopt, 3'000'000);
    freeFocus.idempotencyKey = freeFocus.id;
    auto aborted = session("focus-aborted", SessionType::Focus,
                           SessionStatus::Aborted, std::string{"archived-task"}, 3'500'000,
                           CompletionReason::UserAborted);
    aborted.idempotencyKey = aborted.id;
    auto shortBreak = session("short-break", SessionType::ShortBreak,
                              SessionStatus::Completed, std::nullopt, 3'700'000);
    shortBreak.idempotencyKey = shortBreak.id;
    data.sessions = {taskFocus, freeFocus, aborted, shortBreak};

    const auto result = calculateFocusStatistics(data, 1'000'000, 4'000'000);
    expect(result.completedFocusSessions == 2,
           "Breaks and aborted sessions must be excluded");
    expect(result.completedFreeFocusSessions == 1,
           "Free focus must remain in global statistics");
    expect(result.focusedDurationMs == 50 * kMinuteMs,
           "Natural completion should credit planned focus duration");
    expect(result.completedSessionsByTask.at("archived-task") == 1,
           "Archived task history must remain queryable by task ID snapshot");
}

void manualCompletionCreditsOnlyElapsedDuration()
{
    FocusData data;
    auto manual = session("manual", SessionType::Focus, SessionStatus::Completed,
                          std::nullopt, 1'600'000, CompletionReason::Manual);
    manual.idempotencyKey = manual.id;
    manual.startedAtUtcMs = 1'000'000;
    data.sessions.push_back(manual);

    const auto result = calculateFocusStatistics(data, 1, 2'000'000);
    expect(result.focusedDurationMs == 600'000,
           "Manual completion must not over-credit the planned duration");
    expect(calculateFocusStatistics(data, 1'600'001, 2'000'000)
               .completedFocusSessions == 0,
           "Time windows must use completedAt with [from, to) bounds");
}

} // namespace

int main()
{
    try {
        statisticsUseOnlyCompletedFocusFacts();
        manualCompletionCreditsOnlyElapsedDuration();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
