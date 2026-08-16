#include "audio.h"
#include "config.h"
#include <arduinoFFT.h>

static double vReal[FFT_SAMPLES];
static double vImag[FFT_SAMPLES];
static ArduinoFFT<double> fft(vReal, vImag, FFT_SAMPLES, SAMPLE_RATE);

static float bassEnergy = 0.0f;
static float midEnergy = 0.0f;
static float highEnergy = 0.0f;

// ponytail: no mic wired to the T-Energy S3 yet. sampleAudio() feeds silence,
// so the FFT below runs on zeros and the sound-reactive effects sit dark until
// hardware lands. Everything downstream is already calibrated — attaching a mic
// means rewriting sampleAudio() and nothing else.
// ponytail: TODO buy a mic — needs a long lead (worn on the body, driver at the
// battery pack). INMP441 on I2S is the likely pick: BCK/WS/DIN pins into
// config.h, 32-bit samples shifted down to 16, drop the PDM mode entirely.

void initAudio() {}

void sampleAudio() {
    for (uint16_t i = 0; i < FFT_SAMPLES; i++) {
        vReal[i] = 0.0;
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
}

float getBassEnergy() { return bassEnergy; }
float getMidEnergy()  { return midEnergy; }
float getHighEnergy() { return highEnergy; }
