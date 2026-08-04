#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>

// Pure C++, JUCE-free — same convention as VoiceModel/EvolutionEngine/
// GrainCloud/HarmonicScale, which keeps this testable as a bare
// add_executable target with no JUCE linkage (see Tests/*Test.cpp).
// Main.cpp is the only place that knows about juce::MidiMessage; it
// translates each one into a MidiEvent before calling in here.

// The full set of things a MIDI control can drive. Per-voice targets
// (VoiceVolume..VoiceEnabledToggle) are bound once and apply to whichever
// voice is currently "focused" — see JerricanEditor::focusedVoiceIndex_ —
// rather than being bound per voice. SelectVoice1..4 move that focus.
// Transport (Play/Stop/Reset/Randomize) is deliberately not represented
// here — it's not MIDI-bindable, matching typical controller conventions.
enum class MidiTarget {
    VoiceVolume,
    VoicePitchCenter,
    VoiceTimbre,
    VoiceMotion,
    VoiceComplexity,
    VoiceDissonance,
    VoiceEnabledToggle,
    SelectVoice1,
    SelectVoice2,
    SelectVoice3,
    SelectVoice4,
    EvolutionAmount,
    EvolutionSpeed,
    ReverbRoom,
    ReverbDecay,
    MasterVolume,
};

inline constexpr std::array<MidiTarget, 16> kAllMidiTargets{
    MidiTarget::VoiceVolume,      MidiTarget::VoicePitchCenter, MidiTarget::VoiceTimbre,
    MidiTarget::VoiceMotion,      MidiTarget::VoiceComplexity,  MidiTarget::VoiceDissonance,
    MidiTarget::VoiceEnabledToggle, MidiTarget::SelectVoice1,   MidiTarget::SelectVoice2,
    MidiTarget::SelectVoice3,     MidiTarget::SelectVoice4,     MidiTarget::EvolutionAmount,
    MidiTarget::EvolutionSpeed,   MidiTarget::ReverbRoom,       MidiTarget::ReverbDecay,
    MidiTarget::MasterVolume,
};

// Single source of truth for target <-> name, used by the bindings popup
// UI and by MidiPresetStore's (de)serialization.
inline const char* midiTargetName(MidiTarget target) {
    switch (target) {
        case MidiTarget::VoiceVolume: return "VoiceVolume";
        case MidiTarget::VoicePitchCenter: return "VoicePitchCenter";
        case MidiTarget::VoiceTimbre: return "VoiceTimbre";
        case MidiTarget::VoiceMotion: return "VoiceMotion";
        case MidiTarget::VoiceComplexity: return "VoiceComplexity";
        case MidiTarget::VoiceDissonance: return "VoiceDissonance";
        case MidiTarget::VoiceEnabledToggle: return "VoiceEnabledToggle";
        case MidiTarget::SelectVoice1: return "SelectVoice1";
        case MidiTarget::SelectVoice2: return "SelectVoice2";
        case MidiTarget::SelectVoice3: return "SelectVoice3";
        case MidiTarget::SelectVoice4: return "SelectVoice4";
        case MidiTarget::EvolutionAmount: return "EvolutionAmount";
        case MidiTarget::EvolutionSpeed: return "EvolutionSpeed";
        case MidiTarget::ReverbRoom: return "ReverbRoom";
        case MidiTarget::ReverbDecay: return "ReverbDecay";
        case MidiTarget::MasterVolume: return "MasterVolume";
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
};

// Owns the binding table (target -> physical control) and the learn-mode
// state machine. Knows nothing about VoiceModel/EvolutionEngine/audio —
// Main.cpp's MIDI callback resolves a MidiTarget from handleEvent() and
// decides what to do with it.
class MidiBindingManager {
public:
    void armLearn(MidiTarget target) {
        learning_ = true;
        learningTarget_ = target;
    }

    void cancelLearn() { learning_ = false; }

    bool isLearning() const { return learning_; }
    MidiTarget learningTarget() const { return learningTarget_; }

    // While learning: completes the binding for the armed target (stealing
    // it from any other target that held the same physical control, so one
    // knob/pad never silently drives two things), exits learn mode, and
    // returns nullopt. Otherwise: resolves the event to a bound target, if
    // any.
    std::optional<MidiTarget> handleEvent(const MidiEvent& event) {
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

    void clearBinding(MidiTarget target) { bindings_[indexOf(target)].reset(); }

    std::optional<MidiBinding> getBinding(MidiTarget target) const {
        return bindings_[indexOf(target)];
    }

    void setBinding(MidiTarget target, MidiBinding binding) {
        bindings_[indexOf(target)] = binding;
    }

    void clearAll() {
        for (auto& binding : bindings_) {
            binding.reset();
        }
        learning_ = false;
    }

private:
    static std::size_t indexOf(MidiTarget target) {
        return static_cast<std::size_t>(
            std::distance(kAllMidiTargets.begin(),
                          std::find(kAllMidiTargets.begin(), kAllMidiTargets.end(), target)));
    }

    std::array<std::optional<MidiBinding>, kAllMidiTargets.size()> bindings_{};
    bool learning_ = false;
    MidiTarget learningTarget_ = MidiTarget::VoiceVolume;
};
