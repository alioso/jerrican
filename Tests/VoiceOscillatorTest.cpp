#include <cassert>
#include <iostream>

#include "VoiceOscillator.h"

namespace {

void checkStaysInRange(const std::string& instrument) {
    VoiceOscillator oscillator(instrument);
    oscillator.setSampleRate(44100.0);

    for (int i = 0; i < 44100; ++i) {
        const float sample = oscillator.renderSample(0.5f);
        assert(sample >= -1.0f && sample <= 1.0f);
    }
}

}  // namespace

int main() {
    checkStaysInRange("Sine");
    checkStaysInRange("Saw");
    checkStaysInRange("Noise");
    checkStaysInRange("FM");

    VoiceOscillator low("Sine");
    VoiceOscillator high("Sine");
    low.setSampleRate(44100.0);
    high.setSampleRate(44100.0);

    bool differs = false;
    for (int i = 0; i < 64; ++i) {
        if (low.renderSample(0.0f) != high.renderSample(1.0f)) {
            differs = true;
        }
    }
    assert(differs);

    std::cout << "VoiceOscillator tests passed" << std::endl;
    return 0;
}
