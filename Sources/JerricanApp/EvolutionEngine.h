#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "FastRandom.h"
#include "VoiceModel.h"

// One per voice. Drives the voice's own macros (Volume, Pitch Range center,
// Timbre, Motion, Complexity, Dissonance) to wander on their own over time,
// scaled by two independent global controls: Amount (0 = the macros stay
// exactly where the user set them — update() is a no-op regardless of
// Speed; 1 = new targets are picked often) and Speed (how quickly the
// current value glides toward whichever target was last picked —
// independent of how often a new one is chosen). Each of the 6 macros can
// also be individually opted out via setXEnabled(false), so some of a
// voice's controls can stay under manual control while others evolve.
// Uses the same "occasionally pick a new random target, smooth toward it"
// technique as GrainCloud's internal drift, just applied to the macro
// values themselves instead of grain-cloud-internal state.
//
// Pitch range *width* is held fixed at whatever resetTo() was given — only
// where the range sits wanders, not how wide it is, so it can't collapse
// or invert.
//
// resetTo()/the setXEnabled()/resyncX() methods are called from the UI
// thread (Stop/Reset, Randomize, and the per-parameter toggles in
// Main.cpp), while update() runs on the audio thread — every field that
// crosses that boundary is atomic (relaxed: each field is independent, no
// ordering needed between them), matching the pattern already used
// throughout VoiceModel.
class EvolutionEngine {
public:
    explicit EvolutionEngine(std::uint32_t seed) : random_(seed) {}

    void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }

    void resetTo(float pitchCenter, float pitchWidth, float volume, float timbre, float motion,
                 float complexity, float dissonance) {
        pitchWidth_.store(pitchWidth, std::memory_order_relaxed);
        store(pitchCenterCurrent_, pitchCenterTarget_, pitchCenter);
        store(volumeCurrent_, volumeTarget_, volume);
        store(timbreCurrent_, timbreTarget_, timbre);
        store(motionCurrent_, motionTarget_, motion);
        store(complexityCurrent_, complexityTarget_, complexity);
        store(dissonanceCurrent_, dissonanceTarget_, dissonance);
        sampleCounter_.store(0, std::memory_order_relaxed);
    }

    void setVolumeEnabled(bool enabled) { volumeEnabled_.store(enabled, std::memory_order_relaxed); }
    void setPitchRangeEnabled(bool enabled) {
        pitchRangeEnabled_.store(enabled, std::memory_order_relaxed);
    }
    void setTimbreEnabled(bool enabled) { timbreEnabled_.store(enabled, std::memory_order_relaxed); }
    void setMotionEnabled(bool enabled) { motionEnabled_.store(enabled, std::memory_order_relaxed); }
    void setComplexityEnabled(bool enabled) {
        complexityEnabled_.store(enabled, std::memory_order_relaxed);
    }
    void setDissonanceEnabled(bool enabled) {
        dissonanceEnabled_.store(enabled, std::memory_order_relaxed);
    }

    // Snap a parameter's internal current/target to a live value — call
    // whenever the UI writes that parameter directly (manual drag, or
    // re-enabling a toggle), so the engine picks up and drifts onward from
    // the live value instead of fighting it with a stale internal one.
    void resyncVolume(float value) { store(volumeCurrent_, volumeTarget_, value); }

    // Pitch Range needs both center and width resynced — width only ever
    // otherwise changes via resetTo(), so a manual resize of the range
    // (not just a move) would otherwise leave the engine's width stale and
    // it would silently snap the range back to the old width on its next
    // write.
    void resyncPitchRange(float low, float high) {
        pitchWidth_.store(high - low, std::memory_order_relaxed);
        store(pitchCenterCurrent_, pitchCenterTarget_, (low + high) * 0.5f);
    }
    void resyncTimbre(float value) { store(timbreCurrent_, timbreTarget_, value); }
    void resyncMotion(float value) { store(motionCurrent_, motionTarget_, value); }
    void resyncComplexity(float value) { store(complexityCurrent_, complexityTarget_, value); }
    void resyncDissonance(float value) { store(dissonanceCurrent_, dissonanceTarget_, value); }

    // Called once per sample. Provably inert at amount <= 0 — returns
    // immediately without touching the voice, regardless of speed.
    void update(VoiceModel& voice, float amount, float speed) {
        if (amount <= 0.0f) {
            return;
        }

        const float pitchWidth = pitchWidth_.load(std::memory_order_relaxed);

        const float retargetProbabilityPerSample =
            (amount * retargetRateSpanHz) / static_cast<float>(sampleRate_);
        if (random_.nextFloat01() < retargetProbabilityPerSample) {
            const float halfWidth = pitchWidth * 0.5f;
            pitchCenterTarget_.store(random_.nextFloatRange(halfWidth, 1.0f - halfWidth),
                                     std::memory_order_relaxed);
            volumeTarget_.store(random_.nextFloat01(), std::memory_order_relaxed);
            timbreTarget_.store(random_.nextFloat01(), std::memory_order_relaxed);
            motionTarget_.store(random_.nextFloat01(), std::memory_order_relaxed);
            complexityTarget_.store(random_.nextFloat01(), std::memory_order_relaxed);
            dissonanceTarget_.store(random_.nextFloat01(), std::memory_order_relaxed);
        }

        // Exponential mapping so Speed is useful across a huge dynamic
        // range: ~60s glides at speed=0 up to ~10ms (near-instant) at
        // speed=1, rather than the narrow linear range a direct multiply
        // would give.
        const float smoothing =
            slowSmoothingCoefficient *
            std::pow(fastSmoothingCoefficient / slowSmoothingCoefficient, speed);

        const bool shouldWrite = ++sampleCounter_ >= writeIntervalSamples;
        if (shouldWrite) {
            sampleCounter_ = 0;
        }

        smoothAndMaybeWrite(pitchCenterCurrent_, pitchCenterTarget_, smoothing,
                            pitchRangeEnabled_, shouldWrite, [&](float center) {
                                const float halfWidth = pitchWidth * 0.5f;
                                voice.setPitchRange(center - halfWidth, center + halfWidth);
                            });
        smoothAndMaybeWrite(volumeCurrent_, volumeTarget_, smoothing, volumeEnabled_, shouldWrite,
                            [&](float value) { voice.setVolume(value); });
        smoothAndMaybeWrite(timbreCurrent_, timbreTarget_, smoothing, timbreEnabled_, shouldWrite,
                            [&](float value) { voice.setTimbre(value); });
        smoothAndMaybeWrite(motionCurrent_, motionTarget_, smoothing, motionEnabled_, shouldWrite,
                            [&](float value) { voice.setMotion(value); });
        smoothAndMaybeWrite(complexityCurrent_, complexityTarget_, smoothing, complexityEnabled_,
                            shouldWrite, [&](float value) { voice.setComplexity(value); });
        smoothAndMaybeWrite(dissonanceCurrent_, dissonanceTarget_, smoothing, dissonanceEnabled_,
                            shouldWrite, [&](float value) { voice.setDissonance(value); });
    }

