#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "FastRandom.h"
#include "Grain.h"
#include "HarmonicScale.h"
#include "VoiceOscillator.h"

// One per voice. Owns a fixed-size pool of grains plus the autonomous
// drift/breathing state that makes the voice keep evolving with zero user
// input. Entirely audio-thread state (constructed once, never touched by
// the UI thread) — the UI only ever passes in the current macro values by
// value each call.
//
// Drone/Spark/Haze use renderSample() exactly as before — continuous
// stochastic grain spawning driven by Motion (pitch drift retarget rate)
// and Complexity (grain density). Bass instead uses spawnGrainNow() +
// renderActiveGrainsCorrelated(), called directly by
// JerricanAudioProcessor's metered scheduler (see BassGroovePattern.h) —
// its rhythm comes entirely
// from the beat grid now, with zero leftover stochastic spawning texture.
class GrainCloud {
public:
    static constexpr int kMaxGrains = 24;

    // minGrainDurationMs/maxGrainDurationMs are the main lever for a
    // voice's fundamental character: short & sparse reads as pointillistic,
    // long & overlapping reads as a sustained drone. For Bass specifically,
    // these same two values double as Sustain's 0/1 endpoints (see
    // spawnGrainNow) rather than being fixed constants. `character` picks
    // the grain envelope/filter treatment (see Grain::Character) — fixed
    // per voice, same tier as the duration range.
    explicit GrainCloud(std::uint32_t seed = 0x2545f491u, float minGrainDurationMs = 40.0f,
                         float maxGrainDurationMs = 250.0f,
                         Grain::Character character = Grain::Character::Ambient)
        : random_(seed),
          minGrainDurationMs_(minGrainDurationMs),
          maxGrainDurationMs_(maxGrainDurationMs),
          character_(character) {}

    void setSampleRate(double sampleRate) {
        sampleRate_ = sampleRate;
        correlatedAttackCoefficient_ =
            1.0f - std::exp(-1.0f / (kCorrelatedAttackSeconds * static_cast<float>(sampleRate)));
        correlatedReleaseCoefficient_ =
            1.0f - std::exp(-1.0f / (kCorrelatedReleaseSeconds * static_cast<float>(sampleRate)));
    }

    // Immediately jumps the drift/breathing targets to new random values —
    // used by the Randomize button as a generative nudge rather than a
    // hard parameter snap. Scoped to the voice's current pitch range, same
    // as the autonomous drift in updateDrift() — otherwise the new target
    // just gets clamped straight back to the range edge it's already at.
    // Only meaningful for the Drone/Spark/Haze (renderSample) path — Bass
    // doesn't use driftCenter_/breathingGain_ at all.
    void rerollDrift(float pitchRangeLow, float pitchRangeHigh) {
        driftTarget_ = random_.nextFloatRange(pitchRangeLow, pitchRangeHigh);
        breathingTarget_ = random_.nextFloatRange(0.3f, 1.0f);
    }

    // Unchanged path — Drone/Spark/Haze. motion drives pitch-drift retarget
    // rate, complexity drives continuous stochastic grain spawn rate.
    Grain::StereoSample renderSample(float pitchRangeLow, float pitchRangeHigh, float timbre,
                                      float motion, float complexity, float volume,
                                      float dissonance = 1.0f, int rootSemitoneOffset = 0) {
        updateDrift(pitchRangeLow, pitchRangeHigh, motion);
        maybeSpawnGrain(pitchRangeLow, pitchRangeHigh, timbre, complexity, dissonance,
                        rootSemitoneOffset);
        return renderActiveGrains(volume);
    }

    // Bass-only: places a grain immediately, bypassing the continuous
    // Bernoulli-trial spawn probability entirely — called only when the
    // metered scheduler (BassGroovePattern) fires a trigger, not every
    // sample. Wander controls how far the pitch strays from the pitch
    // range's midpoint (0 = exactly the midpoint every time, a true pedal
    // tone); Sustain interpolates between minGrainDurationMs_ (0, short/
    // punchy) and maxGrainDurationMs_ (1, long/legato); Timbre drives the
    // waveshaper (saturation amount) Grain's triggerBlended applies to its
    // fixed core tone, rather than picking one discrete waveform. Pan is
    // kept close to center (real bass sits
    // centered in a mix), unlike the fully-random pan the stochastic path
    // uses for the other voices.
    void spawnGrainNow(float pitchRangeLow, float pitchRangeHigh, float timbre, float wander,
                       float sustain, float dissonance, int rootSemitoneOffset) {
        for (auto& grain : grains_) {
            if (grain.isActive()) {
                continue;
            }

            const float clampedWander = std::max(0.0f, std::min(1.0f, wander));
            const float anchor = (pitchRangeLow + pitchRangeHigh) * 0.5f;
            const float spread = (pitchRangeHigh - pitchRangeLow) * clampedWander * kWanderSpreadFraction;
            const float rawPitch =
                spread > 0.0f
                    ? std::max(pitchRangeLow, std::min(pitchRangeHigh,
                                                       anchor + random_.nextFloatRange(-spread, spread)))
                    : anchor;

            // Same Dissonance blend as the stochastic path: 0 = fully
            // quantized to this voice's (rooted) consonant scale, 1 = fully
            // free/continuous.
            const float quantizedPitch = HarmonicScale::quantize(rawPitch, rootSemitoneOffset);
            const float pitch = quantizedPitch + (rawPitch - quantizedPitch) * dissonance;

            const float clampedSustain = std::max(0.0f, std::min(1.0f, sustain));
            const float centerDurationMs =
                minGrainDurationMs_ + (maxGrainDurationMs_ - minGrainDurationMs_) * clampedSustain;
            const float jitterRangeMs = (maxGrainDurationMs_ - minGrainDurationMs_) * 0.1f;
            const float durationMs =
                std::max(10.0f, centerDurationMs + random_.nextFloatRange(-jitterRangeMs, jitterRangeMs));

            const float pan = 0.5f + random_.nextFloatRange(-0.2f, 0.2f);

            grain.triggerBlended(sampleRate_, pitch, durationMs, pan, timbre, character_);
            return;
        }
    }

