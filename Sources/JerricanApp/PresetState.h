#pragma once

#include <array>
#include <cmath>

// A full snapshot of every control's value — every voice's knobs,
// enabled state, and per-parameter Evolution toggles, plus the global
// Evolution/Reverb/Volume controls. Deliberately excludes transport
// run/stop state (isPlaying_ isn't a "control", it's transient session
// state) — a Preset is "how the instrument is set up", not "whether it's
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
struct VoicePresetState {
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

struct PresetState {
    std::array<VoicePresetState, 4> voices;
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
    // Index into ModeTable::kModes — see HarmonicScale.h. 0 (Pentatonic)
    // is the original hardcoded scale every voice used before Mode
    // existed, so any preset saved before this field existed (or that
    // simply omits it) defaults here and reproduces the exact same
    // sound with no migration needed.
    int mode = 0;
};

namespace PresetStateDetail {
inline constexpr float kFloatEpsilon = 1e-4f;
inline bool nearlyEqual(float a, float b) { return std::abs(a - b) < kFloatEpsilon; }
}  // namespace PresetStateDetail

// Used by PresetControls to detect dirty/matching state, the same way
// MidiBindingManager::equals() is for bindings. Floats are compared with
// a small epsilon rather than bit-for-bit, since round-tripping through
// text (PresetStore's save/load) isn't guaranteed to be exact.
inline bool operator==(const VoicePresetState& a, const VoicePresetState& b) {
    using PresetStateDetail::nearlyEqual;
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

inline bool operator==(const PresetState& a, const PresetState& b) {
    using PresetStateDetail::nearlyEqual;
    for (std::size_t i = 0; i < a.voices.size(); ++i) {
        if (!(a.voices[i] == b.voices[i])) {
            return false;
        }
    }
    return nearlyEqual(a.evolutionAmount, b.evolutionAmount) &&
           nearlyEqual(a.evolutionSpeed, b.evolutionSpeed) &&
           nearlyEqual(a.reverbRoom, b.reverbRoom) && nearlyEqual(a.reverbDecay, b.reverbDecay) &&
           nearlyEqual(a.masterVolume, b.masterVolume) && nearlyEqual(a.tempo, b.tempo) &&
           a.meterNumerator == b.meterNumerator && a.meterDenominator == b.meterDenominator &&
           a.mode == b.mode;
}

// Like operator==, but this is what the Presets popup actually uses to
// decide whether to show Override — a saved preset with Evolution Amount
// > 0 keeps drifting its own macros forever by design (that's the whole
// point of Evolution), so a bit-exact compare would show Override within
// moments of loading the very preset it's comparing against, permanently.
// A macro only counts as diverged if either Evolution isn't currently
// driving it (evolutionAmount is 0) or its own EvoEnabled toggle is off
// (pinned under manual control) — genuinely changing a pinned macro, or
// any of the non-drifting fields below, still shows Override as before.
inline bool matchesIgnoringEvolutionDrift(const PresetState& saved, const PresetState& live) {
    using PresetStateDetail::nearlyEqual;
    const bool evolving = live.evolutionAmount > 0.0f;
    auto valueMatches = [evolving](bool evoEnabled, float a, float b) {
        return (evolving && evoEnabled) || nearlyEqual(a, b);
    };
    for (std::size_t i = 0; i < saved.voices.size(); ++i) {
        const auto& a = saved.voices[i];
        const auto& b = live.voices[i];
        if (a.enabled != b.enabled || a.rootSemitoneOffset != b.rootSemitoneOffset) {
            return false;
        }
        if (a.volumeEvoEnabled != b.volumeEvoEnabled ||
            a.pitchRangeEvoEnabled != b.pitchRangeEvoEnabled ||
            a.timbreEvoEnabled != b.timbreEvoEnabled || a.motionEvoEnabled != b.motionEvoEnabled ||
            a.complexityEvoEnabled != b.complexityEvoEnabled ||
            a.dissonanceEvoEnabled != b.dissonanceEvoEnabled ||
            a.busyEvoEnabled != b.busyEvoEnabled || a.sustainEvoEnabled != b.sustainEvoEnabled ||
            a.cleanlinessEvoEnabled != b.cleanlinessEvoEnabled ||
            a.attackEvoEnabled != b.attackEvoEnabled) {
            return false;
        }
        if (!valueMatches(a.volumeEvoEnabled, a.volume, b.volume) ||
            !valueMatches(a.pitchRangeEvoEnabled, a.pitchLow, b.pitchLow) ||
            !valueMatches(a.pitchRangeEvoEnabled, a.pitchHigh, b.pitchHigh) ||
            !valueMatches(a.timbreEvoEnabled, a.timbre, b.timbre) ||
            !valueMatches(a.motionEvoEnabled, a.motion, b.motion) ||
            !valueMatches(a.complexityEvoEnabled, a.complexity, b.complexity) ||
            !valueMatches(a.dissonanceEvoEnabled, a.dissonance, b.dissonance) ||
            !valueMatches(a.busyEvoEnabled, a.busy, b.busy) ||
            !valueMatches(a.sustainEvoEnabled, a.sustain, b.sustain) ||
            !valueMatches(a.cleanlinessEvoEnabled, a.cleanliness, b.cleanliness) ||
            !valueMatches(a.attackEvoEnabled, a.attack, b.attack)) {
            return false;
        }
    }
    return nearlyEqual(saved.evolutionAmount, live.evolutionAmount) &&
           nearlyEqual(saved.evolutionSpeed, live.evolutionSpeed) &&
           nearlyEqual(saved.reverbRoom, live.reverbRoom) &&
           nearlyEqual(saved.reverbDecay, live.reverbDecay) &&
           nearlyEqual(saved.masterVolume, live.masterVolume) &&
           nearlyEqual(saved.tempo, live.tempo) && saved.meterNumerator == live.meterNumerator &&
           saved.meterDenominator == live.meterDenominator && saved.mode == live.mode;
}
