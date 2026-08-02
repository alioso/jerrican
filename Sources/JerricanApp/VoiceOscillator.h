#pragma once

#include <cmath>
#include <cstdint>
#include <string>

// Plain-C++ synthesis unit (no JUCE dependency), so it stays unit-testable
// with the same lightweight harness as VoiceModel. Owned solely by the audio
// thread; never touched by the UI thread.
class VoiceOscillator {
public:
    explicit VoiceOscillator(const std::string& instrumentFamily)
        : waveform_(waveformFromInstrument(instrumentFamily)) {}

    void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }

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
            case Waveform::Noise:
                sample = nextNoiseSample();
                break;
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
    enum class Waveform { Sine, Saw, Noise, Fm };

    static constexpr double twoPi = 6.283185307179586;
    static constexpr double minFrequencyHz = 55.0;   // A1
    static constexpr double maxFrequencyHz = 880.0;  // A5
    static constexpr double fmModulatorRatio = 2.0;
    static constexpr double fmModulationIndex = 3.0;

    static Waveform waveformFromInstrument(const std::string& instrument) {
        if (instrument == "Saw") return Waveform::Saw;
        if (instrument == "Noise") return Waveform::Noise;
        if (instrument == "FM") return Waveform::Fm;
        return Waveform::Sine;
    }

    static double frequencyFromNormalizedPitch(float normalizedPitch) {
        const double t = std::max(0.0f, std::min(1.0f, normalizedPitch));
        return minFrequencyHz * std::pow(maxFrequencyHz / minFrequencyHz, t);
    }

    static double wrapPhase(double phase) { return phase - std::floor(phase); }

    float nextNoiseSample() {
        // xorshift32 PRNG: fast, deterministic, no JUCE/stdlib RNG dependency.
        noiseState_ ^= noiseState_ << 13;
        noiseState_ ^= noiseState_ >> 17;
        noiseState_ ^= noiseState_ << 5;
        return static_cast<float>(noiseState_) / static_cast<float>(UINT32_MAX) * 2.0f - 1.0f;
    }

    Waveform waveform_;
    double sampleRate_ = 44100.0;
    double phase_ = 0.0;
    double modPhase_ = 0.0;
    std::uint32_t noiseState_ = 0x9e3779b9u;
};