private:
    static void store(std::atomic<float>& current, std::atomic<float>& target, float value) {
        current.store(value, std::memory_order_relaxed);
        target.store(value, std::memory_order_relaxed);
    }

    template <typename WriteFn>
    static void smoothAndMaybeWrite(std::atomic<float>& current, std::atomic<float>& target,
                                    float smoothing, const std::atomic<bool>& enabled,
                                    bool shouldWrite, WriteFn&& write) {
        if (!enabled.load(std::memory_order_relaxed)) {
            return;
        }

        float value = current.load(std::memory_order_relaxed);
        value += (target.load(std::memory_order_relaxed) - value) * smoothing;
        current.store(value, std::memory_order_relaxed);

        if (shouldWrite) {
            write(value);
        }
    }

    static constexpr int writeIntervalSamples = 64;
    static constexpr float retargetRateSpanHz = 0.3f;
    static constexpr float slowSmoothingCoefficient = 3.78e-7f;  // ~60s glide
    static constexpr float fastSmoothingCoefficient = 2.268e-3f;  // ~10ms glide

    FastRandom random_;
    double sampleRate_ = 44100.0;
    std::atomic<int> sampleCounter_{0};
    std::atomic<float> pitchWidth_{0.2f};
    std::atomic<float> pitchCenterCurrent_{0.5f};
    std::atomic<float> pitchCenterTarget_{0.5f};
    std::atomic<float> volumeCurrent_{0.5f};
    std::atomic<float> volumeTarget_{0.5f};
    std::atomic<float> timbreCurrent_{0.5f};
    std::atomic<float> timbreTarget_{0.5f};
    std::atomic<float> motionCurrent_{0.5f};
    std::atomic<float> motionTarget_{0.5f};
    std::atomic<float> complexityCurrent_{0.5f};
    std::atomic<float> complexityTarget_{0.5f};
    std::atomic<float> dissonanceCurrent_{0.5f};
    std::atomic<float> dissonanceTarget_{0.5f};
    std::atomic<bool> volumeEnabled_{true};
    std::atomic<bool> pitchRangeEnabled_{true};
    std::atomic<bool> timbreEnabled_{true};
    std::atomic<bool> motionEnabled_{true};
    std::atomic<bool> complexityEnabled_{true};
    std::atomic<bool> dissonanceEnabled_{true};
};
