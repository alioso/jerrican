#include <cassert>
#include <cmath>
#include <iostream>

#include "EvolutionEngine.h"
#include "VoiceModel.h"

namespace {

constexpr double kSampleRate = 44100.0;

VoiceModel makeTestVoice() {
    return VoiceModel("Test", true, 0.8f, 0.4f, 0.6f, 0.5f, 0.3f, 0.4f, 0.6f);
}

}  // namespace

int main() {
    // At amount == 0, update() is provably inert: nothing about the voice
    // changes no matter how many times it's called.
    {
        VoiceModel voice = makeTestVoice();
        EvolutionEngine engine(1u);
        engine.setSampleRate(kSampleRate);
        engine.resetTo(0.5f, 0.2f, 0.7f, 0.5f, 0.3f, 0.4f, 0.6f);

        for (int i = 0; i < static_cast<int>(kSampleRate) * 2; ++i) {
            engine.update(voice, 0.0f, 1.0f);
        }

        assert(voice.getPitchRangeLow() == 0.4f);
        assert(voice.getPitchRangeHigh() == 0.6f);
        assert(voice.getVolume() == 0.8f);
        assert(voice.getTimbre() == 0.5f);
        assert(voice.getGroove() == 0.3f);
        assert(voice.getWander() == 0.4f);
        assert(voice.getDissonance() == 0.6f);
    }

    // At amount == speed == 1, over enough samples every macro (including
    // Volume, now tracked too) must actually move.
    {
        VoiceModel voice = makeTestVoice();
        EvolutionEngine engine(2u);
        engine.setSampleRate(kSampleRate);
        engine.resetTo(0.5f, 0.2f, 0.7f, 0.5f, 0.3f, 0.4f, 0.6f);

        bool volumeChanged = false;
        bool timbreChanged = false;
        bool grooveChanged = false;
        bool wanderChanged = false;
        bool pitchRangeChanged = false;
        bool dissonanceChanged = false;

        for (int i = 0; i < static_cast<int>(kSampleRate) * 5; ++i) {
            engine.update(voice, 1.0f, 1.0f);
            if (voice.getVolume() != 0.8f) volumeChanged = true;
            if (voice.getTimbre() != 0.5f) timbreChanged = true;
            if (voice.getGroove() != 0.3f) grooveChanged = true;
            if (voice.getWander() != 0.4f) wanderChanged = true;
            if (voice.getDissonance() != 0.6f) dissonanceChanged = true;
            if (voice.getPitchRangeLow() != 0.4f || voice.getPitchRangeHigh() != 0.6f) {
                pitchRangeChanged = true;
            }
        }

        assert(volumeChanged);
        assert(timbreChanged);
        assert(grooveChanged);
        assert(wanderChanged);
        assert(dissonanceChanged);
        assert(pitchRangeChanged);
    }

    // Pitch range width stays constant — only the center should move.
    {
        VoiceModel voice = makeTestVoice();
        EvolutionEngine engine(3u);
        engine.setSampleRate(kSampleRate);
        engine.resetTo(0.5f, 0.2f, 0.7f, 0.5f, 0.3f, 0.4f, 0.6f);

        for (int i = 0; i < static_cast<int>(kSampleRate) * 5; ++i) {
            engine.update(voice, 1.0f, 1.0f);
            const float width = voice.getPitchRangeHigh() - voice.getPitchRangeLow();
            assert(width > 0.19f && width < 0.21f);
        }
    }

    // Speed is a huge, exponential range: at the same Amount and the same
    // random sequence (identical seeds), speed=1 must move a parameter
    // dramatically more than speed=0 over the same duration — and speed=0
    // (the slowest setting) should barely move it at all.
    {
        VoiceModel voiceSlow = makeTestVoice();
        EvolutionEngine engineSlow(4u);
        engineSlow.setSampleRate(kSampleRate);
        engineSlow.resetTo(0.5f, 0.2f, 0.7f, 0.5f, 0.3f, 0.4f, 0.6f);

        VoiceModel voiceFast = makeTestVoice();
        EvolutionEngine engineFast(4u);
        engineFast.setSampleRate(kSampleRate);
        engineFast.resetTo(0.5f, 0.2f, 0.7f, 0.5f, 0.3f, 0.4f, 0.6f);

        for (int i = 0; i < static_cast<int>(kSampleRate) * 5; ++i) {
            engineSlow.update(voiceSlow, 1.0f, 0.0f);
            engineFast.update(voiceFast, 1.0f, 1.0f);
        }

        const float slowMovement = std::abs(voiceSlow.getTimbre() - 0.5f);
        const float fastMovement = std::abs(voiceFast.getTimbre() - 0.5f);
        assert(slowMovement < 0.05f);
        assert(fastMovement > slowMovement);
    }

    // Per-parameter opt-out: disabling Timbre stops it from ever moving,
    // even at amount=speed=1, while every other (still-enabled) macro
    // keeps evolving normally.
    {
        VoiceModel voice = makeTestVoice();
        EvolutionEngine engine(5u);
        engine.setSampleRate(kSampleRate);
        engine.resetTo(0.5f, 0.2f, 0.7f, 0.5f, 0.3f, 0.4f, 0.6f);
        engine.setTimbreEnabled(false);

        bool grooveChanged = false;

        for (int i = 0; i < static_cast<int>(kSampleRate) * 5; ++i) {
            engine.update(voice, 1.0f, 1.0f);
            if (voice.getGroove() != 0.3f) grooveChanged = true;
        }

        assert(voice.getTimbre() == 0.5f);
        assert(grooveChanged);
    }

    // Re-enabling a disabled parameter resyncs it to the live value first
    // (resyncTimbre), so it doesn't jump to a stale target picked while it
    // was disabled.
    {
        VoiceModel voice = makeTestVoice();
        EvolutionEngine engine(6u);
        engine.setSampleRate(kSampleRate);
        engine.resetTo(0.5f, 0.2f, 0.7f, 0.5f, 0.3f, 0.4f, 0.6f);
        engine.setTimbreEnabled(false);

        for (int i = 0; i < static_cast<int>(kSampleRate) * 5; ++i) {
            engine.update(voice, 1.0f, 1.0f);
        }
        assert(voice.getTimbre() == 0.5f);

        voice.setTimbre(0.9f);
        engine.resyncTimbre(0.9f);
        engine.setTimbreEnabled(true);

        // Immediately after resync + re-enable, one more update() call
        // shouldn't have jumped it away from 0.9 by more than a tiny
        // smoothing step.
        engine.update(voice, 1.0f, 1.0f);
        assert(std::abs(voice.getTimbre() - 0.9f) < 0.05f);
    }

    // The eight is*Enabled() getters track their matching setters — used by
    // both MIDI Learn (to flip-and-resync) and the per-voice Evolution
    // toggle LEDs (to reflect an externally-driven change).
    {
        EvolutionEngine engine(7u);
        assert(engine.isVolumeEnabled());
        assert(engine.isPitchRangeEnabled());
        assert(engine.isTimbreEnabled());
        assert(engine.isGrooveEnabled());
        assert(engine.isWanderEnabled());
        assert(engine.isDissonanceEnabled());
        assert(engine.isBusyEnabled());
        assert(engine.isSustainEnabled());

        engine.setVolumeEnabled(false);
        engine.setPitchRangeEnabled(false);
        engine.setTimbreEnabled(false);
        engine.setGrooveEnabled(false);
        engine.setWanderEnabled(false);
        engine.setDissonanceEnabled(false);
        engine.setBusyEnabled(false);
        engine.setSustainEnabled(false);

        assert(!engine.isVolumeEnabled());
        assert(!engine.isPitchRangeEnabled());
        assert(!engine.isTimbreEnabled());
        assert(!engine.isGrooveEnabled());
        assert(!engine.isWanderEnabled());
        assert(!engine.isDissonanceEnabled());
        assert(!engine.isBusyEnabled());
        assert(!engine.isSustainEnabled());
    }

    // Busy/Sustain — the two macros added for Bass — evolve the same way
    // every other macro does (part of the data-driven refactor's
    // regression guard), even for a non-Bass voice where they're unused.
    {
        VoiceModel voice = makeTestVoice();
        EvolutionEngine engine(8u);
        engine.setSampleRate(kSampleRate);
        engine.resetTo(0.5f, 0.2f, 0.7f, 0.5f, 0.3f, 0.4f, 0.6f, 0.5f, 0.5f);

        bool busyChanged = false;
        bool sustainChanged = false;
        for (int i = 0; i < static_cast<int>(kSampleRate) * 5; ++i) {
            engine.update(voice, 1.0f, 1.0f);
            if (voice.getBusy() != 0.5f) busyChanged = true;
            if (voice.getSustain() != 0.5f) sustainChanged = true;
        }

        assert(busyChanged);
        assert(sustainChanged);
    }

    std::cout << "EvolutionEngine tests passed" << std::endl;
    return 0;
}
