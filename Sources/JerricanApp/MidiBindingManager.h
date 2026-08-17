#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>

// Pure C++, JUCE-free — same convention as VoiceModel/EvolutionEngine/
// GrainCloud/HarmonicScale, which keeps this testable as a bare
// add_executable target with no JUCE linkage (see Tests/*Test.cpp).
// Main.cpp is the only place that knows about juce::MidiMessage; it
// translates each one into a MidiEvent before calling in here.

// The full set of things a MIDI control can drive. Per-voice targets
// (VoicePitchCenter..VoiceDissonanceEvoToggle) are bound once and apply
// to whichever voice is currently "focused" — see
// JerricanEditor::focusedVoiceIndex_ — rather than being bound per
// voice. SelectVoice1..4 move that focus. Transport targets are the one
// exception to "apply to the focused voice" — they're global actions,
// same as Play/Stop/Reset/Randomize's on-screen buttons.
enum class MidiTarget {
    // Per-voice (21): applies to the focused voice. VoiceMotion/
    // VoiceComplexity/VoiceTimbre keep their original identity/name
    // (still exactly "Motion"/"Complexity"/"Timbre" for Spark/Haze) even
    // though Bass's and Ambient's own knobs for the same underlying
    // VoiceModel fields are now labeled Groove/Wander/Material or
    // Speed/Complexity/Material — same precedent as Marmite's VoiceChaos
    // staying the canonical MIDI target name while its on-card label
    // reads "Busy". VoiceBusy/VoiceSustain/VoiceAttack are Bass-only,
    // VoiceCleanliness is Ambient-only; see JerricanProcessor.h's
    // applyMidiTarget for the no-op-when-a-different-voice-is-focused
    // behavior.
    VoicePitchCenter,
    VoiceVolume,
    VoiceTimbre,
    VoiceMotion,
    VoiceComplexity,
    VoiceDissonance,
    VoiceBusy,
    VoiceSustain,
    VoiceCleanliness,
    VoiceAttack,
    VoiceEnabledToggle,
    VoicePitchRangeEvoToggle,
    VoiceVolumeEvoToggle,
    VoiceTimbreEvoToggle,
    VoiceMotionEvoToggle,
    VoiceComplexityEvoToggle,
    VoiceDissonanceEvoToggle,
    VoiceBusyEvoToggle,
    VoiceSustainEvoToggle,
    VoiceCleanlinessEvoToggle,
    VoiceAttackEvoToggle,
    // Voice focus (4).
    SelectVoice1,
    SelectVoice2,
    SelectVoice3,
    SelectVoice4,
    // Transport (4).
    TransportPlay,
    TransportStop,
    TransportReset,
    TransportRandomize,
    // Global (7).
    EvolutionAmount,
    EvolutionSpeed,
    ReverbRoom,
    ReverbDecay,
    MasterVolume,
    Tempo,
    Meter,
};

inline constexpr std::array<MidiTarget, 36> kAllMidiTargets{
    MidiTarget::VoicePitchCenter,
    MidiTarget::VoiceVolume,
    MidiTarget::VoiceTimbre,
    MidiTarget::VoiceMotion,
    MidiTarget::VoiceComplexity,
    MidiTarget::VoiceDissonance,
    MidiTarget::VoiceBusy,
    MidiTarget::VoiceSustain,
    MidiTarget::VoiceCleanliness,
    MidiTarget::VoiceAttack,
    MidiTarget::VoiceEnabledToggle,
    MidiTarget::VoicePitchRangeEvoToggle,
    MidiTarget::VoiceVolumeEvoToggle,
    MidiTarget::VoiceTimbreEvoToggle,
    MidiTarget::VoiceMotionEvoToggle,
    MidiTarget::VoiceComplexityEvoToggle,
    MidiTarget::VoiceDissonanceEvoToggle,
    MidiTarget::VoiceBusyEvoToggle,
    MidiTarget::VoiceSustainEvoToggle,
    MidiTarget::VoiceCleanlinessEvoToggle,
    MidiTarget::VoiceAttackEvoToggle,
    MidiTarget::SelectVoice1,
    MidiTarget::SelectVoice2,
    MidiTarget::SelectVoice3,
    MidiTarget::SelectVoice4,
    MidiTarget::TransportPlay,
    MidiTarget::TransportStop,
    MidiTarget::TransportReset,
    MidiTarget::TransportRandomize,
    MidiTarget::EvolutionAmount,
    MidiTarget::EvolutionSpeed,
    MidiTarget::ReverbRoom,
    MidiTarget::ReverbDecay,
    MidiTarget::MasterVolume,
    MidiTarget::Tempo,
    MidiTarget::Meter,
};

