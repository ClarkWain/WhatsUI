#pragma once

#include <cstdint>

namespace whatsui::focus_tomato {

enum class ClockDriftStatus {
    Normal,
    WallClockJumpedForward,
    WallClockJumpedBackward,
    InvalidCheckpoint,
};

struct ClockCheckpoint {
    std::int64_t wallUtcMs{0};
    std::int64_t monotonicMs{0};
};

struct ClockDriftResult {
    ClockDriftStatus status{ClockDriftStatus::Normal};
    std::int64_t driftMs{0};
};

// Compares elapsed wall time with elapsed monotonic time. UTC is deliberate:
// timezone changes do not affect it and therefore do not trigger a false
// tampering warning. Monotonic rollback means the process/boot checkpoint is
// no longer comparable and must be replaced.
[[nodiscard]] ClockDriftResult detectClockDrift(
    ClockCheckpoint previous,
    ClockCheckpoint current,
    std::int64_t toleranceMs = 2 * 60'000);

} // namespace whatsui::focus_tomato
