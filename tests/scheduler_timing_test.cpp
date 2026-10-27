// SPDX-License-Identifier: MPL-2.0
#include "app/src/main/cpp/skyline/kernel/scheduler_timing.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>

using skyline::kernel::scheduler_detail::ElapsedTimeslice;
using skyline::kernel::scheduler_detail::EstimateRemainingTimeslice;
using skyline::kernel::scheduler_detail::UpdateAverageTimeslice;

int main() {
    // EWMA: 25% of the old estimate plus 75% of the latest duration.
    assert(UpdateAverageTimeslice(100, 20) == 40);
    assert(UpdateAverageTimeslice(40, 200) == 160);
    assert(UpdateAverageTimeslice(0, 8) == 6);
    assert(UpdateAverageTimeslice(100, 0) == 25);

    // The decomposition must remain safe even at the uint64_t boundary.
    constexpr auto max = std::numeric_limits<std::uint64_t>::max();
    static_assert(UpdateAverageTimeslice(max, max) == max);
    static_assert(UpdateAverageTimeslice(max, 0) == max / 4);
    static_assert(UpdateAverageTimeslice(0, max) == (max / 4) * 3 + ((max % 4) * 3) / 4);

    // Remaining time saturates at one tick instead of returning a zero or a
    // wrapped unsigned value after the observed average duration has elapsed.
    assert(EstimateRemainingTimeslice(100, 20, true) == 80);
    assert(EstimateRemainingTimeslice(100, 100, true) == 1);
    assert(EstimateRemainingTimeslice(100, 101, true) == 1);

    // Preserve the scheduler's existing no-history behavior.
    assert(EstimateRemainingTimeslice(0, 0, false) == 1);
    assert(EstimateRemainingTimeslice(0, 20, true) == 20);
    assert(EstimateRemainingTimeslice(0, 0, true) == 0);

    assert(ElapsedTimeslice(120, 100) == 20);
    assert(ElapsedTimeslice(100, 120) == 0);
    assert(ElapsedTimeslice(0, 0) == 0);

    std::cout << "PASS: scheduler EWMA, remaining timeslice, cold start and uint64_t bounds\n";
}
