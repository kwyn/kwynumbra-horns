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

// One callback for every uint8_t knob.
// ponytail: clamps out-of-range writes instead of ignoring them — replaces three
// near-identical classes that disagreed on whether to clamp or reject.
class ByteCallbacks : public NimBLECharacteristicCallbacks {
    uint8_t* target;
    uint8_t lo, hi;
public:
    ByteCallbacks(uint8_t* t, uint8_t lo, uint8_t hi) : target(t), lo(lo), hi(hi) {}
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        uint8_t value = pCharacteristic->getValue<uint8_t>();
        *target = (value < lo) ? lo : (value > hi) ? hi : value;
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

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        Serial.println("BLE client connected");
    }
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        Serial.printf("BLE client disconnected (reason=%d), restarting advertising\n", reason);
        NimBLEDevice::getAdvertising()->start();
    }
};

static ByteCallbacks effectCb(&currentEffect, 0, NUM_EFFECTS - 1);
static ByteCallbacks brightnessCb(&currentBrightness, 0, MAX_BRIGHTNESS);
static ByteCallbacks bpmCb(&currentBPM, MIN_BPM, MAX_BPM);
static ColorCallbacks color1Cb(&color1);
static ColorCallbacks color2Cb(&color2);
static ColorCallbacks color3Cb(&color3);
static ServerCallbacks serverCb;
static NimBLECharacteristic* batteryChar = nullptr;

static void addChar(NimBLEService* svc, const char* uuid,
                    NimBLECharacteristicCallbacks* cb,
                    const uint8_t* initial, size_t len) {
    auto* c = svc->createCharacteristic(
        uuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    c->setCallbacks(cb);
    c->setValue(initial, len);
}

void initBLE() {
    NimBLEDevice::init(DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCb);
    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    uint8_t c1[] = {color1.r, color1.g, color1.b};
    uint8_t c2[] = {color2.r, color2.g, color2.b};
    uint8_t c3[] = {color3.r, color3.g, color3.b};

    addChar(pService, EFFECT_CHAR_UUID,     &effectCb,     &currentEffect,     1);
    addChar(pService, BRIGHTNESS_CHAR_UUID, &brightnessCb, &currentBrightness, 1);
    addChar(pService, COLOR1_CHAR_UUID,     &color1Cb,     c1,                 3);
    addChar(pService, COLOR2_CHAR_UUID,     &color2Cb,     c2,                 3);
    addChar(pService, COLOR3_CHAR_UUID,     &color3Cb,     c3,                 3);
    addChar(pService, BPM_CHAR_UUID,        &bpmCb,        &currentBPM,        1);

    // Standard Battery Service rather than another custom UUID — clients that
    // already speak BLE get the gauge for free.
    NimBLEService* pBattery = pServer->createService(NimBLEUUID((uint16_t)0x180F));
    batteryChar = pBattery->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A19),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    pServer->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
}

void setBatteryLevel(uint8_t percent) {
    if (!batteryChar) return;
    batteryChar->setValue(&percent, 1);
    batteryChar->notify();
}

uint8_t getCurrentEffect()     { return currentEffect; }
uint8_t getCurrentBrightness() { return currentBrightness; }
// Stored raw so the characteristic reads back exactly what the app wrote — the
// picker would otherwise drift darker on every reconnect. Correction happens
// here, on the way to the LEDs.
//
// applyGamma_video maps 0 to 0, so an all-black colour 3 still reads as the
// two-colour-chase signal.
CRGB getColor1() { return applyGamma_video(color1, COLOR_GAMMA); }
CRGB getColor2() { return applyGamma_video(color2, COLOR_GAMMA); }
CRGB getColor3() { return applyGamma_video(color3, COLOR_GAMMA); }
uint8_t getBPM() { return currentBPM; }
