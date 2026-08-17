#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "FastRandom.h"
#include "VoiceModel.h"

// One per voice. Drives the voice's own macros (Volume, Pitch Range center,
// Timbre, Groove, Wander, Dissonance, Busy, Sustain, Cleanliness, Attack)
// to wander on their own over time, scaled by two independent global
// controls: Amount
// (0 = the macros stay exactly where the user set them — update() is a
// no-op regardless of Speed; 1 = new targets are picked often) and Speed
// (how quickly the current value glides toward whichever target was last
// picked — independent of how often a new one is chosen). Each macro can
// also be individually opted out via setXEnabled(false), so some of a
// voice's controls can stay under manual control while others evolve.
// Uses the same "occasionally pick a new random target, smooth toward it"
// technique as GrainCloud's internal drift, just applied to the macro
// values themselves instead of grain-cloud-internal state.
//
// Storage is a data-driven array (MacroState per Macro enumerator) rather
// than one hand-written Current_/Target_/Enabled_ triplet per macro — with
// 10 macros now (grown from the original 6 to add Busy/Sustain/Attack for
// Bass and Cleanliness for Ambient, and more voices expected to grow their
// own bespoke macros in future rounds), spelling each one out by hand
// would make this file grow linearly forever.
// The public surface keeps named per-macro methods (setVolumeEnabled,
// resyncTimbre, etc.) so call sites elsewhere don't need to switch to the
// generic Macro-parameterized API — only this class's internals changed
// shape.
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
    enum class Macro {
        Volume,
        PitchCenter,
        Timbre,
        Groove,
        Wander,
        Dissonance,
        Busy,
        Sustain,
        Cleanliness,
        Attack
    };
    static constexpr int kMacroCount = 10;

    explicit EvolutionEngine(std::uint32_t seed) : random_(seed) {}

    void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }

    void resetTo(float pitchCenter, float pitchWidth, float volume, float timbre, float groove,
                 float wander, float dissonance, float busy = 0.5f, float sustain = 0.5f,
                 float cleanliness = 0.5f, float attack = 0.5f) {
        pitchWidth_.store(pitchWidth, std::memory_order_relaxed);
        storeMacro(Macro::PitchCenter, pitchCenter);
        storeMacro(Macro::Volume, volume);
        storeMacro(Macro::Timbre, timbre);
        storeMacro(Macro::Groove, groove);
        storeMacro(Macro::Wander, wander);
        storeMacro(Macro::Dissonance, dissonance);
        storeMacro(Macro::Busy, busy);
        storeMacro(Macro::Sustain, sustain);
        storeMacro(Macro::Cleanliness, cleanliness);
        storeMacro(Macro::Attack, attack);
        sampleCounter_.store(0, std::memory_order_relaxed);
    }

    // Generic accessors, and the named per-macro wrappers below built on
    // top of them.
    void setEnabled(Macro macro, bool enabled) {
        macros_[index(macro)].enabled.store(enabled, std::memory_order_relaxed);
    }
    bool isEnabled(Macro macro) const {
        return macros_[index(macro)].enabled.load(std::memory_order_relaxed);
    }
    void resync(Macro macro, float value) { storeMacro(macro, value); }

    void setVolumeEnabled(bool enabled) { setEnabled(Macro::Volume, enabled); }
    void setPitchRangeEnabled(bool enabled) { setEnabled(Macro::PitchCenter, enabled); }
    void setTimbreEnabled(bool enabled) { setEnabled(Macro::Timbre, enabled); }
    void setGrooveEnabled(bool enabled) { setEnabled(Macro::Groove, enabled); }
    void setWanderEnabled(bool enabled) { setEnabled(Macro::Wander, enabled); }
    void setDissonanceEnabled(bool enabled) { setEnabled(Macro::Dissonance, enabled); }
    void setBusyEnabled(bool enabled) { setEnabled(Macro::Busy, enabled); }
    void setSustainEnabled(bool enabled) { setEnabled(Macro::Sustain, enabled); }
    void setCleanlinessEnabled(bool enabled) { setEnabled(Macro::Cleanliness, enabled); }
    void setAttackEnabled(bool enabled) { setEnabled(Macro::Attack, enabled); }

    // Read back a parameter's current opt-in/out state — used both to
    // decide whether a toggle flip needs a resync and to keep an external
    // surface (the per-voice Evolution toggle LEDs, MIDI Learn) in sync
    // with changes made some other way.
    bool isVolumeEnabled() const { return isEnabled(Macro::Volume); }
    bool isPitchRangeEnabled() const { return isEnabled(Macro::PitchCenter); }
    bool isTimbreEnabled() const { return isEnabled(Macro::Timbre); }
    bool isGrooveEnabled() const { return isEnabled(Macro::Groove); }
    bool isWanderEnabled() const { return isEnabled(Macro::Wander); }
    bool isDissonanceEnabled() const { return isEnabled(Macro::Dissonance); }
    bool isBusyEnabled() const { return isEnabled(Macro::Busy); }
    bool isSustainEnabled() const { return isEnabled(Macro::Sustain); }
    bool isCleanlinessEnabled() const { return isEnabled(Macro::Cleanliness); }
    bool isAttackEnabled() const { return isEnabled(Macro::Attack); }

    // Snap a parameter's internal current/target to a live value — call
    // whenever the UI writes that parameter directly (manual drag, or
    // re-enabling a toggle), so the engine picks up and drifts onward from
    // the live value instead of fighting it with a stale internal one.
    void resyncVolume(float value) { resync(Macro::Volume, value); }

    // Pitch Range needs both center and width resynced — width only ever
    // otherwise changes via resetTo(), so a manual resize of the range
    // (not just a move) would otherwise leave the engine's width stale and
    // it would silently snap the range back to the old width on its next
    // write.
    void resyncPitchRange(float low, float high) {
        pitchWidth_.store(high - low, std::memory_order_relaxed);
        resync(Macro::PitchCenter, (low + high) * 0.5f);
    }
    void resyncTimbre(float value) { resync(Macro::Timbre, value); }
    void resyncGroove(float value) { resync(Macro::Groove, value); }
    void resyncWander(float value) { resync(Macro::Wander, value); }
    void resyncDissonance(float value) { resync(Macro::Dissonance, value); }
    void resyncBusy(float value) { resync(Macro::Busy, value); }
    void resyncSustain(float value) { resync(Macro::Sustain, value); }
    void resyncCleanliness(float value) { resync(Macro::Cleanliness, value); }
    void resyncAttack(float value) { resync(Macro::Attack, value); }

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
            macros_[index(Macro::PitchCenter)].target.store(
                random_.nextFloatRange(halfWidth, 1.0f - halfWidth), std::memory_order_relaxed);
            for (const Macro macro : {Macro::Volume, Macro::Timbre, Macro::Groove, Macro::Wander,
                                      Macro::Dissonance, Macro::Busy, Macro::Sustain,
                                      Macro::Cleanliness, Macro::Attack}) {
                macros_[index(macro)].target.store(random_.nextFloat01(), std::memory_order_relaxed);
            }
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

        smoothAndMaybeWrite(Macro::PitchCenter, smoothing, shouldWrite, [&](float center) {
            const float halfWidth = pitchWidth * 0.5f;
            voice.setPitchRange(center - halfWidth, center + halfWidth);
        });
        smoothAndMaybeWrite(Macro::Volume, smoothing, shouldWrite,
                            [&](float value) { voice.setVolume(value); });
        smoothAndMaybeWrite(Macro::Timbre, smoothing, shouldWrite,
                            [&](float value) { voice.setTimbre(value); });
        smoothAndMaybeWrite(Macro::Groove, smoothing, shouldWrite,
                            [&](float value) { voice.setGroove(value); });
        smoothAndMaybeWrite(Macro::Wander, smoothing, shouldWrite,
                            [&](float value) { voice.setWander(value); });
        smoothAndMaybeWrite(Macro::Dissonance, smoothing, shouldWrite,
                            [&](float value) { voice.setDissonance(value); });
        smoothAndMaybeWrite(Macro::Busy, smoothing, shouldWrite,
                            [&](float value) { voice.setBusy(value); });
        smoothAndMaybeWrite(Macro::Sustain, smoothing, shouldWrite,
                            [&](float value) { voice.setSustain(value); });
        smoothAndMaybeWrite(Macro::Cleanliness, smoothing, shouldWrite,
                            [&](float value) { voice.setCleanliness(value); });
        smoothAndMaybeWrite(Macro::Attack, smoothing, shouldWrite,
                            [&](float value) { voice.setAttack(value); });
    }

private:
    struct MacroState {
        std::atomic<float> current{0.5f};
        std::atomic<float> target{0.5f};
        std::atomic<bool> enabled{true};
    };

    static constexpr int index(Macro macro) { return static_cast<int>(macro); }

    void storeMacro(Macro macro, float value) {
        auto& state = macros_[index(macro)];
        state.current.store(value, std::memory_order_relaxed);
        state.target.store(value, std::memory_order_relaxed);
    }

    template <typename WriteFn>
    void smoothAndMaybeWrite(Macro macro, float smoothing, bool shouldWrite, WriteFn&& write) {
        auto& state = macros_[index(macro)];
        if (!state.enabled.load(std::memory_order_relaxed)) {
            return;
        }

        float value = state.current.load(std::memory_order_relaxed);
        value += (state.target.load(std::memory_order_relaxed) - value) * smoothing;
        state.current.store(value, std::memory_order_relaxed);

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
    std::array<MacroState, kMacroCount> macros_;
};
