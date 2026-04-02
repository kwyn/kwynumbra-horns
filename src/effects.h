#pragma once

#include <FastLED.h>
#include <cstdint>

void runEffect(uint8_t effectIndex, CRGB* leds, uint16_t numLeds);
bool effectIsSoundReactive(uint8_t effectIndex);
