#pragma once

#include <cstdint>

// LED strip — LilyGO T-Energy S3. Any free GPIO works; 4 is not a strapping
// pin (0/3/45/46) and is clear of the flash/PSRAM and USB pins.
constexpr uint8_t LED_PIN = 4;
// 53 per horn, measured. Both horns hang off IO4 in parallel — same data, so
// they mirror in hardware and the firmware only ever renders one horn's worth.
constexpr uint16_t NUM_LEDS = 53;
constexpr uint8_t MAX_BRIGHTNESS = 128; // Cap for power budget (50%)

// Audio — analysed on the phone and streamed in over BLE. The board does no
// DSP of its own. A 2048-point double-precision FFT does not fit in a 60fps
// render loop on an S3: its FPU is single-precision only, so doubles are
// software-emulated, and collecting 2048 samples at 16kHz costs 128ms before
// any maths starts. The phone has a hardware-accelerated FFT sitting idle.
//
// Past this long with no write, the energies read zero. Load-bearing: without
// it a phone that disconnects, backgrounds or locks freezes the strip on
// whatever it was showing, and a sound-reactive effect holds solid.
constexpr uint32_t AUDIO_STALE_MS = 500;

// Spectral tilt is meant to track the *track*, not the moment — a dark rolling
// section drifting one way, a bright break the other. Per-frame EMA weight, so
// this is roughly an eight-second time constant at 60fps.
// ponytail: feel knob. Raise it and the colour starts twitching per-beat.
constexpr float TILT_SMOOTHING = 0.002f;


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
// How many of the three colours are in play. Replaces the old convention of
// sending black as colour 3 to mean "two colours", which made an intentionally
// black segment unreachable and silently blanked the spectrum effect's top zone.
constexpr const char* COLORCOUNT_CHAR_UUID = "19b10007-e8f2-537e-4f6c-d104768a1214";
// Five bytes: kick, bass, mid, high, balance. Written ~25Hz by the phone.
// Balance is spectral tilt, 0 = all sub, 128 = neutral, 255 = all air. It comes
// from the phone rather than being derived here because it has to be computed
// from raw band levels — the per-band energies above are each normalised to
// their own range, so comparing them answers "which band is busier", not
// "where does the energy sit".
constexpr const char* AUDIO_CHAR_UUID      = "19b10008-e8f2-537e-4f6c-d104768a1214";

// Colour pipeline
// Colours arrive as screen hex — sRGB, gamma-encoded. WS2812s are near-linear
// in PWM, so writing sRGB straight through drives the dark channel of a colour
// far too hot and everything washes out toward white. 2.2 is the sRGB display
// gamma, which is why these look right on a monitor and pastel on a strip.
// ponytail: calibration knob. Raise past 2.2 for deeper, more contrasty colour,
// lower it if the dim end of a fade starts dropping out entirely.
constexpr float COLOR_GAMMA = 2.2f;

// Effects
constexpr uint8_t NUM_EFFECTS = 5;
constexpr uint8_t EFFECT_RAINBOW    = 0;
constexpr uint8_t EFFECT_CHASE      = 1;
constexpr uint8_t EFFECT_BASS_PULSE = 2;
constexpr uint8_t EFFECT_SPECTRUM   = 3;
constexpr uint8_t EFFECT_FLOW       = 4;

// BPM
constexpr uint8_t DEFAULT_BPM = 128;
constexpr uint8_t MIN_BPM = 60;
constexpr uint8_t MAX_BPM = 200;
