#include "focus_statistics.h"

#include <algorithm>
#include <ctime>

namespace whatsui::focus_tomato {

UtcTimeRange localDayUtcRange(std::int64_t nowUtcMs) noexcept
{
    if (nowUtcMs <= 0) return {};
    const std::time_t nowSeconds =
        static_cast<std::time_t>(nowUtcMs / 1000);
    std::tm local{};
#if defined(_WIN32)
    if (::localtime_s(&local, &nowSeconds) != 0) return {};
#else
    if (::localtime_r(&nowSeconds, &local) == nullptr) return {};
#endif
    local.tm_hour = 0;
    local.tm_min = 0;
    local.tm_sec = 0;
    local.tm_isdst = -1;
    const std::time_t from = std::mktime(&local);
    if (from == static_cast<std::time_t>(-1)) return {};
    local.tm_mday += 1;
    local.tm_isdst = -1;
    const std::time_t to = std::mktime(&local);
    if (to == static_cast<std::time_t>(-1) || to <= from) return {};
    return {
        static_cast<std::int64_t>(from) * 1000,
        static_cast<std::int64_t>(to) * 1000,
    };
}

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
