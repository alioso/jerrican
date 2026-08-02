#include <cassert>
#include <iostream>

#include "VoiceOscillator.h"

namespace {

void checkStaysInRange(VoiceOscillator::Waveform waveform) {
    VoiceOscillator oscillator(waveform);
    oscillator.setSampleRate(44100.0);

    for (int i = 0; i < 44100; ++i) {
        const float sample = oscillator.renderSample(0.5f);
        assert(sample >= -1.0f && sample <= 1.0f);
    }
}

}  // namespace

int main() {
    checkStaysInRange(VoiceOscillator::Waveform::Sine);
    checkStaysInRange(VoiceOscillator::Waveform::Saw);
    checkStaysInRange(VoiceOscillator::Waveform::Noise);
    checkStaysInRange(VoiceOscillator::Waveform::Fm);

    VoiceOscillator low(VoiceOscillator::Waveform::Sine);
    VoiceOscillator high(VoiceOscillator::Waveform::Sine);
    low.setSampleRate(44100.0);
    high.setSampleRate(44100.0);

    bool differs = false;
    for (int i = 0; i < 64; ++i) {
        if (low.renderSample(0.0f) != high.renderSample(1.0f)) {
            differs = true;
        }
    }
    assert(differs);

    VoiceOscillator reusable(VoiceOscillator::Waveform::Saw);
    reusable.setSampleRate(44100.0);
    reusable.renderSample(0.5f);
    reusable.setWaveform(VoiceOscillator::Waveform::Noise);
    reusable.reset();
    assert(reusable.renderSample(0.5f) >= -1.0f && reusable.renderSample(0.5f) <= 1.0f);

    std::cout << "VoiceOscillator tests passed" << std::endl;
    return 0;
}
