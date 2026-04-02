# Color Presets, Chase Effect & BPM Control — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Streamline to 4 effects (Rainbow, Chase, Bass Pulse, Spectrum) with user-selectable colors via presets, custom color pickers, and BPM-synced animation speed.

**Architecture:** Firmware receives 3 RGB colors and a BPM value over BLE. Effects read these at runtime. Web UI manages presets client-side and writes raw RGB values. No firmware knowledge of presets.

**Tech Stack:** PlatformIO (ESP32-S3), FastLED, NimBLE-Arduino 2.5.0, ArduinoFFT, Web Bluetooth API

**Spec:** `docs/superpowers/specs/2026-04-01-color-presets-chase-bpm-design.md`

---

### Task 1: Update config.h — new effect constants and BLE UUIDs

**Files:**
- Modify: `src/config.h`

- [ ] **Step 1: Replace the effects and BLE sections of config.h**

Replace the entire contents of `src/config.h` with:

```cpp
#pragma once

#include <cstdint>

// LED strip
constexpr uint8_t LED_PIN = 2;          // GPIO2 (D1)
constexpr uint16_t NUM_LEDS = 60;
constexpr uint8_t MAX_BRIGHTNESS = 128; // Cap for power budget (50%)

// PDM Mic (built-in on XIAO ESP32-S3)
constexpr uint8_t PDM_CLK_PIN = 41;
constexpr uint8_t PDM_DATA_PIN = 42;
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr uint16_t FFT_SAMPLES = 2048;

// BLE
constexpr const char* DEVICE_NAME = "Kwynumbra";
constexpr const char* SERVICE_UUID         = "19b10000-e8f2-537e-4f6c-d104768a1214";
constexpr const char* EFFECT_CHAR_UUID     = "19b10001-e8f2-537e-4f6c-d104768a1214";
constexpr const char* BRIGHTNESS_CHAR_UUID = "19b10002-e8f2-537e-4f6c-d104768a1214";
constexpr const char* COLOR1_CHAR_UUID     = "19b10003-e8f2-537e-4f6c-d104768a1214";
constexpr const char* COLOR2_CHAR_UUID     = "19b10004-e8f2-537e-4f6c-d104768a1214";
constexpr const char* COLOR3_CHAR_UUID     = "19b10005-e8f2-537e-4f6c-d104768a1214";
constexpr const char* BPM_CHAR_UUID        = "19b10006-e8f2-537e-4f6c-d104768a1214";

// Effects
constexpr uint8_t NUM_EFFECTS = 4;
constexpr uint8_t EFFECT_RAINBOW    = 0;
constexpr uint8_t EFFECT_CHASE      = 1;
constexpr uint8_t EFFECT_BASS_PULSE = 2;
constexpr uint8_t EFFECT_SPECTRUM   = 3;

// BPM
constexpr uint8_t DEFAULT_BPM = 128;
constexpr uint8_t MIN_BPM = 60;
constexpr uint8_t MAX_BPM = 200;
```

