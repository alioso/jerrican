#include <cassert>
#include <cmath>
#include <iostream>

#include "HarmonicScale.h"
#include "ModeTable.h"

int main() {
    HarmonicScale scale;  // defaults to mode 0 (Pentatonic)

    // The root (0.0) and the top (1.0, an exact octave-multiple point)
    // quantize to themselves — they're already on the scale.
    assert(std::abs(scale.quantize(0.0f) - 0.0f) < 1e-6f);
    assert(std::abs(scale.quantize(1.0f) - 1.0f) < 1e-6f);

    // Output always stays within [0, 1], even for out-of-range input.
    assert(scale.quantize(-0.5f) >= 0.0f);
    assert(scale.quantize(1.5f) <= 1.0f);

    // A point roughly one semitone above the root (1/48) should snap to
    // the root or the next scale degree (2/48), not stay at an arbitrary
    // off-scale value.
    const float nearRoot = scale.quantize(1.0f / 48.0f);
    assert(nearRoot == 0.0f || std::abs(nearRoot - 2.0f / 48.0f) < 1e-5f);

    // Quantizing an already-quantized value is a no-op (idempotent).
    for (float t = 0.0f; t <= 1.0f; t += 0.05f) {
        const float once = scale.quantize(t);
        const float twice = scale.quantize(once);
        assert(std::abs(once - twice) < 1e-6f);
    }

    // Root offset 0 is exactly today's (unrooted) behavior.
    for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
        assert(scale.quantize(t) == scale.quantize(t, 0));
    }

    // A non-zero root still produces output in [0, 1] across the whole
    // input range, and quantizing an already-quantized value at that same
    // root is still idempotent — the octave-wraparound correction doesn't
    // leave anything unstable at the edges.
    for (int root = 0; root < 12; ++root) {
        for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
            const float once = scale.quantize(t, root);
            assert(once >= 0.0f && once <= 1.0f);
            const float twice = scale.quantize(once, root);
            assert(std::abs(once - twice) < 1e-5f);
        }
    }

    // Different roots generally produce different quantized results for
    // the same input — confirms the offset actually does something
    // (root 7 = E is a perfect fifth from the default root 0 = A).
    bool foundDifference = false;
    for (float t = 0.0f; t <= 1.0f; t += 0.05f) {
        if (std::abs(scale.quantize(t, 0) - scale.quantize(t, 7)) > 1e-4f) {
            foundDifference = true;
            break;
        }
    }
    assert(foundDifference);

    // Every mode in the table behaves consistently: idempotent, bounded
    // to [0, 1], and reports the right degree count — the engine must
    // handle 5-, 6-, and 7-note scales generically, not assume one size.
    for (std::size_t modeIndex = 0; modeIndex < ModeTable::kModes.size(); ++modeIndex) {
        scale.setMode(static_cast<int>(modeIndex));
        assert(scale.degreeCount() == ModeTable::kModes[modeIndex].length);
        for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
            const float once = scale.quantize(t);
            assert(once >= 0.0f && once <= 1.0f);
            const float twice = scale.quantize(once);
            assert(std::abs(once - twice) < 1e-5f);
        }
    }

    // Switching modes actually changes what a given input quantizes to —
    // confirms setMode isn't a no-op. Ionian (index 1, 7 notes) and
    // Hirajoshi (index 16, 5 notes) are different enough from Pentatonic
    // (index 0) that at least one sampled point must land differently.
    scale.setMode(0);
    const float pentatonicSample = scale.quantize(0.3f);
    scale.setMode(1);
    assert(scale.degreeCount() == 7);
    bool differsFromPentatonic = false;
    for (float t = 0.0f; t <= 1.0f; t += 0.02f) {
        scale.setMode(0);
        const float a = scale.quantize(t);
        scale.setMode(1);
        const float b = scale.quantize(t);
        if (std::abs(a - b) > 1e-4f) {
            differsFromPentatonic = true;
            break;
        }
    }
    assert(differsFromPentatonic);
    (void)pentatonicSample;

    // Restore mode 0 explicitly (harmless — just documents the default a
    // fresh instance starts with, in case a future test gets added below
    // this one and assumes it).
    scale.setMode(0);

    std::cout << "HarmonicScale tests passed" << std::endl;
    return 0;
}