// Single source of truth for target <-> name, used by the bindings popup
// UI and by MidiPresetStore's (de)serialization.
inline const char* midiTargetName(MidiTarget target) {
    switch (target) {
        case MidiTarget::VoicePitchCenter: return "VoicePitchCenter";
        case MidiTarget::VoiceVolume: return "VoiceVolume";
        case MidiTarget::VoiceTimbre: return "VoiceTimbre";
        case MidiTarget::VoiceMotion: return "VoiceMotion";
        case MidiTarget::VoiceComplexity: return "VoiceComplexity";
        case MidiTarget::VoiceDissonance: return "VoiceDissonance";
        case MidiTarget::VoiceBusy: return "VoiceBusy";
        case MidiTarget::VoiceSustain: return "VoiceSustain";
        case MidiTarget::VoiceCleanliness: return "VoiceCleanliness";
        case MidiTarget::VoiceAttack: return "VoiceAttack";
        case MidiTarget::VoiceEnabledToggle: return "VoiceEnabledToggle";
        case MidiTarget::VoicePitchRangeEvoToggle: return "VoicePitchRangeEvoToggle";
        case MidiTarget::VoiceVolumeEvoToggle: return "VoiceVolumeEvoToggle";
        case MidiTarget::VoiceTimbreEvoToggle: return "VoiceTimbreEvoToggle";
        case MidiTarget::VoiceMotionEvoToggle: return "VoiceMotionEvoToggle";
        case MidiTarget::VoiceComplexityEvoToggle: return "VoiceComplexityEvoToggle";
        case MidiTarget::VoiceDissonanceEvoToggle: return "VoiceDissonanceEvoToggle";
        case MidiTarget::VoiceBusyEvoToggle: return "VoiceBusyEvoToggle";
        case MidiTarget::VoiceSustainEvoToggle: return "VoiceSustainEvoToggle";
        case MidiTarget::VoiceCleanlinessEvoToggle: return "VoiceCleanlinessEvoToggle";
        case MidiTarget::VoiceAttackEvoToggle: return "VoiceAttackEvoToggle";
        case MidiTarget::SelectVoice1: return "SelectVoice1";
        case MidiTarget::SelectVoice2: return "SelectVoice2";
        case MidiTarget::SelectVoice3: return "SelectVoice3";
        case MidiTarget::SelectVoice4: return "SelectVoice4";
        case MidiTarget::TransportPlay: return "TransportPlay";
        case MidiTarget::TransportStop: return "TransportStop";
        case MidiTarget::TransportReset: return "TransportReset";
        case MidiTarget::TransportRandomize: return "TransportRandomize";
        case MidiTarget::EvolutionAmount: return "EvolutionAmount";
        case MidiTarget::EvolutionSpeed: return "EvolutionSpeed";
        case MidiTarget::ReverbRoom: return "ReverbRoom";
        case MidiTarget::ReverbDecay: return "ReverbDecay";
        case MidiTarget::MasterVolume: return "MasterVolume";
        case MidiTarget::Tempo: return "Tempo";
        case MidiTarget::Meter: return "Meter";
    }
    return "";
}

inline std::optional<MidiTarget> midiTargetFromName(const std::string& name) {
    for (const auto target : kAllMidiTargets) {
        if (name == midiTargetName(target)) {
            return target;
        }
    }
    return std::nullopt;
}

// A controller message reduced to just what binding/dispatch cares about.
// Main.cpp only ever constructs this for CC messages and NoteOn messages
// with velocity > 0 — note-off and everything else is filtered upstream.
struct MidiEvent {
    enum class Type { ControlChange, NoteOn };
    Type type = Type::ControlChange;
    int channel = 1;  // 1-16
    int number = 0;   // CC number, or note number for NoteOn
    float value = 0.0f;  // normalized 0..1 (CC/127, or velocity/127)
};

