#pragma once

#include <cstdint>

// Li-ion (18650) discharge curve: resting millivolts at each 10% step.
// A straight voltage->percent line is badly wrong for li-ion — the cell sits
// between 3.7V and 3.9V for most of its usable charge, so a linear reading
// shows "half full" while the pack is nearly done.
// ponytail: generic 18650 curve. Calibration knob — run your actual cell down
// once at your usual brightness and retune these if the percentage lies.
constexpr uint16_t BATTERY_CURVE[11] = {
    3300, 3600, 3700, 3750, 3790, 3830, 3870, 3920, 3980, 4080, 4200
};

// Pure so it can be tested without a battery — see test/battery_test.cpp.
inline uint8_t batteryPercent(uint16_t mv) {
    if (mv >= BATTERY_CURVE[10]) return 100;
    for (uint8_t i = 1; i <= 10; i++) {
        if (mv < BATTERY_CURVE[i]) {
            uint16_t lo = BATTERY_CURVE[i - 1];
            if (mv <= lo) return (i - 1) * 10;
            return (i - 1) * 10 + (mv - lo) * 10 / (BATTERY_CURVE[i] - lo);
        }
    }
    return 100;
}
