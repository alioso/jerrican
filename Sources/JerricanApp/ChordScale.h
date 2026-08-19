#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "FastRandom.h"
#include "HarmonicScale.h"

// Pentatonic-native chord tables for Spark's keyboard voice — a sibling to
// HarmonicScale.h, not an extension of it, but deliberately built to sit
// in the exact same 5-note scale HarmonicScale locks every other voice to
// (major pentatonic: scale degrees 0,2,4,7,9 semitones from the root,
// repeating every octave). A first version of this file built standard
// 7-note-major-scale diatonic 7th chords (I maj7, IV maj7, V dom7, etc.) —
// several of those (ii, IV, V, vii) need the scale's 4th or major-7th
// degree, notes that flatly don't exist in the pentatonic set every other
// voice is constrained to, so Spark read as "out of key" against the rest
// of the ensemble no matter what else was tuned. Building chords directly
// out of pentatonic scale steps instead guarantees every chord tone is
// already a note the other voices could land on too.
//
// Reuses HarmonicScale's exact conventions: normalizedPitch [0,1] <->
// semitone [0,48], 4 octaves matching VoiceOscillator's 55Hz-880Hz span,
// root-shift + octave-fold.
class ChordScale {
public:
    // Degree: 0-4, one per pentatonic scale step. Each chord stacks every
    // other pentatonic scale degree (indices d, d+2, d+4 within the 5-note
    // scale, wrapping with an octave add) — the pentatonic-native
    // equivalent of stacking thirds in a 7-note scale, and the natural
    // reason real pentatonic harmony leans toward these open, quartal-ish
    // shapes rather than classical major/minor triads.
    static std::array<float, 4> chordTones(int degree, bool /*seventh*/, int rootSemitoneOffset,
                                            float thickness, float dissonance,
                                            FastRandom& random) {
        const auto& stack =
            kPentatonicStacks[static_cast<std::size_t>(std::max(0, std::min(4, degree)))];

        // 4th voice: the root doubled an octave up — same "add a 4th
        // voice without changing the chord's actual color" move as
        // before, now applied on top of a pentatonic-native triad.
        std::array<float, 4> semitones = {stack[0], stack[1], stack[2], stack[0] + 12.0f};

        // Centers the (otherwise low, 0-21 semitone) stack roughly in the
        // middle of the 4-octave [0,48] span.
        for (float& s : semitones) {
            s += kBaseOctaveOffset;
        }

        // Thickness: register spread. 0 = close voicing (as tabled above),
        // 1 = root pulled down and the top voice pushed up an octave each
        // — an open/spread voicing, wider in register. Middle voices stay
        // put; the spread happens at the edges, same as real open-voicing
        // piano/organ technique. A whole-octave shift preserves pitch
        // class exactly, so this never pulls a tone off the pentatonic
        // scale on its own.
        const float clampedThickness = std::max(0.0f, std::min(1.0f, thickness));
        semitones[0] -= 12.0f * clampedThickness;
        semitones[3] += 12.0f * clampedThickness;

        std::array<float, 4> result;
        for (std::size_t i = 0; i < 4; ++i) {
            // Root-shift + octave-fold, then run through the exact same
            // HarmonicScale::quantize every other voice's pitches pass
            // through — since the stack above is already pentatonic-
            // native this is normally a no-op (confirms it stays on-scale
            // rather than just hoping the arithmetic above got it right),
            // but it's what makes the guarantee airtight rather than
            // assumed.
            float shifted = semitones[i];
            while (shifted > semitonesPerRange) {
                shifted -= 12.0f;
            }
            while (shifted < 0.0f) {
                shifted += 12.0f;
            }
            const float normalized = std::max(0.0f, std::min(1.0f, shifted / semitonesPerRange));
            const float quantized = HarmonicScale::quantize(normalized, rootSemitoneOffset);

            // Dissonance: a small chromatic jitter per chord tone, same
            // "0 = strictly on-target, 1 = free to drift" spirit every
            // other voice's Dissonance already has.
            const float clampedDissonance = std::max(0.0f, std::min(1.0f, dissonance));
            const float jitterSemitones = (random.nextFloat01() * 2.0f - 1.0f) *
                                          kMaxDissonanceJitterSemitones * clampedDissonance;
            result[i] =
                std::max(0.0f, std::min(1.0f, quantized + jitterSemitones / semitonesPerRange));
        }
        return result;
    }

private:
    static constexpr float semitonesPerRange = 48.0f;
    static constexpr float kBaseOctaveOffset = 12.0f;
    static constexpr float kMaxDissonanceJitterSemitones = 2.0f;

    // Pentatonic scale steps: 0, 2, 4, 7, 9 semitones from the root. Each
    // row stacks degrees d, d+2, d+4 (mod 5, +12 semitones per wrap).
    static constexpr std::array<std::array<float, 3>, 5> kPentatonicStacks{{
        {0.0f, 4.0f, 9.0f},    // degree 0: steps 0,2,4 -> 0,4,9
        {2.0f, 7.0f, 12.0f},   // degree 1: steps 1,3,0(+12) -> 2,7,12
        {4.0f, 9.0f, 14.0f},   // degree 2: steps 2,4,1(+12) -> 4,9,14
        {7.0f, 12.0f, 16.0f},  // degree 3: steps 3,0(+12),2(+12) -> 7,12,16
        {9.0f, 14.0f, 19.0f},  // degree 4: steps 4,1(+12),3(+12) -> 9,14,19
    }};
};
