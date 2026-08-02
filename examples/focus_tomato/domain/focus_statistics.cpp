#include "focus_statistics.h"

#include <algorithm>

namespace whatsui::focus_tomato {

FocusStatistics calculateFocusStatistics(
    const FocusData& data,
    std::int64_t completedFromUtcMsInclusive,
    std::int64_t completedToUtcMsExclusive)
{
    FocusStatistics result;
    if (completedToUtcMsExclusive <= completedFromUtcMsInclusive) return result;

    for (const auto& session : data.sessions) {
        if (session.type != SessionType::Focus
            || session.status != SessionStatus::Completed
            || !session.endedAtUtcMs
            || *session.endedAtUtcMs < completedFromUtcMsInclusive
            || *session.endedAtUtcMs >= completedToUtcMsExclusive) {
            continue;
        }

        std::int64_t creditedDuration = session.plannedDurationMs;
        if (session.completionReason == CompletionReason::Manual) {
            creditedDuration = std::clamp(
                *session.endedAtUtcMs - session.startedAtUtcMs,
                std::int64_t{0},
                session.plannedDurationMs);
        }
        ++result.completedFocusSessions;
        result.focusedDurationMs += creditedDuration;
        if (session.taskId) {
            ++result.completedSessionsByTask[*session.taskId];
            result.focusedDurationMsByTask[*session.taskId] += creditedDuration;
        } else {
            ++result.completedFreeFocusSessions;
        }
    }
    return result;
}

} // namespace whatsui::focus_tomato
