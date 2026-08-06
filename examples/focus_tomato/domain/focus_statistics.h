#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "focus_data.h"

namespace whatsui::focus_tomato {

struct FocusStatistics {
    int completedFocusSessions{0};
    int completedFreeFocusSessions{0};
    std::int64_t focusedDurationMs{0};
    std::unordered_map<std::string, int> completedSessionsByTask;
    std::unordered_map<std::string, std::int64_t> focusedDurationMsByTask;
};

struct UtcTimeRange {
    std::int64_t fromUtcMs{0};
    std::int64_t toUtcMs{0};
};

[[nodiscard]] UtcTimeRange localDayUtcRange(
    std::int64_t nowUtcMs) noexcept;

// Statistics are projections only. FocusSession records are the source of
// truth; Task.completedPomodoros and future daily summary tables are caches.
[[nodiscard]] FocusStatistics calculateFocusStatistics(
    const FocusData& data,
    std::int64_t completedFromUtcMsInclusive,
    std::int64_t completedToUtcMsExclusive);

} // namespace whatsui::focus_tomato
