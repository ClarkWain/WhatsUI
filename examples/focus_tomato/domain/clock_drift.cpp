#include "clock_drift.h"

namespace whatsui::focus_tomato {

ClockDriftResult detectClockDrift(
    ClockCheckpoint previous,
    ClockCheckpoint current,
    std::int64_t toleranceMs)
{
    if (previous.wallUtcMs <= 0 || previous.monotonicMs < 0
        || current.wallUtcMs <= 0 || current.monotonicMs < previous.monotonicMs
        || toleranceMs < 0) {
        return {ClockDriftStatus::InvalidCheckpoint, 0};
    }
    const std::int64_t wallElapsed = current.wallUtcMs - previous.wallUtcMs;
    const std::int64_t monotonicElapsed = current.monotonicMs - previous.monotonicMs;
    const std::int64_t drift = wallElapsed - monotonicElapsed;
    if (drift > toleranceMs) {
        return {ClockDriftStatus::WallClockJumpedForward, drift};
    }
    if (drift < -toleranceMs) {
        return {ClockDriftStatus::WallClockJumpedBackward, drift};
    }
    return {ClockDriftStatus::Normal, drift};
}

} // namespace whatsui::focus_tomato
