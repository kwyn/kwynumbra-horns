#include <Arduino.h>
#include <FastLED.h>
#include "config.h"
#include "effects.h"
#include "ble_control.h"
#include "audio.h"

static CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(115200);

    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(MAX_BRIGHTNESS);
    FastLED.clear(true);

    initBLE();
    initAudio();

    Serial.println("Kwynumbra ready");
}

void loop() {
    uint8_t effect = getCurrentEffect();

    // Only sample audio for sound-reactive effects
    if (effectIsSoundReactive(effect)) {
        sampleAudio();
        analyzeFrequencies();
    }

    FastLED.setBrightness(getCurrentBrightness());
    runEffect(effect, leds, NUM_LEDS);
    FastLED.show();

    // Target ~60fps
    FastLED.delay(16);
}
