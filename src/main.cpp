#include <Arduino.h>
#include <FastLED.h>
#include <esp_sleep.h>
#include "config.h"
#include "effects.h"
#include "ble_control.h"
#include "battery.h"

static CRGB leds[NUM_LEDS];

// Reports the pack over BLE and, on an unprotected cell, is the only thing that
// stops it being run flat. Sleeps rather than dims: a dim strip still drains.
static void checkBattery() {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 16; i++) {
        sum += analogReadMilliVolts(BATTERY_ADC_PIN);  // eFuse-calibrated, unlike analogRead
    }
    uint16_t mv = static_cast<uint16_t>(sum / 16 * BATTERY_DIVIDER);

    if (mv > BATTERY_FLOOR_MV && mv < BATTERY_CUTOFF_MV) {
        FastLED.clear(true);
        Serial.printf("battery %umV under %umV cutoff — deep sleep\n", mv, BATTERY_CUTOFF_MV);
        Serial.flush();
        esp_deep_sleep(BATTERY_SLEEP_S * 1000000ULL);  // wakes to recheck, so a charger revives it
    }

    setBatteryLevel(batteryPercent(mv));
}

void setup() {
    Serial.begin(115200);

    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(MAX_BRIGHTNESS);
    FastLED.clear(true);

    initBLE();
    checkBattery();   // before lighting up, so a flat cell never gets loaded

    Serial.println("Kwynumbra ready");
}

void loop() {
    static uint32_t lastBattery = 0;
    if (millis() - lastBattery >= BATTERY_SAMPLE_MS) {
        lastBattery = millis();
        checkBattery();
    }

    uint8_t effect = getCurrentEffect();

    updateAudioState();   // advances the slow spectral tilt; once per frame

    FastLED.setBrightness(getCurrentBrightness());
    runEffect(effect, leds, NUM_LEDS);

    // Target ~60fps. FastLED.delay() shows the strip itself (repeatedly, for
    // temporal dithering), so no separate show() is needed.
    FastLED.delay(16);
}
