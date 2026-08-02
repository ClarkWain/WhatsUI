#include "clock_drift.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

using namespace whatsui::focus_tomato;

void expect(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void normalElapsedAndSleepRemainComparable()
{
    const auto result = detectClockDrift(
        {1'000'000, 50'000},
        {1'600'300, 650'000});
    expect(result.status == ClockDriftStatus::Normal && result.driftMs == 300,
           "Small scheduler/wall differences must stay below the recovery threshold");
}

void wallClockJumpsAreDirectional()
{
    expect(detectClockDrift(
               {1'000'000, 50'000},
               {1'900'000, 350'000})
               .status == ClockDriftStatus::WallClockJumpedForward,
           "A large forward wall-clock jump must require recovery policy");
    expect(detectClockDrift(
               {1'000'000, 50'000},
               {700'000, 350'000})
               .status == ClockDriftStatus::WallClockJumpedBackward,
           "A large backward wall-clock jump must not silently extend the timer");
}

void processOrBootBoundaryInvalidatesMonotonicCheckpoint()
{
    expect(detectClockDrift(
               {1'000'000, 500'000},
               {1'010'000, 100})
               .status == ClockDriftStatus::InvalidCheckpoint,
           "A reset monotonic clock must use persisted UTC recovery instead of drift math");
}

} // namespace

int main()
{
    try {
        normalElapsedAndSleepRemainComparable();
        wallClockJumpsAreDirectional();
        processOrBootBoundaryInvalidatesMonotonicCheckpoint();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
