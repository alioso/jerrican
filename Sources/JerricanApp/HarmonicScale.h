#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "ModeTable.h"

// Quantizes a normalized pitch (0..1) to the nearest degree of the
// currently selected Mode (see ModeTable.h — default Pentatonic, the
// original hardcoded scale every voice used before Mode existed), rooted
// by default at the same A that pitch 0.0 already maps to in
// VoiceOscillator's 55Hz-880Hz (exactly 4-octave) mapping — shifted by
// rootSemitoneOffset (0=A, 1=A#, ... 11=G#) for callers that want a
// different per-voice key. At the same root (the default, 0),
// independently-drawn pitches across different voices still land on
// notes that harmonize with each other; giving each voice its own root
// instead lets them harmonize deliberately, as different degrees of a
// chord, rather than only ever landing on identical pitch classes.
//
// Mode and Key are deliberately orthogonal: one HarmonicScale instance is
// shared by every voice (Mode is a single global choice, changed via
// setMode — see JerricanAudioProcessor::applyModeChange, which queues it
// off the audio thread the same way meter changes already are), while
// rootSemitoneOffset is passed in fresh per call (Key stays per-voice).
class HarmonicScale {
public:
    HarmonicScale() { setMode(0); }

    // Rebuilds the candidate-semitone table for the given entry in
    // ModeTable::kModes — expands its interval pattern across 4 octaves,
    // the same shape the original hardcoded-pentatonic table had. This
    // is an O(28) rebuild, not something to call from the audio thread's
    // hot per-sample path; callers queue a change and apply it once per
    // block, mirroring how meter changes are already handled.
    void setMode(int modeIndex) {
        const auto& mode = ModeTable::kModes[static_cast<std::size_t>(std::max(
            0, std::min(static_cast<int>(ModeTable::kModes.size()) - 1, modeIndex)))];
        degreeCount_ = mode.length;
        for (int i = 0; i < mode.length; ++i) {
            degreeSemitones_[static_cast<std::size_t>(i)] =
                static_cast<float>(mode.semitones[static_cast<std::size_t>(i)]);
        }

        candidateCount_ = 0;
        for (int octave = 0; octave < kOctaveCount; ++octave) {
            for (int i = 0; i < mode.length; ++i) {
                const float semitone = static_cast<float>(mode.semitones[static_cast<std::size_t>(i)]) +
                                       static_cast<float>(octave * 12);
                candidates_[static_cast<std::size_t>(candidateCount_)] = semitone;
                ++candidateCount_;
            }
        }
        // The top root, matching the original table's "plus the top root".
        candidates_[static_cast<std::size_t>(candidateCount_)] = semitonesPerRange;
        ++candidateCount_;
    }

    // How many scale degrees the current mode has (5, 6, or 7) — needed
    // by ChordScale to build chords generically for any mode length,
    // rather than assuming a fixed size.
    int degreeCount() const { return degreeCount_; }

    // Semitone offset for the given scale degree, wrapping degree
    // indices past degreeCount() by whole octaves (e.g. degree ==
    // degreeCount() is the same pitch class one octave above degree 0).
    // Used by ChordScale to stack alternating scale degrees into a
    // chord — the same principle regardless of whether the mode has 5,
    // 6, or 7 notes.
    float semitoneForDegree(int degree) const {
        const int wrapped = ((degree % degreeCount_) + degreeCount_) % degreeCount_;
        const int octaves = (degree - wrapped) / degreeCount_;
        return degreeSemitones_[static_cast<std::size_t>(wrapped)] + static_cast<float>(octaves * 12);
    }

    float quantize(float normalizedPitch, int rootSemitoneOffset = 0) const {
        const float semitone =
            std::max(0.0f, std::min(1.0f, normalizedPitch)) * semitonesPerRange;

        float best = 0.0f;
        float bestDistance = std::numeric_limits<float>::max();
        for (int i = 0; i < candidateCount_; ++i) {
            // Shift each candidate by the root, then fold anything that
            // lands outside the 4-octave range back in by one octave —
            // the mode's pattern repeats every 12 semitones, so this
            // keeps the shifted set covering the same [0, 48] span
            // rather than spilling off one edge.
            float shifted = candidates_[static_cast<std::size_t>(i)] +
                            static_cast<float>(rootSemitoneOffset);
            if (shifted > semitonesPerRange) {
                shifted -= 12.0f;
            } else if (shifted < 0.0f) {
                shifted += 12.0f;
            }

            const float distance = std::abs(semitone - shifted);
            if (distance < bestDistance) {
                bestDistance = distance;
                best = shifted;
            }
        }

        return std::max(0.0f, std::min(1.0f, best / semitonesPerRange));
    }

private:
    // 48 semitones = 4 octaves, matching VoiceOscillator's 55Hz->880Hz
    // (16x = 2^4) frequency span exactly.
    static constexpr float semitonesPerRange = 48.0f;
    static constexpr int kOctaveCount = 4;
    // Worst case (a 7-note mode): 4 octaves * 7 notes + the top root.
    static constexpr std::size_t kMaxCandidates =
        static_cast<std::size_t>(kOctaveCount) * ModeTable::kMaxNotes + 1;

    std::array<float, kMaxCandidates> candidates_{};
    int candidateCount_ = 0;

    std::array<float, ModeTable::kMaxNotes> degreeSemitones_{};
    int degreeCount_ = 0;
};