struct MidiBinding {
    MidiEvent::Type type = MidiEvent::Type::ControlChange;
    int channel = 1;
    int number = 0;

    bool matches(const MidiEvent& event) const {
        return type == event.type && channel == event.channel && number == event.number;
    }

    bool operator==(const MidiBinding& other) const {
        return type == other.type && channel == other.channel && number == other.number;
    }
};

// Owns the binding table (target -> physical control) and the learn-mode
// state machine. Knows nothing about VoiceModel/EvolutionEngine/audio —
// Main.cpp's MIDI callback resolves a MidiTarget from handleEvent() and
// decides what to do with it.
class MidiBindingManager {
public:
    // Guards bindings_/learning_/learningTarget_ below. This class is only
    // ever touched from the MIDI thread (handleEvent, via
    // JerricanEditor::handleIncomingMidiMessage) and the UI thread (the
    // bindings popup's Learn/Clear clicks and its refresh timer) — never
    // the audio thread — so a plain mutex is the right tool here, unlike
    // VoiceModel/EvolutionEngine's atomics, which exist specifically to
    // stay lock-free for the audio thread. Blocking briefly on the MIDI
    // thread is harmless; MIDI input runs on its own callback thread, not
    // the realtime audio device thread.
    void armLearn(MidiTarget target) {
        std::lock_guard<std::mutex> lock(mutex_);
        learning_ = true;
        learningTarget_ = target;
    }

    void cancelLearn() {
        std::lock_guard<std::mutex> lock(mutex_);
        learning_ = false;
    }

    bool isLearning() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return learning_;
    }

    MidiTarget learningTarget() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return learningTarget_;
    }

    // While learning: completes the binding for the armed target (stealing
    // it from any other target that held the same physical control, so one
    // knob/pad never silently drives two things), exits learn mode, and
    // returns nullopt. Otherwise: resolves the event to a bound target, if
    // any.
    std::optional<MidiTarget> handleEvent(const MidiEvent& event) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (learning_) {
            for (auto& binding : bindings_) {
                if (binding.has_value() && binding->matches(event)) {
                    binding.reset();
                }
            }
            bindings_[indexOf(learningTarget_)] =
                MidiBinding{event.type, event.channel, event.number};
            learning_ = false;
            return std::nullopt;
        }

        for (const auto target : kAllMidiTargets) {
            const auto& binding = bindings_[indexOf(target)];
            if (binding.has_value() && binding->matches(event)) {
                return target;
            }
        }
        return std::nullopt;
    }

    void clearBinding(MidiTarget target) {
        std::lock_guard<std::mutex> lock(mutex_);
        bindings_[indexOf(target)].reset();
    }

    std::optional<MidiBinding> getBinding(MidiTarget target) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return bindings_[indexOf(target)];
    }

    void setBinding(MidiTarget target, MidiBinding binding) {
        std::lock_guard<std::mutex> lock(mutex_);
        bindings_[indexOf(target)] = binding;
    }

    void clearAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& binding : bindings_) {
            binding.reset();
        }
        learning_ = false;
    }

    // Whether this manager's binding table matches another's — used by
    // PresetControls to tell whether the live bindings still match a
    // selected preset (not dirty) or a saved preset happens to match the
    // live state (so it should auto-select). Ignores learn-mode state,
    // which isn't part of what a preset captures.
    bool equals(const MidiBindingManager& other) const {
        for (const auto target : kAllMidiTargets) {
            if (getBinding(target) != other.getBinding(target)) {
                return false;
            }
        }
        return true;
    }

private:
    static std::size_t indexOf(MidiTarget target) {
        return static_cast<std::size_t>(
            std::distance(kAllMidiTargets.begin(),
                          std::find(kAllMidiTargets.begin(), kAllMidiTargets.end(), target)));
    }

    mutable std::mutex mutex_;
    std::array<std::optional<MidiBinding>, kAllMidiTargets.size()> bindings_{};
    bool learning_ = false;
    MidiTarget learningTarget_ = MidiTarget::VoiceVolume;
};
