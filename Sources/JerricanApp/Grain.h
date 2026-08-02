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
    struct StereoSample {
        float left = 0.0f;
        float right = 0.0f;
    };

    // pan is 0 (full left) .. 1 (full right), 0.5 = center.
    void trigger(VoiceOscillator::Waveform waveform, double sampleRate, float pitch,
                 float durationMs, float pan) {
        oscillator_.setWaveform(waveform);
        oscillator_.setSampleRate(sampleRate);
        oscillator_.reset();
        pitch_ = pitch;

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

        const float raw = oscillator_.renderSample(pitch_);
        const float windowed = raw * hannEnvelope();
        --remainingLifeSamples_;

        return {windowed * leftGain_, windowed * rightGain_};
    }

private:
    static constexpr float halfPi = 1.5707963267948966f;
    static constexpr float twoPi = 6.283185307179586f;

    float hannEnvelope() const {
        const float elapsed = static_cast<float>(totalLifeSamples_ - remainingLifeSamples_);
        const float t = elapsed / static_cast<float>(totalLifeSamples_);
        return 0.5f - 0.5f * std::cos(twoPi * t);
    }

    VoiceOscillator oscillator_;
    float pitch_ = 0.5f;
    int totalLifeSamples_ = 1;
    int remainingLifeSamples_ = 0;
    float leftGain_ = 0.7071f;
    float rightGain_ = 0.7071f;
};
