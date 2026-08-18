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
// Spark uses renderSample() exactly as before — continuous stochastic
// grain spawning driven by Motion (pitch drift retarget rate) and
// Complexity (grain density). Ambient uses renderAmbientSample() and Haze
// uses renderHazeSample() — Speed/Drift (Motion relabeled) directly scale
// grain duration (see maybeSpawnAmbientGrain/maybeSpawnHazeGrain; this is
// the dominant driver of how fast either voice feels like it's evolving,
// unlike Spark where Motion only nudges a hidden pitch-drift retarget
// rate), Layers (Complexity relabeled) drives grain density — triggering
// grains via Grain::triggerAmbient/triggerHaze instead of the random
// pickWaveform lottery. Ambient renders via renderActiveGrainsCorrelated
// (like Bass) since its long, narrow-pitch-spread overlapping grains are
// correlated enough that the plain sqrt(N) normalization let them hit the
// hard clamp and produce audible digital-clipping crackle; Haze also
// renders via renderActiveGrainsCorrelated but pinned to wander=1.0, i.e.
// the same sqrt(N) target renderActiveGrains uses — just reached via a
// glide instead of instantly. Haze's fuzz stage (see triggerHaze) makes
// heavily-saturated overlapping grains sum close to linearly and hit the
// hard clamp several times a second as grain count fluctuates; snapping
// the gain on every one of those hits was audible as a sharp "wave drop".
// A fully-correlated (stronger) exponent was tried here previously and
// produced its own audible "wave" duck/recover under long sustained
// notes, so the exponent stays gentle — only the smoothing was worth
// keeping. Bass instead uses
// spawnGrainNow(), called directly by JerricanAudioProcessor's metered
// scheduler (see BassGroovePattern.h) — its rhythm comes entirely from
// the beat grid now, with zero leftover stochastic spawning texture.
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

    // Ambient-only. updateDrift() (pitch-drift retarget rate) still runs
    // off Speed too, as a secondary texture, but the primary, clearly
    // audible effect of Speed is grain-duration scaling inside
    // maybeSpawnAmbientGrain — see there. `active` gates whether new
    // grains are allowed to spawn at all (Play/Stop, voice enabled),
    // fully separate from Layers' density value: Layers=0 must still mean
    // "very sparse", not "silent", so it can't double as the stop/start
    // gate the way Spark/Haze's spawnDensity=0-when-stopped trick does —
    // Ambient's own density floor (see maybeSpawnAmbientGrain) would
    // otherwise keep spawning new grains forever even while stopped, or
    // before Play was ever pressed.
    Grain::StereoSample renderAmbientSample(float pitchRangeLow, float pitchRangeHigh,
                                             float material, float speed, float complexity,
                                             float volume, float dissonance, float cleanliness,
                                             bool active, int rootSemitoneOffset = 0) {
        updateDrift(pitchRangeLow, pitchRangeHigh, speed);
        if (active) {
            maybeSpawnAmbientGrain(pitchRangeLow, pitchRangeHigh, material, speed, complexity,
                                   dissonance, cleanliness, rootSemitoneOffset);
        }
        return renderActiveGrainsCorrelated(volume, kAmbientCorrelationWander);
    }

    // Haze-only. Same shape as Ambient's renderAmbientSample: updateDrift()
    // (pitch-drift retarget rate) still runs off Drift as a secondary
    // texture, but Drift's primary, clearly audible effect is grain-
    // duration scaling inside maybeSpawnHazeGrain — see there. `active`
    // gates new-grain spawning the same way Ambient's does, separate from
    // Layers' density floor, for the same stop/start reason.
    Grain::StereoSample renderHazeSample(float pitchRangeLow, float pitchRangeHigh, float texture,
                                          float drift, float complexity, float volume,
                                          float dissonance, bool active,
                                          int rootSemitoneOffset = 0) {
        updateDrift(pitchRangeLow, pitchRangeHigh, drift);
        if (active) {
            maybeSpawnHazeGrain(pitchRangeLow, pitchRangeHigh, texture, drift, complexity,
                                dissonance, rootSemitoneOffset);
        }
        // renderActiveGrainsCorrelated with wander pinned to 1.0 — NOT
        // Ambient/Bass's stronger correlation exponents. Pinning wander=1
        // makes normalizationExponent=0.5, i.e. exactly the same sqrt(N)
        // target renderActiveGrains uses, so quiet/clean Haze playing
        // behaves identically to the plain version (an earlier attempt at
        // the fully-correlated variant here ducked/recovered audibly on
        // every new grain onset under long sustained notes — that problem
        // was the *exponent*, not the smoothing, so this keeps the gentle
        // exponent and only borrows the smoothing). The smoothing is what
        // Haze's fuzz stage actually needs: overlapping heavily-saturated
        // grains sum close to linearly and can hit the hard clamp several
        // times a second as grain count fluctuates, and renderActiveGrains'
        // instant per-sample renormalization made every one of those hits
        // audible as a sharp "wave drop". Gliding toward the same target
        // instead smooths those drops into an inaudible level trend.
        //
        // No boost here (unlike Bass/Ambient) — a flat multiplier only
        // makes the fuzz louder, not more saturated-sounding; the actual
        // "Big Muff" character comes from Grain::triggerHaze's waveshape
        // (hardClipBlend_/asymmetry_), not from gain staged at the pool
        // level.
        return renderActiveGrainsCorrelated(volume, 1.0f);
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
                       float sustain, float dissonance, float attack, int rootSemitoneOffset) {
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

            grain.triggerBlended(sampleRate_, pitch, durationMs, pan, timbre, attack, character_);
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

    // Correlated-aware equivalent of renderActiveGrains, used by both Bass
    // and Ambient in place of it. Grains that sum close to linearly
    // (rather than the sqrt(N) the shared method assumes for decorrelated
    // random scatter) — because they're actually correlated, i.e. close
    // in pitch — need stronger normalization or they hit the hard clamp
    // and produce audible digital-clipping crackle. For Bass, Wander
    // controls that directly (0 = pedal tone, everything coincides;
    // higher = pitches scatter across the scale, closer to decorrelated),
    // so its exponent is blended live by Wander. Ambient has no
    // equivalent per-note pitch-spread control (its scatter comes from a
    // fixed internal spread around a slowly-drifting center — see
    // updateDrift/localSpreadFraction), so it's called with a fixed
    // moderate correlation constant instead — its long, multi-second
    // grains overlap heavily enough, clustered narrowly enough, that this
    // needs to lean toward the stronger end most of the time. `boost` is
    // a flat makeup-gain multiplier on top, since Bass (a single melodic
    // line, usually 1 grain at a time) and Ambient (a dense continuous
    // pad) start from very different baseline loudness and need different
    // compensation.
    //
    // The normalization glides toward its target (an envelope follower,
    // not applied instantly) rather than snapping to it every sample: at
    // low correlation-exponent settings it's a much steeper function of N
    // than sqrt(N), and N can change several times a second as grains
    // start/stop — snapping the gain every time produced its own audible
    // stepping/pumping artifact (an earlier version of this fix baked a
    // fixed compensation into each grain at trigger time instead, which
    // fought renderActiveGrains' own per-sample renormalization the same
    // way). Moderate attack/slower release (see kCorrelatedAttackSeconds/
    // kCorrelatedReleaseSeconds) — reacts to sustained density trends
    // rather than ducking on every individual note onset, which produced
    // an audible "wave" pump under long sustained notes when the pattern
    // fired new grains while an old one was still ringing. No
    // breathingGain_ term — that's driftCenter_/breathingGain_'s
    // renderSample-only drift mechanic, which spawnGrainNow/
    // maybeSpawnAmbientGrain never touches, so it would always be a
    // no-op 1.0 here anyway.
    Grain::StereoSample renderActiveGrainsCorrelated(float volume, float wander,
                                                       float boost = 1.0f) {
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

        const float gain = volume * correlatedNormalization_ * boost;
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

    // Ambient-only equivalent of maybeSpawnGrain — same pitch-spread/
    // Dissonance shape, calling triggerAmbient with Material/Cleanliness
    // instead of trigger(pickWaveform(timbre)), with three differences:
    // Layers=0 is floored above true zero (a pad going fully silent
    // because a knob hit its exact minimum isn't "sparse", it's broken —
    // same reasoning as Bass's Busy floor); the density ceiling is its
    // own, much lower than maxGrainsPerSecond (that constant assumes
    // short instrument-like grains; Ambient's grains last seconds each,
    // so even a handful of new layers per second already means a very
    // dense overlapping wash); and Speed directly scales grain duration —
    // the dominant, clearly-audible driver of how fast this pad's layers
    // turn over and feel like they're moving, unlike the barely-perceptible
    // pitch-drift-retarget effect Motion/Speed has via updateDrift() alone.
    // Speed=0 stretches duration out by kAmbientSlowDurationMultiple, so
    // "slow" is genuinely glacial rather than nearly the same as "fast".
    void maybeSpawnAmbientGrain(float low, float high, float material, float speed, float complexity,
                                float dissonance, float cleanliness, int rootSemitoneOffset) {
        const float clampedComplexity = std::max(0.0f, std::min(1.0f, complexity));
        const float grainsPerSecond =
            kMinAmbientGrainsPerSecond +
            clampedComplexity * (kMaxAmbientGrainsPerSecond - kMinAmbientGrainsPerSecond);
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

            const float quantizedPitch = HarmonicScale::quantize(rawPitch, rootSemitoneOffset);
            const float pitch = quantizedPitch + (rawPitch - quantizedPitch) * dissonance;

            const float clampedSpeed = std::max(0.0f, std::min(1.0f, speed));
            const float durationScale = lerp(kAmbientSlowDurationMultiple, 1.0f, clampedSpeed);
            const float durationMs =
                random_.nextFloatRange(minGrainDurationMs_, maxGrainDurationMs_) * durationScale;
            const float pan = random_.nextFloat01();

            grain.triggerAmbient(sampleRate_, pitch, durationMs, pan, material, cleanliness);
            return;
        }
    }

    // Haze-only equivalent of maybeSpawnAmbientGrain — same shape: Layers
    // floored above true zero, its own (Haze-specific) density ceiling
    // since its grains also last seconds each, and Drift scaling grain
    // duration as the dominant audible effect (kHazeSlowDurationMultiple
    // stretches Drift=0 out relative to the configured/Drift=1 range).
    void maybeSpawnHazeGrain(float low, float high, float texture, float drift, float complexity,
                             float dissonance, int rootSemitoneOffset) {
        const float clampedComplexity = std::max(0.0f, std::min(1.0f, complexity));
        const float grainsPerSecond =
            kMinHazeGrainsPerSecond +
            clampedComplexity * (kMaxHazeGrainsPerSecond - kMinHazeGrainsPerSecond);
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

            const float quantizedPitch = HarmonicScale::quantize(rawPitch, rootSemitoneOffset);
            const float pitch = quantizedPitch + (rawPitch - quantizedPitch) * dissonance;

            const float clampedDrift = std::max(0.0f, std::min(1.0f, drift));
            const float durationScale = lerp(kHazeSlowDurationMultiple, 1.0f, clampedDrift);
            const float durationMs =
                random_.nextFloatRange(minGrainDurationMs_, maxGrainDurationMs_) * durationScale;
            const float pan = random_.nextFloat01();

            grain.triggerHaze(sampleRate_, pitch, durationMs, pan, texture);
            return;
        }
    }

    static float lerp(float a, float b, float t) { return a + (b - a) * t; }

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
    // This gain follower is shared across the whole cloud's mixed output,
    // not per-grain — so on a long Sustain=1 note, every subsequent note
    // the pattern fires while the old one is still ringing changes the
    // active-grain count and re-triggers this envelope, ducking the
    // ALREADY-SOUNDING sustained note too. At the original 1ms attack /
    // 30ms release this was audible as a repeating "wave down, wave up"
    // pump for the note's whole life. kCoreHeadroom (Grain.h) already
    // gives real peak margin now, so this can afford to react far more
    // gently — it's here to tame sustained buildup trends, not to be a
    // sample-accurate limiter that ducks on every single note onset.
    static constexpr float kCorrelatedAttackSeconds = 0.02f;   // ~20ms
    static constexpr float kCorrelatedReleaseSeconds = 0.25f;  // ~250ms
    // Ambient's own density range — see maybeSpawnAmbientGrain. A floor
    // above zero so Layers=0 is "a single sparse layer", not silence; a
    // ceiling far below the shared maxGrainsPerSecond since Ambient's
    // multi-second grains overlap far more per spawn than short ones do.
    static constexpr float kMinAmbientGrainsPerSecond = 0.15f;
    static constexpr float kMaxAmbientGrainsPerSecond = 5.0f;
    // Ambient has no per-note pitch-spread control the way Bass's Wander
    // is — a fixed, moderately-correlated assumption for
    // renderActiveGrainsCorrelated instead (see there).
    static constexpr float kAmbientCorrelationWander = 0.3f;
    // How much longer Speed=0 grains last than the configured (Speed=1)
    // min/maxGrainDurationMs_ range — see maybeSpawnAmbientGrain.
    static constexpr float kAmbientSlowDurationMultiple = 3.0f;
    // Haze's own equivalents — see maybeSpawnHazeGrain/renderHazeSample.
    static constexpr float kMinHazeGrainsPerSecond = 0.15f;
    static constexpr float kMaxHazeGrainsPerSecond = 5.0f;
    static constexpr float kHazeSlowDurationMultiple = 3.0f;

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
