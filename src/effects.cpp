#include "effects.h"
#include "config.h"
#include "audio.h"
#include "ble_control.h"

// --- Rainbow (BPM-synced) ---
static uint8_t rainbowHue = 0;
static unsigned long lastRainbowBeat = 0;

static void effectRainbow(CRGB* leds, uint16_t numLeds) {
    unsigned long beatInterval = 60000UL / getBPM();
    unsigned long now = millis();
    if (now - lastRainbowBeat >= beatInterval) {
        lastRainbowBeat = now;
        rainbowHue += 8;
    }
    fill_rainbow(leds, numLeds, rainbowHue, 7);
}

// --- Chase (BPM-synced, 3 colors) ---
static uint16_t chaseOffset = 0;
static unsigned long lastChaseBeat = 0;

static void effectChase(CRGB* leds, uint16_t numLeds) {
    // Advance one LED position per beat, full cycle = numLeds beats
    unsigned long beatInterval = 60000UL / getBPM();
    unsigned long now = millis();
    if (now - lastChaseBeat >= beatInterval) {
        lastChaseBeat = now;
        chaseOffset = (chaseOffset + 1) % numLeds;
    }

    CRGB colors[3] = { getColor1(), getColor2(), getColor3() };
    uint16_t segmentLen = numLeds / 3;

    fadeToBlackBy(leds, numLeds, 60);

    for (uint16_t i = 0; i < numLeds; i++) {
        uint16_t pos = (i + chaseOffset) % numLeds;
        uint8_t colorIdx = i / segmentLen;
        if (colorIdx > 2) colorIdx = 2;
        leds[pos] = colors[colorIdx];
    }
}

// --- Bass Pulse (sound-reactive, uses color 1) ---
static float smoothedBass = 0.0f;

static void effectBassPulse(CRGB* leds, uint16_t numLeds) {
    float bass = getBassEnergy();
    if (bass > smoothedBass) {
        smoothedBass = bass;
    } else {
        smoothedBass *= 0.85f;
    }

    uint8_t brightness = static_cast<uint8_t>(smoothedBass * 255.0f);
    CRGB color = blend(getColor1(), CRGB::White, brightness);
    fill_solid(leds, numLeds, color);
    fadeToBlackBy(leds, numLeds, 255 - brightness);
}

// --- Spectrum (sound-reactive, 3 color zones) ---
static void effectSpectrum(CRGB* leds, uint16_t numLeds) {
    float bass = getBassEnergy();
    float mid = getMidEnergy();
    float high = getHighEnergy();

    uint16_t zone1End = numLeds / 3;
    uint16_t zone2End = (numLeds * 2) / 3;

    uint8_t bassBright = static_cast<uint8_t>(bass * 255.0f);
    for (uint16_t i = 0; i < zone1End; i++) {
        leds[i] = getColor1();
        leds[i].fadeToBlackBy(255 - bassBright);
    }

    uint8_t midBright = static_cast<uint8_t>(mid * 255.0f);
    for (uint16_t i = zone1End; i < zone2End; i++) {
        leds[i] = getColor2();
        leds[i].fadeToBlackBy(255 - midBright);
    }

    uint8_t highBright = static_cast<uint8_t>(high * 255.0f);
    for (uint16_t i = zone2End; i < numLeds; i++) {
        leds[i] = getColor3();
        leds[i].fadeToBlackBy(255 - highBright);
    }
}

// --- Public API ---

bool effectIsSoundReactive(uint8_t effectIndex) {
    return effectIndex == EFFECT_BASS_PULSE || effectIndex == EFFECT_SPECTRUM;
}

void runEffect(uint8_t effectIndex, CRGB* leds, uint16_t numLeds) {
    switch (effectIndex) {
        case EFFECT_RAINBOW:    effectRainbow(leds, numLeds);   break;
        case EFFECT_CHASE:      effectChase(leds, numLeds);     break;
        case EFFECT_BASS_PULSE: effectBassPulse(leds, numLeds); break;
        case EFFECT_SPECTRUM:   effectSpectrum(leds, numLeds);  break;
        default:                effectRainbow(leds, numLeds);   break;
    }
}
