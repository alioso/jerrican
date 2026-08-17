#pragma once

#include <array>
#include <cmath>

// A full snapshot of every control's value — every voice's knobs,
// enabled state, and per-parameter Evolution toggles, plus the global
// Evolution/Reverb/Volume controls. Deliberately excludes transport
// run/stop state (isPlaying_ isn't a "control", it's transient session
// state) — a Scene is "how the instrument is set up", not "whether it's
// currently making sound". Plain C++, JUCE-free, same convention as
// MidiBinding — Main.cpp is the only place that reads/writes this
// against the live VoiceModel/EvolutionEngine/atomics.
// motion/complexity keep their original field names even though Bass's and
// Ambient's own controls (built on these same VoiceModel fields) are now
// labeled Groove/Wander or Speed/Complexity — the serialized name is an
// internal identifier, not something a user sees, so renaming it would
// only add a backward-compatibility migration for zero benefit. busy/
// sustain/attack are Bass-only, cleanliness is Ambient-only (unused, but
// still saved/loaded, for the other voices).
struct VoiceSceneState {
    bool enabled = true;
    float volume = 0.0f;
    float pitchLow = 0.0f;
    float pitchHigh = 0.0f;
    float timbre = 0.0f;
    float motion = 0.0f;
    float complexity = 0.0f;
    float dissonance = 0.0f;
    int rootSemitoneOffset = 0;
    float busy = 0.5f;
    float sustain = 0.5f;
    float cleanliness = 0.5f;
    float attack = 0.5f;
    bool volumeEvoEnabled = true;
    bool pitchRangeEvoEnabled = true;
    bool timbreEvoEnabled = true;
    bool motionEvoEnabled = true;
    bool complexityEvoEnabled = true;
    bool dissonanceEvoEnabled = true;
    bool busyEvoEnabled = true;
    bool sustainEvoEnabled = true;
    bool cleanlinessEvoEnabled = true;
    bool attackEvoEnabled = true;
};

struct SceneState {
    std::array<VoiceSceneState, 4> voices;
    float evolutionAmount = 0.0f;
    float evolutionSpeed = 0.5f;
    float reverbRoom = 0.0f;
    float reverbDecay = 0.0f;
    float masterVolume = 1.0f;
    // Tempo/meter — see PatternClock.h/MeterTable.h. Only Bass consumes
    // these so far, but they're global/instrument-wide the same way
    // Marmite's are.
    float tempo = 120.0f;
    int meterNumerator = 4;
    int meterDenominator = 4;
};

namespace SceneStateDetail {
inline constexpr float kFloatEpsilon = 1e-4f;
inline bool nearlyEqual(float a, float b) { return std::abs(a - b) < kFloatEpsilon; }
}  // namespace SceneStateDetail

// Used by PresetControls to detect dirty/matching state, the same way
// MidiBindingManager::equals() is for bindings. Floats are compared with
// a small epsilon rather than bit-for-bit, since round-tripping through
// text (ScenePresetStore's save/load) isn't guaranteed to be exact.
inline bool operator==(const VoiceSceneState& a, const VoiceSceneState& b) {
    using SceneStateDetail::nearlyEqual;
    return a.enabled == b.enabled && nearlyEqual(a.volume, b.volume) &&
           nearlyEqual(a.pitchLow, b.pitchLow) && nearlyEqual(a.pitchHigh, b.pitchHigh) &&
           nearlyEqual(a.timbre, b.timbre) && nearlyEqual(a.motion, b.motion) &&
           nearlyEqual(a.complexity, b.complexity) && nearlyEqual(a.dissonance, b.dissonance) &&
           a.rootSemitoneOffset == b.rootSemitoneOffset && nearlyEqual(a.busy, b.busy) &&
           nearlyEqual(a.sustain, b.sustain) && nearlyEqual(a.cleanliness, b.cleanliness) &&
           nearlyEqual(a.attack, b.attack) && a.volumeEvoEnabled == b.volumeEvoEnabled &&
           a.pitchRangeEvoEnabled == b.pitchRangeEvoEnabled &&
           a.timbreEvoEnabled == b.timbreEvoEnabled && a.motionEvoEnabled == b.motionEvoEnabled &&
           a.complexityEvoEnabled == b.complexityEvoEnabled &&
           a.dissonanceEvoEnabled == b.dissonanceEvoEnabled &&
           a.busyEvoEnabled == b.busyEvoEnabled && a.sustainEvoEnabled == b.sustainEvoEnabled &&
           a.cleanlinessEvoEnabled == b.cleanlinessEvoEnabled &&
           a.attackEvoEnabled == b.attackEvoEnabled;
}

inline bool operator==(const SceneState& a, const SceneState& b) {
    using SceneStateDetail::nearlyEqual;
    for (std::size_t i = 0; i < a.voices.size(); ++i) {
        if (!(a.voices[i] == b.voices[i])) {
            return false;
        }
    }
    return nearlyEqual(a.evolutionAmount, b.evolutionAmount) &&
           nearlyEqual(a.evolutionSpeed, b.evolutionSpeed) &&
           nearlyEqual(a.reverbRoom, b.reverbRoom) && nearlyEqual(a.reverbDecay, b.reverbDecay) &&
           nearlyEqual(a.masterVolume, b.masterVolume) && nearlyEqual(a.tempo, b.tempo) &&
           a.meterNumerator == b.meterNumerator && a.meterDenominator == b.meterDenominator;
}
