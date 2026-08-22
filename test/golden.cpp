// Golden-frame characterization test for src/effects.cpp.
// Runs every effect over a fixed timeline with fixed inputs and dumps each
// frame as hex. Refactors must not change a single byte; ./test/run.sh diffs
// this against test/golden.txt.
//
// ponytail: covers effects.cpp only. audio.cpp needs i2s, ble_control.cpp
// needs NimBLE — neither links natively, so both are stubbed out here.
#include <FastLED.h>
#include <cstdio>
#include "config.h"
#include "effects.h"

// Deterministic clock, replacing FastLED's wall-clock stub (time_stub.cpp is
// excluded from the native build so these win at link time).
static uint32_t g_now = 0;
extern "C" uint32_t millis(void) { return g_now; }
extern "C" uint32_t micros(void) { return g_now * 1000; }
extern "C" void delay(int) {}
extern "C" void delayMicroseconds(int) {}
extern "C" void yield(void) {}

// Stand-ins for ble_control.cpp and audio.cpp. Signatures come from the real
// headers, so changing one breaks this build — which is the point.
static CRGB g_c1(155, 89, 182), g_c2(0, 206, 209), g_c3(255, 105, 180);
static uint8_t g_bpm = DEFAULT_BPM;
static uint8_t g_colorCount = 3;
static float g_bass = 0, g_mid = 0, g_high = 0;
static float g_kick = 0, g_tilt = 0;
static bool g_audioLive = false;

CRGB getColor1() { return g_c1; }
CRGB getColor2() { return g_c2; }
CRGB getColor3() { return g_c3; }
uint8_t getBPM() { return g_bpm; }
uint8_t getColorCount() { return g_colorCount; }
float getKick() { return g_kick; }
float getTilt() { return g_tilt; }
bool audioIsLive() { return g_audioLive; }
float getBassEnergy() { return g_bass; }
float getMidEnergy()  { return g_mid; }
float getHighEnergy() { return g_high; }

int main() {
    CRGB leds[NUM_LEDS];

    for (uint8_t effect = 0; effect < NUM_EFFECTS; effect++) {
        g_now = 0;
        for (int frame = 0; frame < 40; frame++) {
            g_now += 16;                     // ~60fps
            g_bass = (frame % 8) / 8.0f;     // deterministic stand-in for mic input
            g_mid  = (frame % 5) / 5.0f;
            g_high = (frame % 3) / 3.0f;
            // Periodic hits so the kick-driven paths fire, and a full sweep of
            // the tilt ramp so a change to tiltColor() shows up here.
            g_kick = (frame % 7 == 0) ? 0.9f : 0.05f;
            g_tilt = frame / 20.0f - 1.0f;

            for (uint16_t i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::Black;
            runEffect(effect, leds, NUM_LEDS);

            printf("e%u f%02d ", effect, frame);
            for (uint16_t i = 0; i < NUM_LEDS; i++)
                printf("%02x%02x%02x", leds[i].r, leds[i].g, leds[i].b);
            printf("\n");
        }
    }

    // Two-colour chase. Colour 3 is deliberately left at its real value: with a
    // count of 2, chase must never read colors[2], so this block also proves
    // black-as-a-signal is really gone.
    // g_now keeps climbing rather than resetting — effects hold a static
    // last-frame timestamp, and rewinding the clock underflows it.
    g_colorCount = 2;
    for (int frame = 0; frame < 40; frame++) {
        g_now += 16;
        for (uint16_t i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::Black;
        runEffect(EFFECT_CHASE, leds, NUM_LEDS);

        printf("c2 f%02d ", frame);
        for (uint16_t i = 0; i < NUM_LEDS; i++)
            printf("%02x%02x%02x", leds[i].r, leds[i].g, leds[i].b);
        printf("\n");
    }
    // Chase with a phone streaming: the kick lifts the whole strip. Last, so it
    // cannot perturb the blocks above — effects carry static position state
    // across blocks, so an inserted block shifts everything after it.
    g_colorCount = 3;
    g_audioLive = true;
    for (int frame = 0; frame < 40; frame++) {
        g_now += 16;
        g_kick = (frame % 7 == 0) ? 0.9f : 0.05f;
        for (uint16_t i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::Black;
        runEffect(EFFECT_CHASE, leds, NUM_LEDS);
        printf("ck f%02d ", frame);
        for (uint16_t i = 0; i < NUM_LEDS; i++)
            printf("%02x%02x%02x", leds[i].r, leds[i].g, leds[i].b);
        printf("\n");
    }

    // Spectrum with two colours. The whole point of running colour through
    // tiltColor(): the ramp collapses to colour1 -> colour2 and the effect stays
    // usable, where the old fixed-colour-per-zone version read colour 3 whether
    // it was turned on or not.
    g_colorCount = 2;
    for (int frame = 0; frame < 40; frame++) {
        g_now += 16;
        g_bass = (frame % 8) / 8.0f;
        g_mid  = (frame % 5) / 5.0f;
        g_high = (frame % 3) / 3.0f;
        for (uint16_t i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::Black;
        runEffect(EFFECT_SPECTRUM, leds, NUM_LEDS);
        printf("s2 f%02d ", frame);
        for (uint16_t i = 0; i < NUM_LEDS; i++)
            printf("%02x%02x%02x", leds[i].r, leds[i].g, leds[i].b);
        printf("\n");
    }

    return 0;
}
