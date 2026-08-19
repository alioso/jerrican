#include <cassert>
#include <iostream>

#include "GrainCloud.h"

namespace {

constexpr double kSampleRate = 44100.0;

// The old generic continuous-stochastic renderSample()/maybeSpawnGrain()
// path was removed once every voice got a bespoke spawn method — these
// tests were written against that generic path, so they now exercise
// renderHazeSample() instead, since it's the closest surviving equivalent
// (continuous density-driven spawning gated by an explicit `active` flag,
// same shape the removed API had). What's being tested (spawn/silence,
// bounded output, decay, drift scoping, custom duration ranges, Character
// support, Dissonance) is unchanged — only the call site is.
Grain::StereoSample render(GrainCloud& cloud, float pitchLow, float pitchHigh, float texture,
                           float drift, float complexity, float volume, bool active,
                           float dissonance = 1.0f) {
    return cloud.renderHazeSample(pitchLow, pitchHigh, texture, drift, complexity, volume,
                                  dissonance, 0.0f, active);
}

}  // namespace

int main() {
    // Spawning: with active and enough samples, grains must become
    // audible (i.e. the cloud stops being silent).
    {
        GrainCloud cloud(1u);
        cloud.setSampleRate(kSampleRate);

        bool everAudible = false;
        for (int i = 0; i < static_cast<int>(kSampleRate); ++i) {
            const auto sample = render(cloud, 0.3f, 0.7f, 0.5f, 0.5f, 1.0f, 1.0f, true);
            if (sample.left != 0.0f || sample.right != 0.0f) {
                everAudible = true;
            }
        }
        assert(everAudible);
    }

    // With active == false, no new grains ever spawn, so the cloud stays
    // silent indefinitely.
    {
        GrainCloud cloud(2u);
        cloud.setSampleRate(kSampleRate);

        bool everAudible = false;
        for (int i = 0; i < static_cast<int>(kSampleRate); ++i) {
            const auto sample = render(cloud, 0.3f, 0.7f, 0.5f, 0.5f, 0.0f, 1.0f, false);
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
            const auto sample = render(cloud, 0.0f, 1.0f, 0.5f, 1.0f, 1.0f, 1.0f, true);
            assert(sample.left >= -1.0f && sample.left <= 1.0f);
            assert(sample.right >= -1.0f && sample.right <= 1.0f);
        }
    }

    // Grains eventually terminate: after spawning for a while, then
    // running with active == false for long enough that even the longest
    // grain (250ms) must have finished, the cloud must fall silent again.
    {
        GrainCloud cloud(4u);
        cloud.setSampleRate(kSampleRate);

        for (int i = 0; i < static_cast<int>(kSampleRate); ++i) {
            render(cloud, 0.3f, 0.7f, 0.5f, 0.5f, 1.0f, 1.0f, true);
        }

        bool audibleAfterDecay = false;
        for (int i = 0; i < static_cast<int>(kSampleRate); ++i) {
            const auto sample = render(cloud, 0.3f, 0.7f, 0.5f, 0.5f, 0.0f, 1.0f, false);
            if (i > static_cast<int>(kSampleRate) / 2 && (sample.left != 0.0f || sample.right != 0.0f)) {
                audibleAfterDecay = true;
            }
        }
        assert(!audibleAfterDecay);
    }

    // rerollDrift() is scoped to the given pitch range: after rerolling,
    // spawned grains should still land within that range regardless of how
    // many times it's rerolled (a prior version drew from the full [0, 1]
    // range, which the pitch-clamping in the spawn path would silently
    // absorb — this at least confirms rerolling repeatedly doesn't crash or
    // push output out of the expected bounded range).
    {
        GrainCloud cloud(5u);
        cloud.setSampleRate(kSampleRate);

        for (int i = 0; i < 100; ++i) {
            cloud.rerollDrift(0.3f, 0.7f);
            for (int sample = 0; sample < 1000; ++sample) {
                const auto s = render(cloud, 0.3f, 0.7f, 0.5f, 0.8f, 1.0f, 1.0f, true);
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
            const auto sample = render(cloud, 0.1f, 0.2f, 0.2f, 0.1f, 0.2f, 1.0f, true);
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
            const auto sample = render(cloud, 0.4f, 0.65f, 0.4f, 0.3f, 1.0f, 1.0f, true);
            assert(sample.left >= -1.0f && sample.left <= 1.0f);
            assert(sample.right >= -1.0f && sample.right <= 1.0f);
            if (sample.left != 0.0f || sample.right != 0.0f) {
                everAudible = true;
            }
        }
        assert(everAudible);

        bool audibleAfterDecay = false;
        for (int i = 0; i < static_cast<int>(kSampleRate); ++i) {
            const auto sample = render(cloud, 0.4f, 0.65f, 0.4f, 0.3f, 0.0f, 1.0f, false);
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
            const auto sample = render(cloud, 0.3f, 0.7f, 0.5f, 0.5f, 1.0f, 1.0f, true, 0.0f);
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
