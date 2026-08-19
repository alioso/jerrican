#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

#include "ChordScale.h"
#include "FastRandom.h"
#include "HarmonicScale.h"

int main() {
    FastRandom random(1u);

    // Output always stays within [0, 1], across every degree/root/
    // thickness/dissonance combination, triad and seventh alike.
    for (int degree = 0; degree < 5; ++degree) {
        for (int root = 0; root < 12; ++root) {
            for (float thickness : {0.0f, 0.5f, 1.0f}) {
                for (float dissonance : {0.0f, 0.5f, 1.0f}) {
                    for (bool seventh : {false, true}) {
                        const auto tones = ChordScale::chordTones(degree, seventh, root, thickness,
                                                                   dissonance, random);
                        for (float t : tones) {
                            assert(t >= 0.0f && t <= 1.0f);
                        }
                    }
                }
            }
        }
    }

    // At dissonance=0, chord tones are deterministic (no jitter) — calling
    // twice with the same inputs gives the same result regardless of the
    // random generator's state.
    for (int degree = 0; degree < 5; ++degree) {
        const auto a = ChordScale::chordTones(degree, true, 0, 0.3f, 0.0f, random);
        const auto b = ChordScale::chordTones(degree, true, 0, 0.3f, 0.0f, random);
        for (std::size_t i = 0; i < a.size(); ++i) {
            assert(std::abs(a[i] - b[i]) < 1e-6f);
        }
    }

    // A triad's 4 returned tones are not all identical (root-doubling is
    // an octave apart, not a literal duplicate) — confirms the triad path
    // actually varies register, not just repeating one pitch four times.
    {
        const auto tones = ChordScale::chordTones(0, false, 0, 0.0f, 0.0f, random);
        assert(std::abs(tones[3] - tones[0]) > 1e-3f);
    }

    // Thickness genuinely widens the voicing: the top-minus-bottom span at
    // thickness=1 is wider than at thickness=0, for a fixed degree/root.
    {
        const auto close = ChordScale::chordTones(0, true, 0, 0.0f, 0.0f, random);
        const auto open = ChordScale::chordTones(0, true, 0, 1.0f, 0.0f, random);
        const float closeSpan = *std::max_element(close.begin(), close.end()) -
                                *std::min_element(close.begin(), close.end());
        const float openSpan = *std::max_element(open.begin(), open.end()) -
                               *std::min_element(open.begin(), open.end());
        assert(openSpan >= closeSpan);
    }

    // Different roots generally produce different chord tones for the same
    // degree — confirms rootSemitoneOffset actually does something. Checked
    // across several roots (not just one fixed pair) since two pentatonic
    // scales a specific interval apart can legitimately share a quantized
    // note or two by coincidence for a given degree/thickness — the
    // property being tested is that *some* root produces a difference, not
    // that every specific pair does.
    {
        const auto rootA = ChordScale::chordTones(0, true, 0, 0.3f, 0.0f, random);
        bool foundDifference = false;
        for (int root = 1; root < 12 && !foundDifference; ++root) {
            const auto rootB = ChordScale::chordTones(0, true, root, 0.3f, 0.0f, random);
            for (std::size_t i = 0; i < rootA.size(); ++i) {
                if (std::abs(rootA[i] - rootB[i]) > 1e-4f) {
                    foundDifference = true;
                    break;
                }
            }
        }
        assert(foundDifference);
    }

    // The actual harmony guarantee: at dissonance=0, every chord tone
    // (every degree, root, thickness) is already exactly on the shared
    // pentatonic scale — i.e. re-quantizing it with HarmonicScale is a
    // no-op. This is what makes Keys's chords land in the same key as
    // every other pentatonic-quantized voice; an earlier version built
    // chords from a full 7-note major scale, which needed scale degrees
    // (the 4th, the major 7th) that don't exist in the pentatonic set at
    // all, and read as "out of key" against the rest of the ensemble no
    // matter what else was tuned.
    for (int degree = 0; degree < 5; ++degree) {
        for (int root = 0; root < 12; ++root) {
            for (float thickness : {0.0f, 0.5f, 1.0f}) {
                const auto tones =
                    ChordScale::chordTones(degree, true, root, thickness, 0.0f, random);
                for (float t : tones) {
                    const float requantized = HarmonicScale::quantize(t, root);
                    assert(std::abs(t - requantized) < 1e-4f);
                }
            }
        }
    }

    // Out-of-range degree is clamped, not undefined behavior.
    {
        const auto low = ChordScale::chordTones(-3, true, 0, 0.3f, 0.0f, random);
        const auto high = ChordScale::chordTones(99, true, 0, 0.3f, 0.0f, random);
        for (float t : low) assert(t >= 0.0f && t <= 1.0f);
        for (float t : high) assert(t >= 0.0f && t <= 1.0f);
    }

    std::cout << "ChordScale tests passed" << std::endl;
    return 0;
}