    // Sums every currently-active grain into this sample's stereo output —
    // called every sample by both renderSample() (after its own spawn
    // decision) and, for Bass, directly by JerricanAudioProcessor once per
    // sample regardless of whether spawnGrainNow() fired this sample.
    Grain::StereoSample renderActiveGrains(float volume) {
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

    // Bass-only equivalent of renderActiveGrains, called instead of it in
    // JerricanAudioProcessor's per-sample loop for voice 0. Bass's grains
    // sum close to linearly (rather than the sqrt(N) the shared method
    // assumes for decorrelated random scatter) only when they're actually
    // correlated — i.e. the same pitch, which Wander controls directly
    // (0 = pedal tone, everything coincides; higher = pitches scatter
    // across the scale, closer to decorrelated). Blending the
    // normalization's exponent by Wander means the strong 1/N protection
    // only kicks in when it's actually needed (low Wander), rather than
    // unconditionally — at higher Wander it relaxes toward the same
    // sqrt(N) headroom the other three voices get, so raising Busy
    // doesn't quietly turn Bass down for every Wander setting. A fixed
    // extra boost on top accounts for Bass structurally having far fewer
    // simultaneously-active grains than Drone/Spark/Haze's continuous
    // overlapping textures (it's a melodic line, not a pad) — without it,
    // Bass reads quieter than the others even at a higher Volume setting.
    //
    // The normalization glides toward its target (an envelope follower,
    // not applied instantly) rather than snapping to it every sample: at
    // low Wander it's a much steeper function of N than sqrt(N), and at
    // high Groove/Busy, N can change several times a second as grains
    // start/stop — snapping the gain every time produced its own audible
    // stepping/pumping artifact (an earlier version of this fix baked a
    // fixed compensation into each grain at trigger time instead, which
    // fought renderActiveGrains' own per-sample renormalization the same
    // way). Fast attack (duck quickly when N just rose, to still catch
    // the coherent peak before it clips) / slow release (ease back up
    // when N falls, so decaying notes don't pump the volume). No
    // breathingGain_ term — that's driftCenter_/breathingGain_'s
    // renderSample-only drift mechanic, which spawnGrainNow never
    // touches, so it would always be a no-op 1.0 here anyway.
    Grain::StereoSample renderActiveGrainsCorrelated(float volume, float wander) {
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

        const float clampedWander = std::max(0.0f, std::min(1.0f, wander));
        const float normalizationExponent = 1.0f - 0.5f * clampedWander;  // 1.0 (1/N) .. 0.5 (1/sqrt(N))
        const float targetNormalization =
            activeCount > 0 ? std::pow(static_cast<float>(activeCount), -normalizationExponent) : 1.0f;
        const float coefficient = targetNormalization < correlatedNormalization_
                                       ? correlatedAttackCoefficient_
                                       : correlatedReleaseCoefficient_;
        correlatedNormalization_ += (targetNormalization - correlatedNormalization_) * coefficient;

        const float gain = volume * correlatedNormalization_ * kBassLoudnessBoost;
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

    void maybeSpawnGrain(float low, float high, float timbre, float complexity, float dissonance,
                         int rootSemitoneOffset) {
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
            const float rawPitch = random_.nextFloatRange(std::min(lo, hi), std::max(lo, hi));

            // Dissonance 0 = fully quantized to this voice's (rooted)
            // consonant scale, 1 = fully free/continuous like before this
            // macro existed. rootSemitoneOffset lets different voices
            // quantize to different degrees of the same scale shape,
            // rather than only ever landing on identical pitch classes.
            const float quantizedPitch = HarmonicScale::quantize(rawPitch, rootSemitoneOffset);
            const float pitch = quantizedPitch + (rawPitch - quantizedPitch) * dissonance;

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
    static constexpr float kWanderSpreadFraction = 0.35f;
    static constexpr float kCorrelatedAttackSeconds = 0.001f;   // ~1ms: fast enough to catch a pileup
    static constexpr float kCorrelatedReleaseSeconds = 0.03f;   // ~30ms: slow enough not to pump
    static constexpr float kBassLoudnessBoost = 1.5f;

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
    float correlatedNormalization_ = 1.0f;
    float correlatedAttackCoefficient_ = 1.0f;
    float correlatedReleaseCoefficient_ = 1.0f;
};
