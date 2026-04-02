#pragma once

#include <cstdint>

void initAudio();
void sampleAudio();
void analyzeFrequencies();

// Results from audio analysis (0.0 – 1.0 normalized)
float getBassEnergy();
float getMidEnergy();
float getHighEnergy();
bool isBeat();
