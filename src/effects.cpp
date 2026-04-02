#include "effects.h"
#include "config.h"
#include "audio.h"

// --- Rainbow ---
static void effectRainbow(CRGB* leds, uint16_t numLeds) {
    static uint8_t hue = 0;
    fill_rainbow(leds, numLeds, hue, 7);
    EVERY_N_MILLISECONDS(20) { hue++; }
}

// --- Fire ---
static uint8_t heat[NUM_LEDS];

static void effectFire(CRGB* leds, uint16_t numLeds) {
    // Cool down
    for (uint16_t i = 0; i < numLeds; i++) {
        heat[i] = qsub8(heat[i], random8(0, 55));
    }
    // Heat drifts up
    for (uint16_t k = numLeds - 1; k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }
    // Random ignition near bottom
    if (random8() < 160) {
        uint8_t y = random8(7);
        heat[y] = qadd8(heat[y], random8(160, 255));
    }
    // Map heat to colors
    for (uint16_t j = 0; j < numLeds; j++) {
        leds[j] = HeatColor(heat[j]);
    }
}

// --- Pulse (breathing white) ---
static void effectPulse(CRGB* leds, uint16_t numLeds) {
    uint8_t brightness = beatsin8(30, 30, 255); // 30 BPM breathing
    fill_solid(leds, numLeds, CRGB::White);
    fadeToBlackBy(leds, numLeds, 255 - brightness);
}

// --- Sparkle ---
static void effectSparkle(CRGB* leds, uint16_t numLeds) {
    fadeToBlackBy(leds, numLeds, 40);
    uint16_t pos = random16(numLeds);
    leds[pos] += CRGB::White;
}

// --- Solid white ---
static void effectSolid(CRGB* leds, uint16_t numLeds) {
    fill_solid(leds, numLeds, CRGB::White);
}

// --- Bass Pulse (sound-reactive) ---
static float smoothedBass = 0.0f;

static void effectBassPulse(CRGB* leds, uint16_t numLeds) {
    float bass = getBassEnergy();
    // Smooth: fast attack, slow decay
    if (bass > smoothedBass) {
        smoothedBass = bass;
    } else {
        smoothedBass *= 0.85f; // decay
    }

    uint8_t brightness = static_cast<uint8_t>(smoothedBass * 255.0f);
    // Purple-to-white color shift with energy
    CRGB color = blend(CRGB::Purple, CRGB::White, brightness);
    fill_solid(leds, numLeds, color);
    fadeToBlackBy(leds, numLeds, 255 - brightness);
}

// --- Spectrum (sound-reactive frequency mapping) ---
static void effectSpectrum(CRGB* leds, uint16_t numLeds) {
    float bass = getBassEnergy();
    float mid = getMidEnergy();
    float high = getHighEnergy();

    uint16_t zone1End = numLeds / 3;        // sub-bass: horn base
    uint16_t zone2End = (numLeds * 2) / 3;  // kick/bass: mid horn

    // Zone 1: sub-bass → deep blue/purple
    uint8_t bassBright = static_cast<uint8_t>(bass * 255.0f);
    for (uint16_t i = 0; i < zone1End; i++) {
        leds[i] = CRGB::Purple;
        leds[i].fadeToBlackBy(255 - bassBright);
    }

    // Zone 2: mids → cyan/green
    uint8_t midBright = static_cast<uint8_t>(mid * 255.0f);
    for (uint16_t i = zone1End; i < zone2End; i++) {
        leds[i] = CRGB::Cyan;
        leds[i].fadeToBlackBy(255 - midBright);
    }

    // Zone 3: highs → hot pink at the tips
    uint8_t highBright = static_cast<uint8_t>(high * 255.0f);
    for (uint16_t i = zone2End; i < numLeds; i++) {
        leds[i] = CRGB::DeepPink;
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
        case EFFECT_FIRE:       effectFire(leds, numLeds);      break;
        case EFFECT_PULSE:      effectPulse(leds, numLeds);     break;
        case EFFECT_SPARKLE:    effectSparkle(leds, numLeds);   break;
        case EFFECT_SOLID:      effectSolid(leds, numLeds);     break;
        case EFFECT_BASS_PULSE: effectBassPulse(leds, numLeds); break;
        case EFFECT_SPECTRUM:   effectSpectrum(leds, numLeds);  break;
        default:                effectRainbow(leds, numLeds);   break;
    }
}
