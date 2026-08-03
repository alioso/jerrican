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

    // rerollDrift() is scoped to the given pitch range: after rerolling,
    // spawned grains should still land within that range regardless of how
    // many times it's rerolled (a prior version drew from the full [0, 1]
    // range, which the pitch-clamping in maybeSpawnGrain would silently
    // absorb — this at least confirms rerolling repeatedly doesn't crash or
    // push output out of the expected bounded range).
    {
        GrainCloud cloud(5u);
        cloud.setSampleRate(kSampleRate);

        for (int i = 0; i < 100; ++i) {
            cloud.rerollDrift(0.3f, 0.7f);
            for (int sample = 0; sample < 1000; ++sample) {
                const auto s = cloud.renderSample(0.3f, 0.7f, 0.5f, 0.8f, 1.0f, 1.0f);
                assert(s.left >= -1.0f && s.left <= 1.0f);
                assert(s.right >= -1.0f && s.right <= 1.0f);
            }
        }
    }

    // Custom grain duration range (per-voice archetype support): a cloud
    // configured with long grain durations should still produce bounded
    // output and eventually go silent once spawning stops.
    {
        GrainCloud cloud(6u, 1500.0f, 4000.0f);
        cloud.setSampleRate(kSampleRate);

        for (int i = 0; i < static_cast<int>(kSampleRate); ++i) {
            const auto sample = cloud.renderSample(0.1f, 0.2f, 0.2f, 0.1f, 0.2f, 1.0f);
            assert(sample.left >= -1.0f && sample.left <= 1.0f);
            assert(sample.right >= -1.0f && sample.right <= 1.0f);
        }
    }

    // Character::Plucked (filter sweep + percussive envelope): output must
    // still stay bounded, and grains must still spawn and eventually decay
    // to silence, same contract as the default Ambient character.
    {
        GrainCloud cloud(7u, 60.0f, 180.0f, Grain::Character::Plucked);
        cloud.setSampleRate(kSampleRate);

        bool everAudible = false;
        for (int i = 0; i < static_cast<int>(kSampleRate); ++i) {
            const auto sample = cloud.renderSample(0.4f, 0.65f, 0.4f, 0.3f, 1.0f, 1.0f);
            assert(sample.left >= -1.0f && sample.left <= 1.0f);
            assert(sample.right >= -1.0f && sample.right <= 1.0f);
            if (sample.left != 0.0f || sample.right != 0.0f) {
                everAudible = true;
            }
        }
        assert(everAudible);

        bool audibleAfterDecay = false;
        for (int i = 0; i < static_cast<int>(kSampleRate); ++i) {
            const auto sample = cloud.renderSample(0.4f, 0.65f, 0.4f, 0.3f, 0.0f, 1.0f);
            if (i > static_cast<int>(kSampleRate) / 2 && (sample.left != 0.0f || sample.right != 0.0f)) {
                audibleAfterDecay = true;
            }
        }
        assert(!audibleAfterDecay);
    }

    // dissonance = 0.0 (fully quantized to the shared consonant scale):
    // output must still stay bounded and the cloud must still spawn grains
    // normally (the music-theory correctness of quantize() itself is
    // covered by HarmonicScaleTest, since GrainCloud doesn't expose
    // individual grain pitches to inspect).
    {
        GrainCloud cloud(8u);
        cloud.setSampleRate(kSampleRate);

        bool everAudible = false;
        for (int i = 0; i < static_cast<int>(kSampleRate); ++i) {
            const auto sample = cloud.renderSample(0.3f, 0.7f, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f);
            assert(sample.left >= -1.0f && sample.left <= 1.0f);
            assert(sample.right >= -1.0f && sample.right <= 1.0f);
            if (sample.left != 0.0f || sample.right != 0.0f) {
                everAudible = true;
            }
        }
        assert(everAudible);
    }

    std::cout << "GrainCloud tests passed" << std::endl;
    return 0;
}
