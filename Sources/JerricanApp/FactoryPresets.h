#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

// The presets a fresh install (a genuinely empty Presets/ directory —
// no prior installation to migrate from) is seeded with, so a release
// build handed to someone else has something real to load rather than
// an empty Presets menu. Content is a snapshot of an actual tuned
// preset (Calm), in the exact key=value text PresetStore already reads/
// writes — same format, just embedded here instead of read from disk.
inline std::vector<std::pair<std::string, std::string>> factoryPresets() {
    return {
        {"Calm",
         "voice0.enabled=1\n"
         "voice0.volume=0.368313\n"
         "voice0.pitchLow=0\n"
         "voice0.pitchHigh=0.418716\n"
         "voice0.timbre=0.314078\n"
         "voice0.motion=0.185438\n"
         "voice0.complexity=0.295703\n"
         "voice0.dissonance=0\n"
         "voice0.rootSemitoneOffset=3\n"
         "voice0.busy=0.161281\n"
         "voice0.sustain=0.767125\n"
         "voice0.cleanliness=0.5\n"
         "voice0.attack=0.723984\n"
         "voice0.volumeEvoEnabled=1\n"
         "voice0.pitchRangeEvoEnabled=1\n"
         "voice0.timbreEvoEnabled=1\n"
         "voice0.motionEvoEnabled=1\n"
         "voice0.complexityEvoEnabled=1\n"
         "voice0.dissonanceEvoEnabled=1\n"
         "voice0.busyEvoEnabled=1\n"
         "voice0.sustainEvoEnabled=1\n"
         "voice0.cleanlinessEvoEnabled=1\n"
         "voice0.attackEvoEnabled=1\n"
         "voice1.enabled=1\n"
         "voice1.volume=0.371172\n"
         "voice1.pitchLow=0.296498\n"
         "voice1.pitchHigh=0.959443\n"
         "voice1.timbre=0\n"
         "voice1.motion=0.401031\n"
         "voice1.complexity=0.809813\n"
         "voice1.dissonance=0\n"
         "voice1.rootSemitoneOffset=3\n"
         "voice1.busy=0.5\n"
         "voice1.sustain=0.5\n"
         "voice1.cleanliness=0.84875\n"
         "voice1.attack=0.5\n"
         "voice1.volumeEvoEnabled=1\n"
         "voice1.pitchRangeEvoEnabled=1\n"
         "voice1.timbreEvoEnabled=1\n"
         "voice1.motionEvoEnabled=1\n"
         "voice1.complexityEvoEnabled=1\n"
         "voice1.dissonanceEvoEnabled=1\n"
         "voice1.busyEvoEnabled=1\n"
         "voice1.sustainEvoEnabled=1\n"
         "voice1.cleanlinessEvoEnabled=1\n"
         "voice1.attackEvoEnabled=1\n"
         "voice2.enabled=0\n"
         "voice2.volume=0.752593\n"
         "voice2.pitchLow=0.100469\n"
         "voice2.pitchHigh=0.396408\n"
         "voice2.timbre=0.114609\n"
         "voice2.motion=0.505828\n"
         "voice2.complexity=0.125906\n"
         "voice2.dissonance=0\n"
         "voice2.rootSemitoneOffset=7\n"
         "voice2.busy=0.5\n"
         "voice2.sustain=0.5\n"
         "voice2.cleanliness=0.5\n"
         "voice2.attack=0.5\n"
         "voice2.volumeEvoEnabled=1\n"
         "voice2.pitchRangeEvoEnabled=1\n"
         "voice2.timbreEvoEnabled=1\n"
         "voice2.motionEvoEnabled=1\n"
         "voice2.complexityEvoEnabled=1\n"
         "voice2.dissonanceEvoEnabled=1\n"
         "voice2.busyEvoEnabled=1\n"
         "voice2.sustainEvoEnabled=1\n"
         "voice2.cleanlinessEvoEnabled=1\n"
         "voice2.attackEvoEnabled=1\n"
         "voice3.enabled=1\n"
         "voice3.volume=0.0278281\n"
         "voice3.pitchLow=0.15\n"
         "voice3.pitchHigh=0.658376\n"
         "voice3.timbre=0.60786\n"
         "voice3.motion=0.497844\n"
         "voice3.complexity=0.15\n"
         "voice3.dissonance=0.0268438\n"
         "voice3.rootSemitoneOffset=3\n"
         "voice3.busy=0.5\n"
         "voice3.sustain=0.5\n"
         "voice3.cleanliness=0.5\n"
         "voice3.attack=0.218734\n"
         "voice3.volumeEvoEnabled=1\n"
         "voice3.pitchRangeEvoEnabled=1\n"
         "voice3.timbreEvoEnabled=1\n"
         "voice3.motionEvoEnabled=1\n"
         "voice3.complexityEvoEnabled=1\n"
         "voice3.dissonanceEvoEnabled=1\n"
         "voice3.busyEvoEnabled=1\n"
         "voice3.sustainEvoEnabled=1\n"
         "voice3.cleanlinessEvoEnabled=1\n"
         "voice3.attackEvoEnabled=1\n"
         "global.evolutionAmount=0\n"
         "global.evolutionSpeed=0.5\n"
         "global.reverbRoom=0.123844\n"
         "global.reverbDecay=0.460937\n"
         "global.masterVolume=1\n"
         "global.tempo=120\n"
         "global.meterNumerator=4\n"
         "global.meterDenominator=4\n"},
    };
}

// True for any preset name that ships baked into the install (see
// factoryPresets() above) — used to keep Override/Delete off-limits for
// them in the Presets popup, so the shipped starting point can't be lost
// by mistake; Save As under a new name is always still available.
inline bool isFactoryPresetName(const std::string& name) {
    const auto factory = factoryPresets();
    return std::any_of(factory.begin(), factory.end(),
                        [&name](const auto& entry) { return entry.first == name; });
}
