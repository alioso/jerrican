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

    std::cout << "HarmonicScale tests passed" << std::endl;
    return 0;
}
