#pragma once

#include <cstdint>

// LED strip — LilyGO T-Energy S3. Any free GPIO works; 4 is not a strapping
// pin (0/3/45/46) and is clear of the flash/PSRAM and USB pins.
constexpr uint8_t LED_PIN = 4;
constexpr uint16_t NUM_LEDS = 60;
constexpr uint8_t MAX_BRIGHTNESS = 128; // Cap for power budget (50%)

// Mic — none on this board yet, see audio.cpp. Rate/size kept because the FFT
// bin math in analyzeFrequencies() is calibrated against them.
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr uint16_t FFT_SAMPLES = 2048;

// Battery — T-Energy S3 wires the 18650 to IO3 through a 1:2 divider. While
// USB-C is plugged in this reads the charger rail, not the cell.
constexpr uint8_t BATTERY_ADC_PIN = 3;
constexpr float BATTERY_DIVIDER = 2.0f;   // ponytail: calibration knob, trim against a meter
constexpr uint32_t BATTERY_SAMPLE_MS = 10000;

// Unprotected cells carry no cutoff of their own, so this is the only thing
// standing between a long night and a damaged (and dangerous to recharge) cell.
// Measured under LED load, where an 18650 sags well below its resting voltage.
constexpr uint16_t BATTERY_CUTOFF_MV = 3200;
// A reading under this is broken wiring or a floating pin, not a flat cell —
// don't sleep on it, or bad ADC wiring looks like a bricked board.
constexpr uint16_t BATTERY_FLOOR_MV = 2000;
constexpr uint32_t BATTERY_SLEEP_S = 300; // recheck this often, so it wakes back up on a charger

// BLE
constexpr const char* DEVICE_NAME = "Kwynumbra";
constexpr const char* SERVICE_UUID         = "19b10000-e8f2-537e-4f6c-d104768a1214";
constexpr const char* EFFECT_CHAR_UUID     = "19b10001-e8f2-537e-4f6c-d104768a1214";
constexpr const char* BRIGHTNESS_CHAR_UUID = "19b10002-e8f2-537e-4f6c-d104768a1214";
constexpr const char* COLOR1_CHAR_UUID     = "19b10003-e8f2-537e-4f6c-d104768a1214";
constexpr const char* COLOR2_CHAR_UUID     = "19b10004-e8f2-537e-4f6c-d104768a1214";
constexpr const char* COLOR3_CHAR_UUID     = "19b10005-e8f2-537e-4f6c-d104768a1214";
constexpr const char* BPM_CHAR_UUID        = "19b10006-e8f2-537e-4f6c-d104768a1214";

// Colour pipeline
// Colours arrive as screen hex — sRGB, gamma-encoded. WS2812s are near-linear
// in PWM, so writing sRGB straight through drives the dark channel of a colour
// far too hot and everything washes out toward white. 2.2 is the sRGB display
// gamma, which is why these look right on a monitor and pastel on a strip.
// ponytail: calibration knob. Raise past 2.2 for deeper, more contrasty colour,
// lower it if the dim end of a fade starts dropping out entirely.
constexpr float COLOR_GAMMA = 2.2f;

// Effects
constexpr uint8_t NUM_EFFECTS = 4;
constexpr uint8_t EFFECT_RAINBOW    = 0;
constexpr uint8_t EFFECT_CHASE      = 1;
constexpr uint8_t EFFECT_BASS_PULSE = 2;
constexpr uint8_t EFFECT_SPECTRUM   = 3;

// BPM
constexpr uint8_t DEFAULT_BPM = 128;
constexpr uint8_t MIN_BPM = 60;
constexpr uint8_t MAX_BPM = 200;
