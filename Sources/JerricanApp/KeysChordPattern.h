#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>

#include "FastRandom.h"
#include "MeterTable.h"

// Keys's metered chord-comping engine — copies BassGroovePattern's
// rhythm-mask mechanism nearly verbatim (Busy -> density, Groove ->
// placement/feel + humanization, guaranteed downbeat, bar-boundary
// regeneration; see BassGroovePattern.h for the full rationale, not
// repeated here) and adds diatonic chord-degree selection on top: each
// bar-boundary regeneration also has a chance to move to a new degree via
// a small weighted transition table, so progressions read as directed
// motion rather than uniformly random degree jumps.
class KeysChordPattern {
public:
    struct Trigger {
        int degree = 0;
        float pitchJitterSemitones = 0.0f;
        float velocity = 1.0f;
    };

    KeysChordPattern(std::uint32_t seed, const MeterTable::AccentProfile* accentProfile,
                      int activeSlotCount)
        : random_(seed), accentProfile_(accentProfile), activeSlotCount_(activeSlotCount) {}

    void setAccentProfile(const MeterTable::AccentProfile* accentProfile, int activeSlotCount) {
        accentProfile_ = accentProfile;
        activeSlotCount_ = activeSlotCount;
    }

    void forceRegenerateNextBoundary() { initialized_ = false; }

    std::optional<Trigger> update(bool onGridBoundary, int currentSlot, float busy, float groove,
                                  float evolutionAmount, double samplesPerSubdivision) {
        if (onGridBoundary) {
            if (!initialized_ || currentSlot == 0) {
                regenerateBar(busy, groove, evolutionAmount);
                initialized_ = true;
                lastBusy_ = busy;
                lastGroove_ = groove;
            }

            if (slotActive_[static_cast<std::size_t>(currentSlot)]) {
                const int maxDelaySamples = static_cast<int>(
                    groove * static_cast<float>(samplesPerSubdivision) * 0.5f);
                pendingDelaySamples_ =
                    maxDelaySamples > 0
                        ? static_cast<int>(random_.nextFloat01() * static_cast<float>(maxDelaySamples))
                        : 0;
                pendingVelocity_ = 1.0f - random_.nextFloat01() * groove * 0.5f;
                pendingPitchJitterSemitones_ = (random_.nextFloat01() * 2.0f - 1.0f) * groove * 2.0f;
                pendingDegree_ = currentDegree_;
                hasPendingTrigger_ = true;
            }
        }

        if (hasPendingTrigger_) {
            if (pendingDelaySamples_ <= 0) {
                hasPendingTrigger_ = false;
                return Trigger{pendingDegree_, pendingPitchJitterSemitones_, pendingVelocity_};
            }
            --pendingDelaySamples_;
        }

        return std::nullopt;
    }

private:
    void regenerateBar(float busy, float groove, float evolutionAmount) {
        const bool paramsChanged = !initialized_ ||
                                   std::abs(busy - lastBusy_) > kParamChangeThreshold ||
                                   std::abs(groove - lastGroove_) > kParamChangeThreshold;
        const float mutationProbability =
            kBaselineMutationProbability + evolutionAmount * (0.1f + 0.5f * groove);
        const float effectiveBusy = kMinBusy + busy * (1.0f - kMinBusy);

        for (int slot = 0; slot < activeSlotCount_; ++slot) {
            const float profileWeight = (*accentProfile_)[static_cast<std::size_t>(slot)];

            if (slot == 0) {
                slotActive_[static_cast<std::size_t>(slot)] = true;
                continue;
            }

            if (paramsChanged || random_.nextFloat01() < mutationProbability) {
                const float slotWeight = profileWeight + (1.0f - profileWeight) * groove;
                slotActive_[static_cast<std::size_t>(slot)] =
                    random_.nextFloat01() < effectiveBusy * slotWeight;
            }
        }

        // Chord-degree motion: each bar has a chance to move to a new
        // degree (not every hit — a comping pattern reiterates one chord
        // a few times before moving, it doesn't relitigate the harmony on
        // every single note). Weighted by kTransitions[currentDegree_], so
        // motion favors common tonal moves (I->IV/V/vi, V->I, ii->V, etc.)
        // over uniformly random degree jumps.
        if (!initialized_ || random_.nextFloat01() < kDegreeChangeProbability) {
            currentDegree_ = pickNextDegree(currentDegree_);
        }
    }

    int pickNextDegree(int from) {
        const auto& row = kTransitions[static_cast<std::size_t>(from)];
        float totalWeight = 0.0f;
        for (const auto& [degree, weight] : row) {
            totalWeight += weight;
        }
        float pick = random_.nextFloat01() * totalWeight;
        for (const auto& [degree, weight] : row) {
            if (pick < weight) {
                return degree;
            }
            pick -= weight;
        }
        return from;
    }

    static constexpr float kParamChangeThreshold = 0.01f;
    static constexpr float kBaselineMutationProbability = 0.03f;
    static constexpr float kMinBusy = 0.12f;
    // A new chord roughly every couple of bars on average, not every bar —
    // gives each chord room to actually be heard as a comping figure.
    static constexpr float kDegreeChangeProbability = 0.4f;

    // Weighted "preferred next degree" table, indexed by current degree
    // (0-4, matching ChordScale's 5 pentatonic-native chords — not
    // classical roman-numeral functional harmony, since a 5-note scale
    // doesn't really support that vocabulary). Every degree favors
    // returning to degree 0 (the most "grounded" chord, built on the
    // scale's own root) most strongly, with lighter weight toward its
    // neighbors — a simple, directed motion rather than picking uniformly
    // at random.
    using Transition = std::pair<int, float>;
    static constexpr std::array<std::array<Transition, 3>, 5> kTransitions{{
        {{{1, 1.0f}, {3, 1.0f}, {4, 0.8f}}},  // degree 0 -> 1, 3, 4
        {{{0, 1.2f}, {2, 0.8f}, {4, 0.5f}}},  // degree 1 -> 0, 2, 4
        {{{0, 1.0f}, {1, 0.6f}, {3, 0.6f}}},  // degree 2 -> 0, 1, 3
        {{{0, 1.2f}, {4, 0.8f}, {2, 0.5f}}},  // degree 3 -> 0, 4, 2
        {{{0, 1.4f}, {3, 0.6f}, {1, 0.4f}}},  // degree 4 -> 0, 3, 1
    }};

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
    float pendingPitchJitterSemitones_ = 0.0f;
    int pendingDegree_ = 0;
    int currentDegree_ = 0;
};
