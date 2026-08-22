#include "effects.h"
#include "config.h"
#include "ble_control.h"

// Advances `phase` at a BPM-derived rate and wraps it at `wrap`.
//
// Every BPM-synced effect goes through this. Stepping a value once per beat
// instead reads as stutter, not as tempo — the strip sits still for most of the
// beat and then jumps. Accumulating against real elapsed milliseconds keeps
// motion continuous while still carrying the tempo, and it stays smooth if the
// frame rate wobbles.
static void advancePhase(float& phase, unsigned long& lastFrame,
                         float unitsPerBeat, float wrap) {
    unsigned long now = millis();
    if (lastFrame > 0) {
        float perMs = unitsPerBeat * getBPM() / 60000.0f;
        phase = fmodf(phase + perMs * static_cast<float>(now - lastFrame), wrap);
    }
    lastFrame = now;
}

// --- Rainbow (BPM-synced scroll) ---
static float rainbowHue = 0.0f;
static unsigned long lastRainbowFrame = 0;
// Hue step per LED — sets how much rainbow fits on the strip, not its speed.
constexpr uint8_t RAINBOW_DELTA_HUE = 7;
// Travel speed in thirds-of-the-strip per beat, deliberately the same unit as
// CHASE_SEGMENTS_PER_BEAT so the two effects move at a matching pace.
// ponytail: feel knob, tune alongside the chase one.
constexpr float RAINBOW_SEGMENTS_PER_BEAT = 1.0f;

static void effectRainbow(CRGB* leds, uint16_t numLeds) {
    // Phase is tracked in hue rather than in LEDs so it wraps at 256, exactly
    // where the colour wheel wraps. Tracking LEDs and scaling to hue afterwards
    // puts a visible seam wherever numLeds * RAINBOW_DELTA_HUE isn't a multiple
    // of 256 — which for a 60 LED strip it is not.
    float ledsPerBeat = (numLeds / 3.0f) * RAINBOW_SEGMENTS_PER_BEAT;
    advancePhase(rainbowHue, lastRainbowFrame, ledsPerBeat * RAINBOW_DELTA_HUE, 256.0f);
    fill_rainbow(leds, numLeds, static_cast<uint8_t>(rainbowHue), RAINBOW_DELTA_HUE);
}

// --- Chase (BPM-synced scroll) ---
static float chasePos = 0.0f;
static unsigned long lastChaseFrame = 0;
// BPM drives scroll speed, not brightness. One colour segment passes per beat,
// so the movement carries the tempo and the strip holds a steady level — a
// brightness pulse here read as a flicker rather than as a beat.
// ponytail: calibration knob. Raise for a busier chase, halve for a lazier one.
constexpr float CHASE_SEGMENTS_PER_BEAT = 1.0f;

static void effectChase(CRGB* leds, uint16_t numLeds) {
    CRGB colors[3] = { getColor1(), getColor2(), getColor3() };

    uint8_t numColors = getColorCount();

    float segmentLen = numLeds / static_cast<float>(numColors);

    // Still one segment per beat, so the tempo reads the same either way.
    advancePhase(chasePos, lastChaseFrame,
                 segmentLen * CHASE_SEGMENTS_PER_BEAT,
                 static_cast<float>(numLeds));

    float blendWidth = 4.0f;

    for (uint16_t i = 0; i < numLeds; i++) {
        float patternPos = fmodf(static_cast<float>(i) + chasePos, static_cast<float>(numLeds));
        float segFloat = patternPos / segmentLen;
        uint8_t seg = static_cast<uint8_t>(segFloat);
        if (seg >= numColors) seg = numColors - 1;
        float posInSeg = fmodf(patternPos, segmentLen);
        float distToEnd = segmentLen - posInSeg;

        CRGB c;
        if (distToEnd < blendWidth) {
            uint8_t nextSeg = (seg + 1) % numColors;
            uint8_t blendAmt = static_cast<uint8_t>((1.0f - distToEnd / blendWidth) * 255.0f);
            c = blend(colors[seg], colors[nextSeg], blendAmt);
        } else {
            c = colors[seg];
        }
        leds[i] = c;
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
    color.fadeToBlackBy(255 - brightness);
    fill_solid(leds, numLeds, color);
}

// --- Spectrum (sound-reactive, 3 color zones) ---
static void effectSpectrum(CRGB* leds, uint16_t numLeds) {
    const CRGB colors[3] = { getColor1(), getColor2(), getColor3() };
    const float energy[3] = { getBassEnergy(), getMidEnergy(), getHighEnergy() };

    for (uint8_t z = 0; z < 3; z++) {
        uint16_t start = (numLeds * z) / 3;
        uint16_t end   = (numLeds * (z + 1)) / 3;
        uint8_t bright = static_cast<uint8_t>(energy[z] * 255.0f);
        for (uint16_t i = start; i < end; i++) {
            leds[i] = colors[z];
            leds[i].fadeToBlackBy(255 - bright);
        }
    }
}

// --- Public API ---

void runEffect(uint8_t effectIndex, CRGB* leds, uint16_t numLeds) {
    switch (effectIndex) {
        case EFFECT_RAINBOW:    effectRainbow(leds, numLeds);   break;
        case EFFECT_CHASE:      effectChase(leds, numLeds);     break;
        case EFFECT_BASS_PULSE: effectBassPulse(leds, numLeds); break;
        case EFFECT_SPECTRUM:   effectSpectrum(leds, numLeds);  break;
        default:                effectRainbow(leds, numLeds);   break;
    }
}
