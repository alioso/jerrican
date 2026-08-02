#include <cassert>
#include <iostream>

#include "GrainCloud.h"

namespace {

constexpr double kSampleRate = 44100.0;

}  // namespace

int main() {
    // Spawning: with complexity > 0 and enough samples, grains must become
    // audible (i.e. the cloud stops being silent).
    {
        GrainCloud cloud(1u);
        cloud.setSampleRate(kSampleRate);

        bool everAudible = false;
        for (int i = 0; i < static_cast<int>(kSampleRate); ++i) {
            const auto sample = cloud.renderSample(0.3f, 0.7f, 0.5f, 0.5f, 1.0f, 1.0f);
            if (sample.left != 0.0f || sample.right != 0.0f) {
                everAudible = true;
            }
        }
        assert(everAudible);
    }

    // With complexity == 0, no new grains ever spawn, so the cloud stays
    // silent indefinitely.
    {
        GrainCloud cloud(2u);
        cloud.setSampleRate(kSampleRate);

        bool everAudible = false;
        for (int i = 0; i < static_cast<int>(kSampleRate); ++i) {
            const auto sample = cloud.renderSample(0.3f, 0.7f, 0.5f, 0.5f, 0.0f, 1.0f);
            if (sample.left != 0.0f || sample.right != 0.0f) {
                everAudible = true;
            }
        }
        assert(!everAudible);
    }

    // Output stays within a sane bounded range even with grains overlapping.
    {
        GrainCloud cloud(3u);
        cloud.setSampleRate(kSampleRate);

        for (int i = 0; i < static_cast<int>(kSampleRate) * 2; ++i) {
            const auto sample = cloud.renderSample(0.0f, 1.0f, 0.5f, 1.0f, 1.0f, 1.0f);
            assert(sample.left >= -1.0f && sample.left <= 1.0f);
            assert(sample.right >= -1.0f && sample.right <= 1.0f);
        }
    }

    // Grains eventually terminate: after spawning for a while, then running
    // with complexity == 0 for long enough that even the longest grain
    // (250ms) must have finished, the cloud must fall silent again.
    {
        GrainCloud cloud(4u);
        cloud.setSampleRate(kSampleRate);

        for (int i = 0; i < static_cast<int>(kSampleRate); ++i) {
            cloud.renderSample(0.3f, 0.7f, 0.5f, 0.5f, 1.0f, 1.0f);
        }

        bool audibleAfterDecay = false;
        for (int i = 0; i < static_cast<int>(kSampleRate); ++i) {
            const auto sample = cloud.renderSample(0.3f, 0.7f, 0.5f, 0.5f, 0.0f, 1.0f);
            if (i > static_cast<int>(kSampleRate) / 2 && (sample.left != 0.0f || sample.right != 0.0f)) {
                audibleAfterDecay = true;
            }
        }
        assert(!audibleAfterDecay);
    }

    std::cout << "GrainCloud tests passed" << std::endl;
    return 0;
}
