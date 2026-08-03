#include <cassert>
#include <iostream>

#include "EvolutionEngine.h"
#include "VoiceModel.h"

namespace {

constexpr double kSampleRate = 44100.0;

}  // namespace

int main() {
    // At evolution == 0, update() is provably inert: nothing about the
    // voice changes no matter how many times it's called.
    {
        VoiceModel voice("Test", true, 0.8f, 0.4f, 0.6f, 0.5f, 0.3f, 0.4f, 0.6f);
        EvolutionEngine engine(1u);
        engine.setSampleRate(kSampleRate);
        engine.resetTo(0.5f, 0.2f, 0.5f, 0.3f, 0.4f, 0.6f);

        for (int i = 0; i < static_cast<int>(kSampleRate) * 2; ++i) {
            engine.update(voice, 0.0f);
        }

        assert(voice.getPitchRangeLow() == 0.4f);
        assert(voice.getPitchRangeHigh() == 0.6f);
        assert(voice.getTimbre() == 0.5f);
        assert(voice.getMotion() == 0.3f);
        assert(voice.getComplexity() == 0.4f);
        assert(voice.getDissonance() == 0.6f);
    }

    // At evolution == 1, over enough samples the macros must actually move.
    {
        VoiceModel voice("Test", true, 0.8f, 0.4f, 0.6f, 0.5f, 0.3f, 0.4f, 0.6f);
        EvolutionEngine engine(2u);
        engine.setSampleRate(kSampleRate);
        engine.resetTo(0.5f, 0.2f, 0.5f, 0.3f, 0.4f, 0.6f);

        bool timbreChanged = false;
        bool motionChanged = false;
        bool complexityChanged = false;
        bool pitchRangeChanged = false;
        bool dissonanceChanged = false;

        for (int i = 0; i < static_cast<int>(kSampleRate) * 5; ++i) {
            engine.update(voice, 1.0f);
            if (voice.getTimbre() != 0.5f) timbreChanged = true;
            if (voice.getMotion() != 0.3f) motionChanged = true;
            if (voice.getComplexity() != 0.4f) complexityChanged = true;
            if (voice.getDissonance() != 0.6f) dissonanceChanged = true;
            if (voice.getPitchRangeLow() != 0.4f || voice.getPitchRangeHigh() != 0.6f) {
                pitchRangeChanged = true;
            }
        }

        assert(timbreChanged);
        assert(motionChanged);
        assert(complexityChanged);
        assert(dissonanceChanged);
        assert(pitchRangeChanged);
    }

    // Pitch range width stays constant — only the center should move.
    {
        VoiceModel voice("Test", true, 0.8f, 0.4f, 0.6f, 0.5f, 0.3f, 0.4f, 0.6f);
        EvolutionEngine engine(3u);
        engine.setSampleRate(kSampleRate);
        engine.resetTo(0.5f, 0.2f, 0.5f, 0.3f, 0.4f, 0.6f);

        for (int i = 0; i < static_cast<int>(kSampleRate) * 5; ++i) {
            engine.update(voice, 1.0f);
            const float width = voice.getPitchRangeHigh() - voice.getPitchRangeLow();
            assert(width > 0.19f && width < 0.21f);
        }
    }

    std::cout << "EvolutionEngine tests passed" << std::endl;
    return 0;
}
