#include <cassert>
#include <cstdint>
#include "skyline/kernel/scheduler_wake.h"

using skyline::kernel::ShouldWakeParkedThreadForTimeslice;

int main() {
    // Lower numeric values represent higher priorities.
    assert(ShouldWakeParkedThreadForTimeslice(2, 5, false, 10, 0));
    assert(!ShouldWakeParkedThreadForTimeslice(6, 5, false, 10, 0));

    // A single runnable thread, or a next thread of a different priority, has no equal-priority successor.
    assert(ShouldWakeParkedThreadForTimeslice(5, 5, false, 10, 0));

    // For equal priorities, preserve the established earlier-timeslice ordering.
    assert(ShouldWakeParkedThreadForTimeslice(5, 5, true, 10, 11));
    assert(!ShouldWakeParkedThreadForTimeslice(5, 5, true, 11, 10));
    assert(!ShouldWakeParkedThreadForTimeslice(5, 5, true, 10, 10));
}
