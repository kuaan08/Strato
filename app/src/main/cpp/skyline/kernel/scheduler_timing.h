// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <cstdint>

namespace skyline::kernel::scheduler_detail {
    /**
     * @brief Updates a timeslice EWMA with 25% previous value and 75% new sample.
     *
     * The quotient/remainder form preserves the integer result of
     * (previous * 1 + elapsed * 3) / 4 without overflowing uint64_t.
     */
    constexpr std::uint64_t UpdateAverageTimeslice(std::uint64_t previous, std::uint64_t elapsed) noexcept {
        return previous / 4 + (elapsed / 4) * 3 + (previous % 4 + (elapsed % 4) * 3) / 4;
    }

    /**
     * @brief Returns the estimated time remaining before a queued thread runs.
     *
     * A thread with no history is estimated from its current elapsed timeslice,
     * matching the scheduler's existing cold-start policy. A known estimate is
     * saturated to one tick after it expires rather than wrapping or returning
     * an unbounded value.
     */
    constexpr std::uint64_t EstimateRemainingTimeslice(std::uint64_t average, std::uint64_t elapsed,
                                                       bool hasStartedTimeslice) noexcept {
        if (average == 0)
            return hasStartedTimeslice ? elapsed : 1;
        return average > elapsed ? average - elapsed : 1;
    }

    constexpr std::uint64_t ElapsedTimeslice(std::uint64_t now, std::uint64_t start) noexcept {
        return now >= start ? now - start : 0;
    }
}
