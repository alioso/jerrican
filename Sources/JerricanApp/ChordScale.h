#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "FastRandom.h"
#include "HarmonicScale.h"

// Chord tables for Keys's keyboard voice — a sibling to HarmonicScale.h,
// not an extension of it, but deliberately built to sit in the exact same
// scale (whichever Mode is currently selected — see ModeTable.h/
// HarmonicScale.h) HarmonicScale locks every other voice to. A first
// version of this file built standard 7-note-major-scale diatonic 7th
// chords (I maj7, IV maj7, V dom7, etc.) hardcoded against the pentatonic
// scale every voice used before Mode existed — several of those chords
// (ii, IV, V, vii) need scale degrees that flatly don't exist in a 5-note
// scale, so Keys read as "out of key" against the rest of the ensemble no
// matter what else was tuned. Building chords directly out of the current
// mode's own scale steps instead guarantees every chord tone is already a
// note the other voices could land on too, regardless of which Mode (5,
// 6, or 7 notes) is selected.
//
// Reuses HarmonicScale's exact conventions: normalizedPitch [0,1] <->
// semitone [0,48], 4 octaves matching VoiceOscillator's 55Hz-880Hz span,
// root-shift + octave-fold.
class ChordScale {
public:
    // Degree: 0..(scale.degreeCount()-1), one per scale step of whichever
    // Mode is currently selected. Each chord stacks every other scale
    // degree (indices d, d+2, d+4, wrapping with an octave add per wrap)
    // — the pentatonic-native origin of this trick was already the
    // generalizable part: skip-alternating-degrees isn't specific to a
    // 5-note scale, for a 7-note scale it naturally produces real
    // tertian 7th chords instead.
    static std::array<float, 4> chordTones(const HarmonicScale& scale, int degree,
                                            bool /*seventh*/, int rootSemitoneOffset,
                                            float thickness, float dissonance,
                                            FastRandom& random) {
        const int degreeCount = std::max(1, scale.degreeCount());
        const int clampedDegree = ((degree % degreeCount) + degreeCount) % degreeCount;
        const float s0 = scale.semitoneForDegree(clampedDegree);
        const float s1 = scale.semitoneForDegree(clampedDegree + 2);
        const float s2 = scale.semitoneForDegree(clampedDegree + 4);

        // 4th voice: the root doubled an octave up — same "add a 4th
        // voice without changing the chord's actual color" move as
        // before, now applied on top of whichever scale is current.
        std::array<float, 4> semitones = {s0, s1, s2, s0 + 12.0f};

        // Centers the (otherwise low) stack roughly in the middle of the
        // 4-octave [0,48] span.
        for (float& s : semitones) {
            s += kBaseOctaveOffset;
        }

        // Thickness: register spread. 0 = close voicing (as stacked
        // above), 1 = root pulled down and the top voice pushed up an
        // octave each — an open/spread voicing, wider in register.
        // Middle voices stay put; the spread happens at the edges, same
        // as real open-voicing piano/organ technique. A whole-octave
        // shift preserves pitch class exactly, so this never pulls a
        // tone off the current scale on its own.
        const float clampedThickness = std::max(0.0f, std::min(1.0f, thickness));
        semitones[0] -= 12.0f * clampedThickness;
        semitones[3] += 12.0f * clampedThickness;

        std::array<float, 4> result;
        for (std::size_t i = 0; i < 4; ++i) {
            // Root-shift + octave-fold, then run through the exact same
            // HarmonicScale::quantize every other voice's pitches pass
            // through — since the stack above is already scale-native
            // this is normally a no-op (confirms it stays on-scale
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
            const float quantized = scale.quantize(normalized, rootSemitoneOffset);

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
};
