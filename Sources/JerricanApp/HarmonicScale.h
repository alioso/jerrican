#pragma once

#include <algorithm>
#include <array>
#include <cmath>

// Quantizes a normalized pitch (0..1) to the nearest degree of a fixed,
// deliberately safe/consonant scale: A major pentatonic, rooted at the
// same A that pitch 0.0 already maps to in VoiceOscillator's 55Hz-880Hz
// (exactly 4-octave) mapping. Shared by every voice — not user-facing —
// so that at low Dissonance, independently-drawn pitches across different
// voices still land on notes that harmonize with each other, rather than
// each voice merely being consonant with itself.
class HarmonicScale {
public:
    static float quantize(float normalizedPitch) {
        const float semitone =
            std::max(0.0f, std::min(1.0f, normalizedPitch)) * semitonesPerRange;

        float best = candidateSemitones.front();
        float bestDistance = std::abs(semitone - best);
        for (float candidate : candidateSemitones) {
            const float distance = std::abs(semitone - candidate);
            if (distance < bestDistance) {
                bestDistance = distance;
                best = candidate;
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
