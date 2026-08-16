#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

#include "FastRandom.h"
#include "MeterTable.h"

// The engine behind Bass's metered rhythm (see JerricanProcessor.h) — a
// single-voice adaptation of Marmite's GroovePattern. Holds a persistent
// mask, one bar's worth of on/off state, mostly held steady and only
// mutated slot-by-slot on bar boundaries rather than re-decided every
// subdivision.
//
// Busy plugs into the same role Marmite's Density does (raises the mask's
// overall on-probability). Groove plugs into the same role Marmite's
// effectiveWild does (0 = the accent profile fully governs — a locked,
// repeating rhythm; 1 = the profile's influence fades toward uniform/
// unconstrained placement) *and* drives the per-slot micro-timing delay
// plus a small pitch/velocity humanization jitter — Groove is Bass's one
// "how loose does this feel" control, doing the job Marmite splits across
// Wild (global) and each voice's own Busy-offset, which isn't needed here
// since there's only one voice on the clock so far.
class BassGroovePattern {
public:
    struct Trigger {
        float pitchSemitones = 0.0f;
        float velocity = 1.0f;
    };

    BassGroovePattern(std::uint32_t seed, const MeterTable::AccentProfile* accentProfile,
                      int activeSlotCount)
        : random_(seed), accentProfile_(accentProfile), activeSlotCount_(activeSlotCount) {}

    // Called whenever the meter changes — the caller must also call
    // forceRegenerateNextBoundary() so the stale mask (sized/shaped for the
    // old meter) isn't reused.
    void setAccentProfile(const MeterTable::AccentProfile* accentProfile, int activeSlotCount) {
        accentProfile_ = accentProfile;
        activeSlotCount_ = activeSlotCount;
    }

    // Forces a full mask regeneration on the next bar-boundary update(),
    // regardless of Busy/Groove having changed — used when the meter (and
    // therefore the mask's shape/length) has just changed, or on a host
    // transport start/loop re-snap.
    void forceRegenerateNextBoundary() { initialized_ = false; }

    // Called once per sample, in lockstep with the shared PatternClock's
    // tick() (via onGridBoundary). currentSlot is a shared bar-position
    // counter (0..activeSlotCount_-1) owned by the caller
    // (JerricanAudioProcessor), incremented on every grid tick.
    std::optional<Trigger> update(bool onGridBoundary, int currentSlot, float busy, float groove,
                                  float evolutionAmount, double samplesPerSubdivision) {
        if (onGridBoundary) {
            // Regenerate the whole bar's mask exactly once per bar (slot
            // 0), plus once unconditionally on the very first call so Bass
            // isn't silent for its entire first bar.
            if (!initialized_ || currentSlot == 0) {
                regenerateBar(busy, groove, evolutionAmount);
                initialized_ = true;
                lastBusy_ = busy;
                lastGroove_ = groove;
            }

            if (slotActive_[static_cast<std::size_t>(currentSlot)]) {
                // A half-subdivision-max random delay for micro-timing
                // looseness, scaled by Groove — 0 locks exactly to the
                // grid, 1 up to half a subdivision late.
                const int maxDelaySamples = static_cast<int>(
                    groove * static_cast<float>(samplesPerSubdivision) * 0.5f);
                pendingDelaySamples_ =
                    maxDelaySamples > 0
                        ? static_cast<int>(random_.nextFloat01() * static_cast<float>(maxDelaySamples))
                        : 0;
                pendingVelocity_ = 1.0f - random_.nextFloat01() * groove * 0.5f;
                pendingPitchSemitones_ = (random_.nextFloat01() * 2.0f - 1.0f) * groove * 2.0f;
                hasPendingTrigger_ = true;
            }
        }

        if (hasPendingTrigger_) {
            if (pendingDelaySamples_ <= 0) {
                hasPendingTrigger_ = false;
                return Trigger{pendingPitchSemitones_, pendingVelocity_};
            }
            --pendingDelaySamples_;
        }

        return std::nullopt;
    }

private:
    void regenerateBar(float busy, float groove, float evolutionAmount) {
        // A genuine manual edit to Busy or Groove since the last
        // regeneration always forces a full fresh re-roll, independent of
        // Evolution Amount — turning a knob has to do something
        // immediately. Otherwise, per-slot mutation is tied to Amount,
        // plus a small nonzero floor even at Amount=0 so the pattern can't
        // get permanently stuck on one unlucky initial roll forever.
        const bool paramsChanged = !initialized_ ||
                                   std::abs(busy - lastBusy_) > kParamChangeThreshold ||
                                   std::abs(groove - lastGroove_) > kParamChangeThreshold;
        const float mutationProbability =
            kBaselineMutationProbability + evolutionAmount * (0.1f + 0.5f * groove);

        // Busy=0 shouldn't mean total silence — a bass that never plays
        // anything isn't a sparse bass, it's a broken one. Flooring the
        // effective density guarantees at least an occasional hit (mostly
        // landing on the strongest accent slots, since it's still scaled
        // by slotWeight) rather than a hard cutoff to zero.
        const float effectiveBusy = kMinBusy + busy * (1.0f - kMinBusy);

        for (int slot = 0; slot < activeSlotCount_; ++slot) {
            const float profileWeight = (*accentProfile_)[static_cast<std::size_t>(slot)];

            // Even the busy-floor above is still only a *probability* —
            // at low Busy, a bar can (and, over a few bars, realistically
            // will) roll every one of its downbeats inactive, which reads
            // as "the instrument stopped." A pulse group's true downbeat
            // (weight 1.0 — see MeterTable::generateBassAccentProfile)
            // always fires: a walking bass should never skip its own
            // downbeat regardless of how low Busy is turned. Busy/Groove
            // still fully govern everything in between.
            if (profileWeight >= kGuaranteedDownbeatWeight) {
                slotActive_[static_cast<std::size_t>(slot)] = true;
                continue;
            }

            if (paramsChanged || random_.nextFloat01() < mutationProbability) {
                // lerp(accentProfile[slot], 1.0, groove) — the profile
                // fully shapes slot likelihood at groove=0, and its
                // influence fades to nothing (uniform) at groove=1.
                const float slotWeight = profileWeight + (1.0f - profileWeight) * groove;
                slotActive_[static_cast<std::size_t>(slot)] =
                    random_.nextFloat01() < effectiveBusy * slotWeight;
            }
        }
    }

    static constexpr float kParamChangeThreshold = 0.01f;
    static constexpr float kBaselineMutationProbability = 0.03f;
    static constexpr float kMinBusy = 0.12f;
    static constexpr float kGuaranteedDownbeatWeight = 0.99f;

    FastRandom random_;
    const MeterTable::AccentProfile* accentProfile_;
    int activeSlotCount_;
    std::array<bool, MeterTable::kMaxSlotsPerBar> slotActive_{};
    bool initialized_ = false;
    float lastBusy_ = 0.0f;
    float lastGroove_ = 0.0f;
    bool hasPendingTrigger_ = false;
    int pendingDelaySamples_ = 0;
    float pendingVelocity_ = 1.0f;
    float pendingPitchSemitones_ = 0.0f;
};
