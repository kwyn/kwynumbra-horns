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
