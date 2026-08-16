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
