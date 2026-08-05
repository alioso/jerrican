#include <cassert>
#include <cmath>
#include <iostream>

#include "HarmonicScale.h"

int main() {
    // The root (0.0) and the top (1.0, an exact octave-multiple point)
    // quantize to themselves — they're already on the scale.
    assert(std::abs(HarmonicScale::quantize(0.0f) - 0.0f) < 1e-6f);
    assert(std::abs(HarmonicScale::quantize(1.0f) - 1.0f) < 1e-6f);

    // Output always stays within [0, 1], even for out-of-range input.
    assert(HarmonicScale::quantize(-0.5f) >= 0.0f);
    assert(HarmonicScale::quantize(1.5f) <= 1.0f);

    // A point roughly one semitone above the root (1/48) should snap to
    // the root or the next scale degree (2/48), not stay at an arbitrary
    // off-scale value.
    const float nearRoot = HarmonicScale::quantize(1.0f / 48.0f);
    assert(nearRoot == 0.0f || std::abs(nearRoot - 2.0f / 48.0f) < 1e-5f);

    // Quantizing an already-quantized value is a no-op (idempotent).
    for (float t = 0.0f; t <= 1.0f; t += 0.05f) {
        const float once = HarmonicScale::quantize(t);
        const float twice = HarmonicScale::quantize(once);
        assert(std::abs(once - twice) < 1e-6f);
    }

    // Root offset 0 is exactly today's (unrooted) behavior.
    for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
        assert(HarmonicScale::quantize(t) == HarmonicScale::quantize(t, 0));
    }

    // A non-zero root still produces output in [0, 1] across the whole
    // input range, and quantizing an already-quantized value at that same
    // root is still idempotent — the octave-wraparound correction doesn't
    // leave anything unstable at the edges.
    for (int root = 0; root < 12; ++root) {
        for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
            const float once = HarmonicScale::quantize(t, root);
            assert(once >= 0.0f && once <= 1.0f);
            const float twice = HarmonicScale::quantize(once, root);
            assert(std::abs(once - twice) < 1e-5f);
        }
    }

    // Different roots generally produce different quantized results for
    // the same input — confirms the offset actually does something
    // (root 7 = E is a perfect fifth from the default root 0 = A).
    bool foundDifference = false;
    for (float t = 0.0f; t <= 1.0f; t += 0.05f) {
        if (std::abs(HarmonicScale::quantize(t, 0) - HarmonicScale::quantize(t, 7)) > 1e-4f) {
            foundDifference = true;
            break;
        }
    }
    assert(foundDifference);

    std::cout << "HarmonicScale tests passed" << std::endl;
    return 0;
}
