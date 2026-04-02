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