- [ ] **Step 2: Verify it compiles (will fail at link due to removed effects — that's expected)**

Run: `pio run 2>&1 | tail -5`
Expected: Linker errors about missing effect functions (effectFire, etc.) — confirms config changes are picked up.

- [ ] **Step 3: Commit**

```bash
git add src/config.h
git commit -m "feat: update config for 4 effects, add color/BPM BLE UUIDs"
```

---

### Task 2: Add color and BPM BLE characteristics

**Files:**
- Modify: `src/ble_control.h`
- Modify: `src/ble_control.cpp`

- [ ] **Step 1: Update ble_control.h to add new getters**

Replace the entire contents of `src/ble_control.h` with:

```cpp
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
```

- [ ] **Step 2: Update ble_control.cpp with color/BPM state, callbacks, and characteristics**

Replace the entire contents of `src/ble_control.cpp` with:

```cpp
#include "ble_control.h"
#include "config.h"
#include <Arduino.h>
#include <NimBLEDevice.h>

static uint8_t currentEffect = EFFECT_RAINBOW;
static uint8_t currentBrightness = MAX_BRIGHTNESS;
static CRGB color1 = CRGB(155, 89, 182);   // Cyber purple
static CRGB color2 = CRGB(0, 206, 209);    // Cyber cyan
static CRGB color3 = CRGB(255, 105, 180);  // Cyber hot pink
static uint8_t currentBPM = DEFAULT_BPM;

class EffectCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        uint8_t value = pCharacteristic->getValue<uint8_t>();
        if (value < NUM_EFFECTS) {
            currentEffect = value;
        }
    }
};

class BrightnessCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        uint8_t value = pCharacteristic->getValue<uint8_t>();
        currentBrightness = (value < MAX_BRIGHTNESS) ? value : MAX_BRIGHTNESS;
    }
};

class ColorCallbacks : public NimBLECharacteristicCallbacks {
    CRGB* target;
public:
    ColorCallbacks(CRGB* t) : target(t) {}
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        std::string val = pCharacteristic->getValue();
        if (val.length() >= 3) {
            *target = CRGB(
                static_cast<uint8_t>(val[0]),
                static_cast<uint8_t>(val[1]),
                static_cast<uint8_t>(val[2])
            );
        }
    }
};

class BPMCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        uint8_t value = pCharacteristic->getValue<uint8_t>();
        if (value >= MIN_BPM && value <= MAX_BPM) {
            currentBPM = value;
        }
    }
};

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        Serial.println("BLE client connected");
    }
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        Serial.printf("BLE client disconnected (reason=%d), restarting advertising\n", reason);
        NimBLEDevice::getAdvertising()->start();
    }
};

static EffectCallbacks effectCb;
static BrightnessCallbacks brightnessCb;
static ColorCallbacks color1Cb(&color1);
static ColorCallbacks color2Cb(&color2);
static ColorCallbacks color3Cb(&color3);
static BPMCallbacks bpmCb;
static ServerCallbacks serverCb;

void initBLE() {
    NimBLEDevice::init(DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCb);
    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    auto* pEffectChar = pService->createCharacteristic(
        EFFECT_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    pEffectChar->setCallbacks(&effectCb);
    pEffectChar->setValue(currentEffect);

    auto* pBrightnessChar = pService->createCharacteristic(
        BRIGHTNESS_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    pBrightnessChar->setCallbacks(&brightnessCb);
    pBrightnessChar->setValue(currentBrightness);

    auto* pColor1Char = pService->createCharacteristic(
        COLOR1_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    pColor1Char->setCallbacks(&color1Cb);
    uint8_t c1[] = {color1.r, color1.g, color1.b};
    pColor1Char->setValue(c1, 3);

    auto* pColor2Char = pService->createCharacteristic(
        COLOR2_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    pColor2Char->setCallbacks(&color2Cb);
    uint8_t c2[] = {color2.r, color2.g, color2.b};
    pColor2Char->setValue(c2, 3);

    auto* pColor3Char = pService->createCharacteristic(
        COLOR3_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    pColor3Char->setCallbacks(&color3Cb);
    uint8_t c3[] = {color3.r, color3.g, color3.b};
    pColor3Char->setValue(c3, 3);

    auto* pBPMChar = pService->createCharacteristic(
        BPM_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    pBPMChar->setCallbacks(&bpmCb);
    pBPMChar->setValue(currentBPM);

    pServer->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
}

uint8_t getCurrentEffect()     { return currentEffect; }
uint8_t getCurrentBrightness() { return currentBrightness; }
CRGB getColor1() { return color1; }
CRGB getColor2() { return color2; }
CRGB getColor3() { return color3; }
uint8_t getBPM() { return currentBPM; }
```

- [ ] **Step 3: Verify it compiles (link errors from effects.cpp still expected)**

Run: `pio run 2>&1 | tail -5`
Expected: Errors from `effects.cpp` referencing removed constants — that's fine, we fix it next.

- [ ] **Step 4: Commit**

```bash
git add src/ble_control.h src/ble_control.cpp
git commit -m "feat: add BLE characteristics for 3 colors and BPM"
```

---

### Task 3: Rewrite effects — remove old, add Chase, update remaining

**Files:**
- Modify: `src/effects.cpp`

- [ ] **Step 1: Replace effects.cpp with the 4 new effects**

Replace the entire contents of `src/effects.cpp` with:

```cpp
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
```

- [ ] **Step 2: Build firmware — should compile clean**

Run: `pio run 2>&1 | tail -5`
Expected: `[SUCCESS]` with no errors.

- [ ] **Step 3: Flash to device**

Run: `pio run -t upload 2>&1 | tail -5`
Expected: `[SUCCESS]` — LEDs show rainbow at 128 BPM speed.

- [ ] **Step 4: Commit**

```bash
git add src/effects.cpp
git commit -m "feat: rewrite effects — 4 modes with color/BPM support"
```

---

### Task 4: Rebuild web UI with presets, BPM, and color pickers

**Files:**
- Modify: `web/index.html`

- [ ] **Step 1: Replace web/index.html with the new UI**

Replace the entire contents of `web/index.html` with:

```html
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>Kwynumbra</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
    background: #0a0a0a;
    color: #fff;
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 16px;
    -webkit-user-select: none;
    user-select: none;
  }
  h1 {
    font-size: 0.75em;
    margin-bottom: 10px;
    color: #555;
    text-transform: uppercase;
    letter-spacing: 3px;
  }
  #connect-btn {
    width: 100%;
    max-width: 400px;
    padding: 14px;
    font-size: 1.1em;
    font-weight: 600;
    border: 2px solid #9b59b6;
    background: transparent;
    color: #9b59b6;
    border-radius: 10px;
    cursor: pointer;
    margin-bottom: 16px;
    transition: all 0.2s;
  }
  #connect-btn:active { background: #9b59b6; color: #fff; }
  #connect-btn.connected { background: #9b59b6; color: #fff; }
  #controls { display: none; width: 100%; max-width: 400px; }

  /* Sliders */
  .slider-row {
    margin-bottom: 14px;
  }
  .slider-label {
    display: flex;
    justify-content: space-between;
    margin-bottom: 4px;
    font-size: 0.8em;
    color: #666;
  }
  input[type="range"] {
    -webkit-appearance: none;
    width: 100%;
    height: 36px;
    background: transparent;
  }
  input[type="range"]::-webkit-slider-runnable-track {
    height: 6px;
    background: #333;
    border-radius: 3px;
  }
  input[type="range"]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 28px;
    height: 28px;
    background: #9b59b6;
    border-radius: 50%;
    margin-top: -11px;
  }

  /* BPM stepper */
  .bpm-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 14px;
  }
  .bpm-label { font-size: 0.8em; color: #666; }
  .bpm-controls { display: flex; align-items: center; gap: 12px; }
  .bpm-btn {
    width: 40px;
    height: 40px;
    border: 2px solid #333;
    background: #1a1a1a;
    color: #ccc;
    border-radius: 10px;
    font-size: 1.4em;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
  }
  .bpm-btn:active { background: #333; }
  .bpm-value {
    font-size: 1.5em;
    font-weight: 700;
    min-width: 50px;
    text-align: center;
  }

  /* Color presets */
  .presets-grid {
    display: grid;
    grid-template-columns: 1fr 1fr 1fr 1fr;
    gap: 8px;
    margin-bottom: 10px;
  }
  .preset-btn {
    padding: 8px 4px;
    border: 2px solid #333;
    background: #1a1a1a;
    border-radius: 10px;
    cursor: pointer;
    text-align: center;
    transition: all 0.15s;
  }
  .preset-btn:active { transform: scale(0.95); }
  .preset-btn.active {
    border-color: #9b59b6;
    background: #2a1a3a;
    box-shadow: 0 0 15px rgba(155, 89, 182, 0.3);
  }
  .preset-dots {
    display: flex;
    gap: 3px;
    justify-content: center;
    margin-bottom: 4px;
  }
  .preset-dot {
    width: 12px;
    height: 12px;
    border-radius: 50%;
  }
  .preset-label {
    font-size: 0.7em;
    color: #999;
    font-weight: 600;
  }

  /* Custom color pickers */
  .custom-colors {
    display: none;
    justify-content: center;
    gap: 20px;
    margin-bottom: 14px;
    padding: 10px 0;
  }
  .custom-colors.visible { display: flex; }
  .color-pick {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
  }
  .color-pick input[type="color"] {
    -webkit-appearance: none;
    width: 48px;
    height: 48px;
    border: 2px solid #444;
    border-radius: 50%;
    background: none;
    cursor: pointer;
    padding: 0;
  }
  .color-pick input[type="color"]::-webkit-color-swatch-wrapper { padding: 2px; }
  .color-pick input[type="color"]::-webkit-color-swatch { border-radius: 50%; border: none; }
  .color-pick label { font-size: 0.65em; color: #666; }

  /* Effects grid */
  .effects-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
  }
  .effect-btn {
    padding: 18px 10px;
    font-size: 1em;
    font-weight: 600;
    border: 2px solid #333;
    background: #1a1a1a;
    color: #ccc;
    border-radius: 10px;
    cursor: pointer;
    transition: all 0.15s;
    text-align: center;
  }
  .effect-btn:active { transform: scale(0.95); }
  .effect-btn.active {
    border-color: #9b59b6;
    background: #2a1a3a;
    color: #fff;
    box-shadow: 0 0 20px rgba(155, 89, 182, 0.3);
  }
  .effect-btn .emoji { font-size: 1.3em; display: block; margin-bottom: 2px; }
  #status { margin-top: 12px; font-size: 0.75em; color: #555; }
</style>
</head>
<body>

<h1>Kwynumbra</h1>
<button id="connect-btn">Connect to Kwynumbra</button>

<div id="controls">
  <!-- Brightness -->
  <div class="slider-row">
    <div class="slider-label">
      <span>Brightness</span>
      <span id="brightness-val">128</span>
    </div>
    <input type="range" id="brightness" min="0" max="128" value="128">
  </div>

  <!-- BPM -->
  <div class="bpm-row">
    <span class="bpm-label">BPM</span>
    <div class="bpm-controls">
      <button class="bpm-btn" id="bpm-down">&minus;</button>
      <span class="bpm-value" id="bpm-val">128</span>
      <button class="bpm-btn" id="bpm-up">+</button>
    </div>
  </div>

  <!-- Color Presets -->
  <div class="presets-grid">
    <button class="preset-btn" data-preset="trans">
      <div class="preset-dots">
        <span class="preset-dot" style="background:#F5A9B8"></span>
        <span class="preset-dot" style="background:#5BCEFA"></span>
        <span class="preset-dot" style="background:#FFFFFF"></span>
      </div>
      <span class="preset-label">Trans</span>
    </button>
    <button class="preset-btn active" data-preset="cyber">
      <div class="preset-dots">
        <span class="preset-dot" style="background:#9B59B6"></span>
        <span class="preset-dot" style="background:#00CED1"></span>
        <span class="preset-dot" style="background:#FF69B4"></span>
      </div>
      <span class="preset-label">Cyber</span>
    </button>
    <button class="preset-btn" data-preset="fire">
      <div class="preset-dots">
        <span class="preset-dot" style="background:#FF0000"></span>
        <span class="preset-dot" style="background:#FF8C00"></span>
        <span class="preset-dot" style="background:#FFD700"></span>
      </div>
      <span class="preset-label">Fire</span>
    </button>
    <button class="preset-btn" data-preset="custom">
      <div class="preset-dots">
        <span class="preset-dot" style="background:#888;border:1px solid #555"></span>
        <span class="preset-dot" style="background:#888;border:1px solid #555"></span>
        <span class="preset-dot" style="background:#888;border:1px solid #555"></span>
      </div>
      <span class="preset-label">Custom</span>
    </button>
  </div>

  <!-- Custom Color Pickers (hidden until Custom selected) -->
  <div class="custom-colors" id="custom-colors">
    <div class="color-pick">
      <input type="color" id="color1" value="#9B59B6">
      <label>Color 1</label>
    </div>
    <div class="color-pick">
      <input type="color" id="color2" value="#00CED1">
      <label>Color 2</label>
    </div>
    <div class="color-pick">
      <input type="color" id="color3" value="#FF69B4">
      <label>Color 3</label>
    </div>
  </div>

  <!-- Effects -->
  <div class="effects-grid">
    <button class="effect-btn active" data-effect="0">
      <span class="emoji">&#x1F308;</span>Rainbow
    </button>
    <button class="effect-btn" data-effect="1">
      <span class="emoji">&#x1F3C1;</span>Chase
    </button>
    <button class="effect-btn" data-effect="2">
      <span class="emoji">&#x1F941;</span>Bass Pulse
    </button>
    <button class="effect-btn" data-effect="3">
      <span class="emoji">&#x1F3B5;</span>Spectrum
    </button>
  </div>
</div>

<div id="status"></div>

<script>
const SERVICE_UUID         = '19b10000-e8f2-537e-4f6c-d104768a1214';
const EFFECT_CHAR_UUID     = '19b10001-e8f2-537e-4f6c-d104768a1214';
const BRIGHTNESS_CHAR_UUID = '19b10002-e8f2-537e-4f6c-d104768a1214';
const COLOR1_CHAR_UUID     = '19b10003-e8f2-537e-4f6c-d104768a1214';
const COLOR2_CHAR_UUID     = '19b10004-e8f2-537e-4f6c-d104768a1214';
const COLOR3_CHAR_UUID     = '19b10005-e8f2-537e-4f6c-d104768a1214';
const BPM_CHAR_UUID        = '19b10006-e8f2-537e-4f6c-d104768a1214';

const PRESETS = {
  trans:  ['#F5A9B8', '#5BCEFA', '#FFFFFF'],
  cyber:  ['#9B59B6', '#00CED1', '#FF69B4'],
  fire:   ['#FF0000', '#FF8C00', '#FFD700'],
};

let effectChar = null;
let brightnessChar = null;
let color1Char = null;
let color2Char = null;
let color3Char = null;
let bpmChar = null;

const connectBtn = document.getElementById('connect-btn');
const controls = document.getElementById('controls');
const brightnessSlider = document.getElementById('brightness');
const brightnessVal = document.getElementById('brightness-val');
const bpmVal = document.getElementById('bpm-val');
const bpmDown = document.getElementById('bpm-down');
const bpmUp = document.getElementById('bpm-up');
const customColors = document.getElementById('custom-colors');
const colorInputs = [
  document.getElementById('color1'),
  document.getElementById('color2'),
  document.getElementById('color3'),
];
const status = document.getElementById('status');
const effectBtns = document.querySelectorAll('.effect-btn');
const presetBtns = document.querySelectorAll('.preset-btn');

let currentBPM = 128;

function setStatus(msg) { status.textContent = msg; }

function hexToRgb(hex) {
  const r = parseInt(hex.substr(1, 2), 16);
  const g = parseInt(hex.substr(3, 2), 16);
  const b = parseInt(hex.substr(5, 2), 16);
  return [r, g, b];
}

function rgbToHex(r, g, b) {
  return '#' + [r, g, b].map(v => v.toString(16).padStart(2, '0')).join('');
}

async function writeColor(char, hex) {
  if (!char) return;
  try {
    const [r, g, b] = hexToRgb(hex);
    await char.writeValue(new Uint8Array([r, g, b]));
  } catch (err) {
    setStatus('Write error: ' + err.message);
  }
}

async function writeBPM(val) {
  if (!bpmChar) return;
  try {
    await bpmChar.writeValue(new Uint8Array([val]));
  } catch (err) {
    setStatus('Write error: ' + err.message);
  }
}

// Connect
connectBtn.addEventListener('click', async () => {
  try {
    setStatus('Scanning...');
    const device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }],
    });

    device.addEventListener('gattserverdisconnected', () => {
      connectBtn.textContent = 'Reconnect';
      connectBtn.classList.remove('connected');
      controls.style.display = 'none';
      setStatus('Disconnected');
    });

    setStatus('Connecting...');
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(SERVICE_UUID);

    effectChar = await service.getCharacteristic(EFFECT_CHAR_UUID);
    brightnessChar = await service.getCharacteristic(BRIGHTNESS_CHAR_UUID);
    color1Char = await service.getCharacteristic(COLOR1_CHAR_UUID);
    color2Char = await service.getCharacteristic(COLOR2_CHAR_UUID);
    color3Char = await service.getCharacteristic(COLOR3_CHAR_UUID);
    bpmChar = await service.getCharacteristic(BPM_CHAR_UUID);

    // Sync UI from device state
    const effectVal = (await effectChar.readValue()).getUint8(0);
    const brightVal = (await brightnessChar.readValue()).getUint8(0);
    const bpmVal2 = (await bpmChar.readValue()).getUint8(0);

    const c1 = await color1Char.readValue();
    const c2 = await color2Char.readValue();
    const c3 = await color3Char.readValue();

    effectBtns.forEach(btn => {
      btn.classList.toggle('active', parseInt(btn.dataset.effect) === effectVal);
    });
    brightnessSlider.value = brightVal;
    brightnessVal.textContent = brightVal;
    currentBPM = bpmVal2;
    bpmVal.textContent = currentBPM;

    colorInputs[0].value = rgbToHex(c1.getUint8(0), c1.getUint8(1), c1.getUint8(2));
    colorInputs[1].value = rgbToHex(c2.getUint8(0), c2.getUint8(1), c2.getUint8(2));
    colorInputs[2].value = rgbToHex(c3.getUint8(0), c3.getUint8(1), c3.getUint8(2));

    connectBtn.textContent = 'Connected';
    connectBtn.classList.add('connected');
    controls.style.display = 'block';
    setStatus('');
  } catch (err) {
    setStatus('Error: ' + err.message);
  }
});

// Effects
effectBtns.forEach(btn => {
  btn.addEventListener('click', async () => {
    if (!effectChar) return;
    const idx = parseInt(btn.dataset.effect);
    try {
      await effectChar.writeValue(new Uint8Array([idx]));
      effectBtns.forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
    } catch (err) {
      setStatus('Write error: ' + err.message);
    }
  });
});

// Brightness
brightnessSlider.addEventListener('input', async () => {
  const val = parseInt(brightnessSlider.value);
  brightnessVal.textContent = val;
  if (!brightnessChar) return;
  try {
    await brightnessChar.writeValue(new Uint8Array([val]));
  } catch (err) {
    setStatus('Write error: ' + err.message);
  }
});

// BPM
bpmDown.addEventListener('click', () => {
  if (currentBPM > 60) {
    currentBPM--;
    bpmVal.textContent = currentBPM;
    writeBPM(currentBPM);
  }
});
bpmUp.addEventListener('click', () => {
  if (currentBPM < 200) {
    currentBPM++;
    bpmVal.textContent = currentBPM;
    writeBPM(currentBPM);
  }
});

// Presets
presetBtns.forEach(btn => {
  btn.addEventListener('click', async () => {
    presetBtns.forEach(b => b.classList.remove('active'));
    btn.classList.add('active');

    const preset = btn.dataset.preset;

    if (preset === 'custom') {
      customColors.classList.add('visible');
      return;
    }

    customColors.classList.remove('visible');
    const colors = PRESETS[preset];
    if (!colors) return;

    colorInputs[0].value = colors[0];
    colorInputs[1].value = colors[1];
    colorInputs[2].value = colors[2];

    await writeColor(color1Char, colors[0]);
    await writeColor(color2Char, colors[1]);
    await writeColor(color3Char, colors[2]);
  });
});

// Custom color pickers
const colorChars = () => [color1Char, color2Char, color3Char];
colorInputs.forEach((input, i) => {
  input.addEventListener('input', () => {
    writeColor(colorChars()[i], input.value);
  });
});
</script>

</body>
</html>
```

- [ ] **Step 2: Commit**

```bash
git add web/index.html
git commit -m "feat: rebuild web UI with presets, custom colors, BPM stepper, 4 effects"
```

---

### Task 5: Build, flash, and verify

**Files:** None (verification only)

- [ ] **Step 1: Build firmware**

Run: `pio run 2>&1 | tail -5`
Expected: `[SUCCESS]`

- [ ] **Step 2: Flash firmware**

Run: `pio run -t upload 2>&1 | tail -5`
Expected: `[SUCCESS]` — LEDs show rainbow cycling at 128 BPM

- [ ] **Step 3: Manual verification checklist**

Open `web/index.html` in Bluefy and verify:

1. Tap Connect → finds "Kwynumbra", connects, UI shows controls
2. Brightness slider → LEDs dim/brighten
3. Tap Chase → 3-color segments race along strip
4. Tap Trans preset → colors change to pink/blue/white
5. BPM +/- → Chase and Rainbow speed changes
6. Tap Custom → 3 color picker circles appear
7. Pick custom colors → strip updates immediately
8. Tap Cyber → pickers collapse, colors revert to purple/cyan/pink
9. Tap Bass Pulse → strip pulses with selected colors when music plays
10. Tap Spectrum → 3 zones use selected colors
11. Walk away → BLE stays connected
12. Disconnect → tap Reconnect → works

- [ ] **Step 4: Commit any fixes, push to remote**

```bash
git push origin main
```
