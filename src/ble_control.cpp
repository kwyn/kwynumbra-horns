#include "ble_control.h"
#include "config.h"
#include <NimBLEDevice.h>

static uint8_t currentEffect = EFFECT_RAINBOW;
static uint8_t currentBrightness = MAX_BRIGHTNESS;

static NimBLECharacteristic* pEffectChar = nullptr;
static NimBLECharacteristic* pBrightnessChar = nullptr;

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

static EffectCallbacks effectCb;
static BrightnessCallbacks brightnessCb;

void initBLE() {
    NimBLEDevice::init(DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Max range for Burning Man

    NimBLEServer* pServer = NimBLEDevice::createServer();
    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    pEffectChar = pService->createCharacteristic(
        EFFECT_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    pEffectChar->setCallbacks(&effectCb);
    pEffectChar->setValue(currentEffect);

    pBrightnessChar = pService->createCharacteristic(
        BRIGHTNESS_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    pBrightnessChar->setCallbacks(&brightnessCb);
    pBrightnessChar->setValue(currentBrightness);

    pServer->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
}

uint8_t getCurrentEffect()     { return currentEffect; }
uint8_t getCurrentBrightness() { return currentBrightness; }
