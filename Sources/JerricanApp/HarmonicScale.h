#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

// Quantizes a normalized pitch (0..1) to the nearest degree of a
// deliberately safe/consonant scale: major pentatonic, rooted by default
// at the same A that pitch 0.0 already maps to in VoiceOscillator's
// 55Hz-880Hz (exactly 4-octave) mapping — shifted by rootSemitoneOffset
// (0=A, 1=A#, ... 11=G#) for callers that want a different per-voice
// key. At the same root (the default, 0), independently-drawn pitches
// across different voices still land on notes that harmonize with each
// other; giving each voice its own root instead lets them harmonize
// deliberately, as different degrees of a chord, rather than only ever
// landing on identical pitch classes.
class HarmonicScale {
public:
    static float quantize(float normalizedPitch, int rootSemitoneOffset = 0) {
        const float semitone =
            std::max(0.0f, std::min(1.0f, normalizedPitch)) * semitonesPerRange;

        float best = 0.0f;
        float bestDistance = std::numeric_limits<float>::max();
        for (float candidate : candidateSemitones) {
            // Shift each candidate by the root, then fold anything that
            // lands outside the 4-octave range back in by one octave —
            // the pentatonic pattern repeats every 12 semitones, so this
            // keeps the shifted set covering the same [0, 48] span
            // rather than spilling off one edge.
            float shifted = candidate + static_cast<float>(rootSemitoneOffset);
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

    // A major pentatonic (A, B, C#, E, F#) across all 4 octaves, plus the
    // top root.
    static constexpr std::array<float, 21> candidateSemitones{
        0.0f,  2.0f,  4.0f,  7.0f,  9.0f,  12.0f, 14.0f, 16.0f, 19.0f, 21.0f,
        24.0f, 26.0f, 28.0f, 31.0f, 33.0f, 36.0f, 38.0f, 40.0f, 43.0f, 45.0f, 48.0f};
};
