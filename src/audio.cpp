#include "audio.h"
#include "config.h"
#include <driver/i2s.h>
#include <arduinoFFT.h>

static double vReal[FFT_SAMPLES];
static double vImag[FFT_SAMPLES];
static ArduinoFFT<double> fft(vReal, vImag, FFT_SAMPLES, SAMPLE_RATE);

static float bassEnergy = 0.0f;
static float midEnergy = 0.0f;
static float highEnergy = 0.0f;
static bool beatDetected = false;

// Moving average for beat detection
static constexpr uint8_t AVG_WINDOW = 16;
static float bassHistory[AVG_WINDOW] = {};
static uint8_t historyIdx = 0;
static float bassAvg = 0.0f;

void initAudio() {
    i2s_config_t i2s_config = {};
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
    i2s_config.sample_rate = SAMPLE_RATE;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_config.dma_buf_count = 4;
    i2s_config.dma_buf_len = FFT_SAMPLES / 2;
    i2s_config.use_apll = false;

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, nullptr);

    i2s_pin_config_t pin_config = {};
    pin_config.bck_io_num = I2S_PIN_NO_CHANGE;
    pin_config.ws_io_num = PDM_CLK_PIN;
    pin_config.data_in_num = PDM_DATA_PIN;
    pin_config.data_out_num = I2S_PIN_NO_CHANGE;

    i2s_set_pin(I2S_NUM_0, &pin_config);
}

void sampleAudio() {
    int16_t rawSamples[FFT_SAMPLES];
    size_t bytesRead = 0;
    i2s_read(I2S_NUM_0, rawSamples, sizeof(rawSamples), &bytesRead, portMAX_DELAY);

    uint16_t samplesRead = bytesRead / sizeof(int16_t);
    for (uint16_t i = 0; i < FFT_SAMPLES; i++) {
        if (i < samplesRead) {
            vReal[i] = static_cast<double>(rawSamples[i]);
        } else {
            vReal[i] = 0.0;
        }
        vImag[i] = 0.0;
    }
}

void analyzeFrequencies() {
    fft.windowing(FFT_WIN_TYP_HANN, FFT_FORWARD);
    fft.compute(FFT_FORWARD);
    fft.complexToMagnitude();

    // Frequency resolution: SAMPLE_RATE / FFT_SAMPLES = ~7.8 Hz per bin
    // Bass: bins 3-25 (~23-195 Hz), weight kick range (bins 5-13, ~39-101 Hz)
    double bass = 0.0;
    for (uint16_t i = 3; i <= 25; i++) {
        double weight = (i >= 5 && i <= 13) ? 2.0 : 1.0;
        bass += vReal[i] * weight;
    }

    // Mids: bins 26-80 (~203-625 Hz)
    double mid = 0.0;
    for (uint16_t i = 26; i <= 80; i++) {
        mid += vReal[i];
    }

    // Highs: bins 81-512 (~632-4000 Hz)
    double high = 0.0;
    for (uint16_t i = 81; i <= 512; i++) {
        high += vReal[i];
    }

    // Normalize (empirical scaling factors — tune to your environment)
    constexpr double BASS_SCALE = 500000.0;
    constexpr double MID_SCALE = 300000.0;
    constexpr double HIGH_SCALE = 200000.0;

    bassEnergy = constrain(static_cast<float>(bass / BASS_SCALE), 0.0f, 1.0f);
    midEnergy = constrain(static_cast<float>(mid / MID_SCALE), 0.0f, 1.0f);
    highEnergy = constrain(static_cast<float>(high / HIGH_SCALE), 0.0f, 1.0f);

    // Beat detection: bass exceeds moving average by threshold
    bassHistory[historyIdx] = bassEnergy;
    historyIdx = (historyIdx + 1) % AVG_WINDOW;

    float sum = 0.0f;
    for (uint8_t i = 0; i < AVG_WINDOW; i++) {
        sum += bassHistory[i];
    }
    bassAvg = sum / AVG_WINDOW;

    beatDetected = (bassEnergy > bassAvg * 1.4f) && (bassEnergy > 0.15f);
}

float getBassEnergy() { return bassEnergy; }
float getMidEnergy()  { return midEnergy; }
float getHighEnergy() { return highEnergy; }
bool isBeat()         { return beatDetected; }
