#pragma once

#include <algorithm>
#include <cmath>

#include "VoiceOscillator.h"

// A single grain: a short-lived enveloped burst from a VoiceOscillator.
// Lives entirely on the audio thread, re-triggered in place by GrainCloud
// (no allocation). Idle grains (isActive() == false) are silent and safe to
// re-trigger at any time.
class Grain {
public:
    // Ambient: today's symmetric Hann window, unfiltered — smooth, static
    // timbre, right for sustained pads (Drone/Haze). Plucked: fast attack +
    // curved decay plus a bright-to-dark filter sweep — a produced pluck
    // rather than a static tone, for the short-grain "instrument" voices
    // (Pulse/Spark).
    enum class Character { Ambient, Plucked };

    struct StereoSample {
        float left = 0.0f;
        float right = 0.0f;
    };

    // pan is 0 (full left) .. 1 (full right), 0.5 = center.
    void trigger(VoiceOscillator::Waveform waveform, double sampleRate, float pitch,
                 float durationMs, float pan, Character character) {
        oscillator_.setWaveform(waveform);
        oscillator_.setSampleRate(sampleRate);
        oscillator_.reset();
        pitch_ = pitch;
        character_ = character;
        sampleRate_ = sampleRate;
        fundamentalHz_ = VoiceOscillator::frequencyFromNormalizedPitch(pitch);
        filterState_ = 0.0f;

        totalLifeSamples_ = std::max(1, static_cast<int>(durationMs * 0.001 * sampleRate));
        remainingLifeSamples_ = totalLifeSamples_;

        const float clampedPan = std::max(0.0f, std::min(1.0f, pan));
        const float angle = clampedPan * halfPi;
        leftGain_ = std::cos(angle);
        rightGain_ = std::sin(angle);
    }

    bool isActive() const { return remainingLifeSamples_ > 0; }

    StereoSample renderSample() {
        if (remainingLifeSamples_ <= 0) {
            return {};
        }

        const float t = 1.0f - static_cast<float>(remainingLifeSamples_) /
                                    static_cast<float>(totalLifeSamples_);

        float sample = oscillator_.renderSample(pitch_);
        if (character_ == Character::Plucked) {
            sample = applyFilterSweep(sample, t);
        }

        const float envelope =
            character_ == Character::Plucked ? percussiveEnvelope(t) : hannEnvelope(t);
        const float windowed = sample * envelope;
        --remainingLifeSamples_;

        return {windowed * leftGain_, windowed * rightGain_};
    }

private:
    static constexpr float halfPi = 1.5707963267948966f;
    static constexpr float twoPi = 6.283185307179586f;
    static constexpr float attackFraction = 0.15f;
    static constexpr float decayShape = 1.4f;
    static constexpr float filterSweepCurve = 0.5f;
    static constexpr float filterStartMultiple = 6.0f;
    static constexpr float filterEndMultiple = 3.0f;

    static float hannEnvelope(float t) { return 0.5f - 0.5f * std::cos(twoPi * t); }

    static float percussiveEnvelope(float t) {
        if (t < attackFraction) {
            return t / attackFraction;
        }
        const float u = (t - attackFraction) / (1.0f - attackFraction);
        return std::pow(1.0f - u, decayShape);
    }

    float applyFilterSweep(float rawSample, float t) {
        const float sweepProgress = std::pow(t, filterSweepCurve);
        const float cutoffMultiple =
            filterStartMultiple + (filterEndMultiple - filterStartMultiple) * sweepProgress;
        const float cutoffHz = static_cast<float>(fundamentalHz_) * cutoffMultiple;
        const float filterCoefficient =
            1.0f - std::exp(-twoPi * cutoffHz / static_cast<float>(sampleRate_));
        filterState_ += (rawSample - filterState_) * filterCoefficient;
        return filterState_;
    }

    VoiceOscillator oscillator_;
    Character character_ = Character::Ambient;
    float pitch_ = 0.5f;
    double sampleRate_ = 44100.0;
    double fundamentalHz_ = 220.0;
    float filterState_ = 0.0f;
    int totalLifeSamples_ = 1;
    int remainingLifeSamples_ = 0;
    float leftGain_ = 0.7071f;
    float rightGain_ = 0.7071f;
};
