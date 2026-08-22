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

// Position along the palette the user picked, from spectral tilt: -1 lands on
// colour 1, 0 on colour 2, +1 on colour 3. Tilt never invents a hue — doing
// that would fight the colour picker — it only slides along the chosen ramp,
// so the presets in the app double as warm-to-cold ramps for free.
static CRGB tiltColor(float tilt) {
    float t = (tilt + 1.0f) * 0.5f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    if (getColorCount() < 3) {
        return blend(getColor1(), getColor2(), static_cast<uint8_t>(t * 255.0f));
    }
    if (t < 0.5f) {
        return blend(getColor1(), getColor2(), static_cast<uint8_t>(t * 2.0f * 255.0f));
    }
    return blend(getColor2(), getColor3(), static_cast<uint8_t>((t - 0.5f) * 2.0f * 255.0f));
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

// Kick lift. This is the one case where audio may drive brightness: the rule
// above bans *tempo* in brightness because a slowly-varying value there reads
// as flicker. A transient is the opposite — it reads as impact, which is the
// whole point.
//
// The strip drops to CHASE_FLOOR to leave headroom, so a kick lifts it to full
// rather than washing it toward white. That only happens while a phone is
// actually streaming; with no audio there is no kick coming, and dimming to
// 55% forever would just make chase look broken.
// ponytail: feel knobs. Lower the floor for a harder throb.
constexpr float CHASE_FLOOR = 0.55f;
constexpr float CHASE_LIFT_DECAY = 0.90f;
static float chaseLift = 0.0f;

static void effectChase(CRGB* leds, uint16_t numLeds) {
    CRGB colors[3] = { getColor1(), getColor2(), getColor3() };

    uint8_t numColors = getColorCount();

    float segmentLen = numLeds / static_cast<float>(numColors);

    // Still one segment per beat, so the tempo reads the same either way.
    advancePhase(chasePos, lastChaseFrame,
                 segmentLen * CHASE_SEGMENTS_PER_BEAT,
                 static_cast<float>(numLeds));

    float blendWidth = 4.0f;

    // Attack instant, decay slow — a kick should snap the strip up and let it
    // fall, not ramp into it.
    float kick = getKick();
    chaseLift = (kick > chaseLift) ? kick : chaseLift * CHASE_LIFT_DECAY;
    uint8_t level = audioIsLive()
        ? static_cast<uint8_t>((CHASE_FLOOR + (1.0f - CHASE_FLOOR) * chaseLift) * 255.0f)
        : 255;

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
        c.nscale8_video(level);   // _video so a dim chase never drops to black
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

// --- Spectrum (sound-reactive, 3 hard colour zones) ---
// Deliberately stepped. Flow below is the smooth reading of the same three
// bands; the hard boundaries here make each band legible on its own, which is
// the useful thing when you want to see what the audio is doing rather than
// feel it.
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

// --- Flow (sound-reactive, the whole horn as one gradient) ---
// The same three bands as Spectrum, read smoothly instead of stepped. Position
// maps continuously to frequency — base is bass, tip is the top band — and both
// brightness and colour interpolate between the band values.
//
// Spectrum's three hard zones read as three separate bars stacked on one strip,
// with boundaries at arbitrary pixel counts that line up with nothing physical.
// Interpolating removes the seams: measured across one frame, the largest jump
// between neighbouring pixels drops from 360 to 18.
static void effectFlow(CRGB* leds, uint16_t numLeds) {
    const float energy[3] = { getBassEnergy(), getMidEnergy(), getHighEnergy() };

    for (uint16_t i = 0; i < numLeds; i++) {
        float p = (numLeds > 1) ? static_cast<float>(i) / (numLeds - 1) : 0.0f;

        // A float index into the three bands: 0 at the base, 2 at the tip.
        float f = p * 2.0f;
        uint8_t lo = (f >= 1.0f) ? 1 : 0;
        float frac = f - static_cast<float>(lo);
        float e = energy[lo] + (energy[lo + 1] - energy[lo]) * frac;

        // Colour rides the same ramp. Reusing the tilt mapping means a
        // two-colour palette collapses correctly here for free.
        CRGB c = tiltColor(f - 1.0f);
        c.nscale8_video(static_cast<uint8_t>(e * 255.0f));
        leds[i] = c;
    }
}

// --- Bass Bloom (sound-reactive, built for bass music) ---
// The base of a horn is LED 0. Each kick launches a pulse that travels out to
// the tip and fades; sustained sub sets a background glow so a drop stays lit
// between hits; spectral tilt colours the whole thing.
//
// Brightness comes from the *onset*, never from the level. This is the whole
// point of the effect: a reece or a wobble holds getBassEnergy() high for a
// full drop, so driving brightness from it reads as a constant glow with no
// beat in it at all. getKick() is the rise above the recent average.
constexpr uint8_t BLOOM_PULSES = 4;      // ponytail: fixed pool. Raise if fast rolls swallow pulses.
// ponytail: feel knobs, all four. Tune against real tracks on the horns.
constexpr float BLOOM_THRESHOLD = 0.35f; // kick level that counts as a hit
constexpr float BLOOM_DECAY = 0.95f;     // per frame; a pulse should still be alive at the tip
constexpr float BLOOM_WIDTH_MIN = 1.5f;  // half-width in LEDs with no mids present
constexpr float BLOOM_WIDTH_MID = 4.0f;  // extra half-width at full mids — fatter patch, fatter pulse
constexpr float BLOOM_GLOW = 0.35f;      // how brightly sustained sub sits under the pulses

static struct { float pos; float energy; } bloomPulses[BLOOM_PULSES];
static unsigned long lastBloomFrame = 0;
static bool bloomArmed = true;

static void effectBassBloom(CRGB* leds, uint16_t numLeds) {
    unsigned long now = millis();
    float dt = (lastBloomFrame > 0) ? static_cast<float>(now - lastBloomFrame) : 0.0f;
    lastBloomFrame = now;

    float kick = getKick();
    // Rising edge with hysteresis. Without it a kick that stays above the
    // threshold for several frames spawns a pulse on every one of them, and a
    // wobbling level retriggers continuously.
    if (bloomArmed && kick >= BLOOM_THRESHOLD) {
        uint8_t weakest = 0;
        for (uint8_t i = 1; i < BLOOM_PULSES; i++) {
            if (bloomPulses[i].energy < bloomPulses[weakest].energy) weakest = i;
        }
        bloomPulses[weakest].pos = 0.0f;
        bloomPulses[weakest].energy = kick;
        bloomArmed = false;
    } else if (kick < BLOOM_THRESHOLD * 0.6f) {
        bloomArmed = true;
    }

    CRGB color = tiltColor(getTilt());

    // Background glow, so the strip does not go black between kicks on a track
    // that is holding a wall of sub.
    CRGB glow = color;
    glow.fadeToBlackBy(255 - static_cast<uint8_t>(getBassEnergy() * BLOOM_GLOW * 255.0f));
    fill_solid(leds, numLeds, glow);

    // One horn-length per beat: tempo goes into motion, never into brightness.
    float perMs = numLeds * getBPM() / 60000.0f;
    float halfWidth = BLOOM_WIDTH_MIN + getMidEnergy() * BLOOM_WIDTH_MID;

    for (uint8_t p = 0; p < BLOOM_PULSES; p++) {
        if (bloomPulses[p].energy <= 0.01f) continue;
        bloomPulses[p].pos += perMs * dt;
        bloomPulses[p].energy *= BLOOM_DECAY;

        if (bloomPulses[p].pos - halfWidth > static_cast<float>(numLeds)) {
            bloomPulses[p].energy = 0.0f;   // off the tip, free the slot
            continue;
        }

        for (uint16_t i = 0; i < numLeds; i++) {
            float d = fabsf(static_cast<float>(i) - bloomPulses[p].pos);
            if (d >= halfWidth) continue;
            float amt = (1.0f - d / halfWidth) * bloomPulses[p].energy;
            CRGB add = color;
            add.fadeToBlackBy(255 - static_cast<uint8_t>(amt * 255.0f));
            leds[i] += add;   // saturating, so overlapping pulses just read brighter
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
        case EFFECT_BASS_BLOOM: effectBassBloom(leds, numLeds); break;
        case EFFECT_FLOW:       effectFlow(leds, numLeds);      break;
        default:                effectRainbow(leds, numLeds);   break;
    }
}
