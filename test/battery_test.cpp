// Curve interpolation for the battery gauge. No framework — asserts and a main().
#include "battery.h"
#include <cassert>
#include <cstdio>

int main() {
    // Clamps at both ends.
    assert(batteryPercent(2000) == 0);
    assert(batteryPercent(3300) == 0);
    assert(batteryPercent(4200) == 100);
    assert(batteryPercent(5000) == 100);

    // Breakpoints land exactly on their step.
    assert(batteryPercent(3600) == 10);
    assert(batteryPercent(3750) == 30);
    assert(batteryPercent(3980) == 80);

    // Interpolates between breakpoints.
    assert(batteryPercent(3770) == 35);   // halfway 3750..3790
    assert(batteryPercent(4199) == 99);

    // Monotonic across the whole range — catches a bad loop bound or off-by-one.
    uint8_t prev = 0;
    for (uint16_t mv = 3000; mv <= 4300; mv++) {
        uint8_t pct = batteryPercent(mv);
        assert(pct >= prev);
        assert(pct <= 100);
        prev = pct;
    }

    printf("PASS - battery curve\n");
    return 0;
}
