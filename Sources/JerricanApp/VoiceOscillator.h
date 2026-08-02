#pragma once

#include <algorithm>
#include <cmath>

#include "FastRandom.h"

// Plain-C++ synthesis unit (no JUCE dependency), so it stays unit-testable
// the same way VoiceModel is. Used both directly and as the per-grain
// generator inside Grain; never touched by the UI thread.
class VoiceOscillator {
public:
    enum class Waveform { Sine, Saw, Noise, Fm };

    explicit VoiceOscillator(Waveform waveform = Waveform::Sine) : waveform_(waveform) {}

    void setWaveform(Waveform waveform) { waveform_ = waveform; }
    void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }

    // Shared with Grain's filter envelope, which needs the same
    // pitch->Hz mapping to compute its fundamental-relative cutoff.
    static double frequencyFromNormalizedPitch(float normalizedPitch) {
        const double t = std::max(0.0f, std::min(1.0f, normalizedPitch));
        return minFrequencyHz * std::pow(maxFrequencyHz / minFrequencyHz, t);
    }

    // Resets phase state so a re-triggered grain doesn't inherit a stale
    // waveform's phase relationship.
    void reset() {
        phase_ = 0.0;
        modPhase_ = 0.0;
        noiseFilterState_ = 0.0f;
    }

    // normalizedPitch is expected in [0, 1]; returns a sample in [-1, 1].
    float renderSample(float normalizedPitch) {
        const double frequency = frequencyFromNormalizedPitch(normalizedPitch);
        const double phaseIncrement = frequency / sampleRate_;

        float sample = 0.0f;
        switch (waveform_) {
            case Waveform::Sine:
                sample = static_cast<float>(std::sin(phase_ * twoPi));
                break;
            case Waveform::Saw:
                sample = static_cast<float>(2.0 * phase_ - 1.0);
                break;
            case Waveform::Noise: {
                // Band-limited ("colored") noise: a one-pole lowpass over
                // white noise, cutoff tied to the grain's pitch. Without
                // this, Noise is deaf to Pitch Range and just reads as flat
                // broadband hiss; filtering makes it a pitched texture
                // instead.
                const float cutoffHz = static_cast<float>(frequency) * noiseBrightnessMultiplier;
                const float filterCoefficient =
                    1.0f - std::exp(-static_cast<float>(twoPi) * cutoffHz / static_cast<float>(sampleRate_));
                const float whiteNoise = noise_.nextFloatRange(-1.0f, 1.0f);
                noiseFilterState_ += (whiteNoise - noiseFilterState_) * filterCoefficient;
                sample = noiseFilterState_;
                break;
            }
            case Waveform::Fm: {
                const double modulator = std::sin(modPhase_ * twoPi);
                sample = static_cast<float>(std::sin(phase_ * twoPi + fmModulationIndex * modulator));
                modPhase_ = wrapPhase(modPhase_ + phaseIncrement * fmModulatorRatio);
                break;
            }
        }

        phase_ = wrapPhase(phase_ + phaseIncrement);
        return sample;
    }

private:
    static constexpr double twoPi = 6.283185307179586;
    static constexpr double minFrequencyHz = 55.0;   // A1
    static constexpr double maxFrequencyHz = 880.0;  // A5
    static constexpr double fmModulatorRatio = 2.0;
    static constexpr double fmModulationIndex = 1.8;
    static constexpr float noiseBrightnessMultiplier = 4.0f;

    static double wrapPhase(double phase) { return phase - std::floor(phase); }

    Waveform waveform_;
    double sampleRate_ = 44100.0;
    double phase_ = 0.0;
    double modPhase_ = 0.0;
    float noiseFilterState_ = 0.0f;
    FastRandom noise_{0x1234567u};
};
