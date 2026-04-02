#pragma once

#include <cstdint>
#include <FastLED.h>

void initBLE();

uint8_t getCurrentEffect();
uint8_t getCurrentBrightness();
CRGB getColor1();
CRGB getColor2();
CRGB getColor3();
uint8_t getBPM();
