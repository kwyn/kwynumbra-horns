#pragma once

#include <cstdint>
#include <FastLED.h>

void initBLE();

// Standard BLE Battery Service (0x180F) — phones and BLE scanners render it
// natively, so there is nothing custom to teach a client.
void setBatteryLevel(uint8_t percent);

uint8_t getCurrentEffect();
uint8_t getCurrentBrightness();
CRGB getColor1();
CRGB getColor2();
CRGB getColor3();
uint8_t getBPM();

// 2 or 3 — how many of the colours above an effect should use.
uint8_t getColorCount();

// Audio analysis, computed on the phone and streamed in (0.0 – 1.0). All read
// zero once the stream goes stale, so a dropped phone fades rather than
// freezing — see AUDIO_STALE_MS.
//
// getKick() is a bass *onset*, not a bass level. Sustained sub (a reece, a
// wobble) holds getBassEnergy() high for a whole drop while the kick is still
// a series of distinct hits; driving brightness from the level reads as "bright
// the entire time" with no beat visible at all.
float getKick();
float getBassEnergy();
float getMidEnergy();
float getHighEnergy();

// Spectral tilt, -1 (all sub) to +1 (all air), smoothed over seconds. Advanced
// by updateAudioState(), which must be called once per frame.
float getTilt();
void updateAudioState();
