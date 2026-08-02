#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "FastRandom.h"
#include "Grain.h"
#include "VoiceOscillator.h"

// One per voice. Owns a fixed-size pool of grains plus the autonomous
// drift/breathing state that makes the voice keep evolving with zero user
// input. Entirely audio-thread state (constructed once, never touched by
// the UI thread) — the UI only ever passes in the current macro values by
// value each call.
class GrainCloud {
public:
    static constexpr int kMaxGrains = 24;

    // minGrainDurationMs/maxGrainDurationMs are the main lever for a
    // voice's fundamental character: short & sparse reads as pointillistic,
    // long & overlapping reads as a sustained drone. Defaults match the
    // original one-size-fits-all range. `character` picks the grain
    // envelope/filter treatment (see Grain::Character) — fixed per voice,
    // same tier as the duration range.
    explicit GrainCloud(std::uint32_t seed = 0x2545f491u, float minGrainDurationMs = 40.0f,
                         float maxGrainDurationMs = 250.0f,
                         Grain::Character character = Grain::Character::Ambient)
        : random_(seed),
          minGrainDurationMs_(minGrainDurationMs),
          maxGrainDurationMs_(maxGrainDurationMs),
          character_(character) {}

    void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }

    // Immediately jumps the drift/breathing targets to new random values —
    // used by the Randomize button as a generative nudge rather than a
    // hard parameter snap. Scoped to the voice's current pitch range, same
    // as the autonomous drift in updateDrift() — otherwise the new target
    // just gets clamped straight back to the range edge it's already at.
    void rerollDrift(float pitchRangeLow, float pitchRangeHigh) {
        driftTarget_ = random_.nextFloatRange(pitchRangeLow, pitchRangeHigh);
        breathingTarget_ = random_.nextFloatRange(0.3f, 1.0f);
    }

    Grain::StereoSample renderSample(float pitchRangeLow, float pitchRangeHigh, float timbre,
                                      float motion, float complexity, float volume) {
        updateDrift(pitchRangeLow, pitchRangeHigh, motion);
        maybeSpawnGrain(pitchRangeLow, pitchRangeHigh, timbre, complexity);

        float left = 0.0f;
        float right = 0.0f;
        int activeCount = 0;
        for (auto& grain : grains_) {
            if (grain.isActive()) {
                const auto sample = grain.renderSample();
                left += sample.left;
                right += sample.right;
                ++activeCount;
            }
        }

        // Overlapping grains are uncorrelated, so normalize by sqrt(N) to
        // keep perceived loudness roughly stable as density changes, then
        // hard-clamp as a safety net against the (rare) case of many grains
        // peaking in the same sample.
        const float normalization = activeCount > 0 ? 1.0f / std::sqrt(static_cast<float>(activeCount)) : 1.0f;
        const float gain = volume * breathingGain_ * normalization;
        return {std::max(-1.0f, std::min(1.0f, left * gain)),
                std::max(-1.0f, std::min(1.0f, right * gain))};
    }

private:
    void updateDrift(float low, float high, float motion) {
        const float pickProbabilityPerSample =
            (driftPickRateMinHz + motion * driftPickRateSpanHz) / static_cast<float>(sampleRate_);
        if (random_.nextFloat01() < pickProbabilityPerSample) {
            driftTarget_ = random_.nextFloatRange(low, high);
            breathingTarget_ = random_.nextFloatRange(1.0f - motion * 0.7f, 1.0f);
        }

        driftCenter_ += (driftTarget_ - driftCenter_) * smoothingCoefficient;
        breathingGain_ += (breathingTarget_ - breathingGain_) * smoothingCoefficient;
    }

    void maybeSpawnGrain(float low, float high, float timbre, float complexity) {
        const float grainsPerSecond = std::max(0.0f, complexity) * maxGrainsPerSecond;
        const float spawnProbabilityPerSample = grainsPerSecond / static_cast<float>(sampleRate_);
        if (random_.nextFloat01() >= spawnProbabilityPerSample) {
            return;
        }

        for (auto& grain : grains_) {
            if (grain.isActive()) {
                continue;
            }

            const float spread = (high - low) * localSpreadFraction;
            const float lo = std::max(low, std::min(high, driftCenter_ - spread));
            const float hi = std::max(low, std::min(high, driftCenter_ + spread));
            const float pitch = random_.nextFloatRange(std::min(lo, hi), std::max(lo, hi));
            const float durationMs = random_.nextFloatRange(minGrainDurationMs_, maxGrainDurationMs_);
            const float pan = random_.nextFloat01();

            grain.trigger(pickWaveform(timbre), sampleRate_, pitch, durationMs, pan, character_);
            return;
        }
    }

    VoiceOscillator::Waveform pickWaveform(float timbre) {
        const float position = std::max(0.0f, std::min(1.0f, timbre)) * 3.0f;  // 0..3
        const float jittered =
            std::max(0.0f, std::min(3.0f, position + random_.nextFloatRange(-0.75f, 0.75f)));
        switch (static_cast<int>(jittered + 0.5f)) {
            case 0:
                return VoiceOscillator::Waveform::Sine;
            case 1:
                return VoiceOscillator::Waveform::Saw;
            case 2:
                return VoiceOscillator::Waveform::Fm;
            default:
                return VoiceOscillator::Waveform::Noise;
        }
    }

    static constexpr float smoothingCoefficient = 0.0005f;
    static constexpr float driftPickRateMinHz = 0.05f;
    static constexpr float driftPickRateSpanHz = 0.45f;
    static constexpr float localSpreadFraction = 0.15f;
    static constexpr float maxGrainsPerSecond = 40.0f;

    double sampleRate_ = 44100.0;
    std::array<Grain, kMaxGrains> grains_;
    FastRandom random_;
    float minGrainDurationMs_;
    float maxGrainDurationMs_;
    Grain::Character character_;
    float driftCenter_ = 0.5f;
    float driftTarget_ = 0.5f;
    float breathingGain_ = 1.0f;
    float breathingTarget_ = 1.0f;
};
