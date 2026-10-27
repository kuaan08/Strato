// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <cstdint>

namespace skyline::kernel {
    /**
     * @brief Decide whether a parked thread may replace the current/next scheduled thread.
     *
     * Priorities use the scheduler convention: lower numeric values are higher priority.
     */
    inline constexpr bool ShouldWakeParkedThreadForTimeslice(
        std::int8_t parkedPriority,
        std::int8_t currentPriority,
        bool nextThreadHasSamePriority,
        std::uint64_t parkedTimesliceStart,
        std::uint64_t nextTimesliceStart) noexcept {
        return parkedPriority < currentPriority ||
               (parkedPriority == currentPriority &&
                (!nextThreadHasSamePriority || parkedTimesliceStart < nextTimesliceStart));
    }
}
