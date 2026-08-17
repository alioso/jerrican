#pragma once

#include <JuceHeader.h>

#include <array>
#include <optional>

#include "AudioRecorder.h"
#include "BassGroovePattern.h"
#include "EvolutionEngine.h"
#include "FastRandom.h"
#include "Grain.h"
#include "GrainCloud.h"
#include "MeterTable.h"
#include "MidiBindingManager.h"
#include "MidiPresetStore.h"
#include "PatternClock.h"
#include "ScenePresetStore.h"
#include "SceneState.h"
#include "VoiceModel.h"

class JerricanAudioProcessorEditor;  // defined in JerricanEditor.h, included at the bottom.

// Owns every piece of live engine state (voices, grain clouds, evolution,
// reverb, MIDI Learn/Scenes stores, the recorder) and does all audio/MIDI
// processing — the AudioProcessor half of the Processor/Editor split. Any
// juce::Component work belongs in JerricanAudioProcessorEditor instead;
// this class must stay usable with no editor ever created (headless
// hosts, offline rendering).
class JerricanAudioProcessor : public juce::AudioProcessor {
public:
    // groove/wander are the same VoiceModel fields the old Motion/
    // Complexity macros used — voice 0 (Bass) and voice 1 (Ambient) give
    // them their own bespoke meaning (Bass: rhythmic placement / harmonic
    // roaming; Ambient: layer-phasing Speed / texture-thickness
    // Complexity — Complexity's underlying mechanism is unchanged for
    // Ambient, only relabeled); Spark/Haze keep the exact old numeric
    // values and exact old behavior (pitch-drift retarget rate /
    // grain-spawn density). timbre similarly means Material for Ambient
    // (new DSP — see Grain::triggerAmbient) but is otherwise unchanged for
    // the other three. busy/sustain/attack are Bass-only, cleanliness is
    // Ambient-only — unused by whichever voices don't consume them.
    struct InitialVoice {
        const char* name;
        bool enabled;
        float volume;
        float pitchLow;
        float pitchHigh;
        float timbre;
        float groove;
        float wander;
        float dissonance;
        float busy;
        float sustain;
        float cleanliness;
        float attack;
        int rootSemitoneOffset;
        float minGrainDurationMs;
        float maxGrainDurationMs;
        Grain::Character character;
    };

    // Grain duration range is the main lever for a voice's fundamental
    // character (see GrainCloud) — short & sparse reads as pointillistic,
    // long & overlapping reads as a sustained drone. Character::Plucked
    // (Bass/Spark) gets a softened fast-attack envelope and a gentle
    // bright-to-dark filter sweep per grain; Character::Ambient (Ambient/
    // Haze) is the original unfiltered symmetric envelope. Dissonance
    // near 0 for everyone by default so voices quantize mostly to their
    // (rooted) consonant scale out of the box; roots default to an A
    // major triad across the four voices.
    //
    // Bass (formerly Pulse) sits low in the 4-octave range (~75-260Hz),
    // grain duration 110ms-4.8s doubles as Sustain's 0/1 endpoints (see
    // GrainCloud::spawnGrainNow) — the top end is deliberately past a
    // full 4/4 bar at 60bpm (4s), so Sustain=1 can genuinely hold a whole
    // note for a bar rather than getting cut short — and Busy/Groove/
    // Wander starting points aim for a moderate, mostly-anchored-to-root
    // walking line — all tuning starting points, not locked.
    //
    // Ambient (formerly Drone) sits low (~57-125Hz), long grain duration
    // (1.5-4s) for genuinely sustained overlapping layers, Speed/
    // Complexity kept slow/sparse for a glacial Eno-style drift, Material
    // leaning toward Glass (pure) and Cleanliness leaning clean — a
    // deliberately understated starting point, not locked.
    static constexpr std::array<InitialVoice, 4> kInitialVoices{
        {{"Bass", true, 0.70f, 0.15f, 0.45f, 0.35f, 0.25f, 0.30f, 0.10f, 0.45f, 0.5f, 0.5f, 0.85f, 0,
          110.0f, 4800.0f, Grain::Character::Plucked},
         {"Ambient", true, 0.60f, 0.05f, 0.20f, 0.15f, 0.10f, 0.12f, 0.15f, 0.5f, 0.5f, 0.65f, 0.5f, 0,
          1500.0f, 4000.0f, Grain::Character::Ambient},
         {"Spark", true, 0.55f, 0.60f, 0.85f, 0.70f, 0.50f, 0.45f, 0.15f, 0.5f, 0.5f, 0.5f, 0.5f, 7,
          150.0f, 400.0f, Grain::Character::Plucked},
         {"Haze", true, 0.50f, 0.15f, 0.35f, 0.75f, 0.20f, 0.15f, 0.15f, 0.5f, 0.5f, 0.5f, 0.5f, 4,
          2000.0f, 5000.0f, Grain::Character::Ambient}}};

    JerricanAudioProcessor()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo())),
          voices_{VoiceModel(kInitialVoices[0].name, kInitialVoices[0].enabled,
                              kInitialVoices[0].volume, kInitialVoices[0].pitchLow,
                              kInitialVoices[0].pitchHigh, kInitialVoices[0].timbre,
                              kInitialVoices[0].groove, kInitialVoices[0].wander,
                              kInitialVoices[0].dissonance, kInitialVoices[0].rootSemitoneOffset,
                              kInitialVoices[0].busy, kInitialVoices[0].sustain,
                              kInitialVoices[0].cleanliness, kInitialVoices[0].attack),
                  VoiceModel(kInitialVoices[1].name, kInitialVoices[1].enabled,
                              kInitialVoices[1].volume, kInitialVoices[1].pitchLow,
                              kInitialVoices[1].pitchHigh, kInitialVoices[1].timbre,
                              kInitialVoices[1].groove, kInitialVoices[1].wander,
                              kInitialVoices[1].dissonance, kInitialVoices[1].rootSemitoneOffset,
                              kInitialVoices[1].busy, kInitialVoices[1].sustain,
                              kInitialVoices[1].cleanliness, kInitialVoices[1].attack),
                  VoiceModel(kInitialVoices[2].name, kInitialVoices[2].enabled,
                              kInitialVoices[2].volume, kInitialVoices[2].pitchLow,
                              kInitialVoices[2].pitchHigh, kInitialVoices[2].timbre,
                              kInitialVoices[2].groove, kInitialVoices[2].wander,
                              kInitialVoices[2].dissonance, kInitialVoices[2].rootSemitoneOffset,
                              kInitialVoices[2].busy, kInitialVoices[2].sustain,
                              kInitialVoices[2].cleanliness, kInitialVoices[2].attack),
                  VoiceModel(kInitialVoices[3].name, kInitialVoices[3].enabled,
                              kInitialVoices[3].volume, kInitialVoices[3].pitchLow,
                              kInitialVoices[3].pitchHigh, kInitialVoices[3].timbre,
                              kInitialVoices[3].groove, kInitialVoices[3].wander,
                              kInitialVoices[3].dissonance, kInitialVoices[3].rootSemitoneOffset,
                              kInitialVoices[3].busy, kInitialVoices[3].sustain,
                              kInitialVoices[3].cleanliness, kInitialVoices[3].attack)},
          grainClouds_{GrainCloud(0x1a2b3c4du, kInitialVoices[0].minGrainDurationMs,
                                   kInitialVoices[0].maxGrainDurationMs, kInitialVoices[0].character),
                       GrainCloud(0x5e6f7081u, kInitialVoices[1].minGrainDurationMs,
                                   kInitialVoices[1].maxGrainDurationMs, kInitialVoices[1].character),
                       GrainCloud(0x92a3b4c5u, kInitialVoices[2].minGrainDurationMs,
                                   kInitialVoices[2].maxGrainDurationMs, kInitialVoices[2].character),
                       GrainCloud(0xd6e7f809u, kInitialVoices[3].minGrainDurationMs,
                                   kInitialVoices[3].maxGrainDurationMs, kInitialVoices[3].character)},
          evolutionEngines_{EvolutionEngine(0x37a1f2c9u), EvolutionEngine(0x6b4d8e12u),
                             EvolutionEngine(0xa9c3f501u), EvolutionEngine(0xe1d47b6au)},
          currentBassAccentProfile_(
              MeterTable::generateBassAccentProfile(MeterTable::kMeters[MeterTable::kDefaultMeterIndex])),
          bassGroovePattern_(0x7a2c91efu, &currentBassAccentProfile_, currentMeterSlotCount_) {
        for (size_t i = 0; i < voices_.size(); ++i) {
            const auto& initial = kInitialVoices[i];
            const float center = (initial.pitchLow + initial.pitchHigh) * 0.5f;
            const float width = initial.pitchHigh - initial.pitchLow;
            evolutionEngines_[i].resetTo(center, width, initial.volume, initial.timbre,
                                          initial.groove, initial.wander, initial.dissonance,
                                          initial.busy, initial.sustain, initial.cleanliness,
                                          initial.attack);
        }
        recordingThread_.startThread();
    }

    ~JerricanAudioProcessor() override {
        recorder_.stop();
        recordingThread_.stopThread(2000);
    }

    const juce::String getName() const override { return "Jerrican"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    void prepareToPlay(double sampleRate, int /*samplesPerBlock*/) override {
        for (auto& cloud : grainClouds_) {
            cloud.setSampleRate(sampleRate);
        }
        for (auto& engine : evolutionEngines_) {
            engine.setSampleRate(sampleRate);
        }
        reverb_.setSampleRate(sampleRate);
        patternClock_.setSampleRate(sampleRate);
        patternClock_.setBpm(tempo_.load(std::memory_order_relaxed));
        sampleRate_ = sampleRate;
    }

    void releaseResources() override {}

    // Runs on the audio thread. Every write below goes through the same
    // atomic VoiceModel/EvolutionEngine setters already used from the UI
    // thread — safe under the lock-free pattern already established
    // throughout this codebase, no new synchronization needed.
    void handleMidiMessage(const juce::MidiMessage& message) {
        std::optional<MidiEvent> event;
        if (message.isController()) {
            event = MidiEvent{MidiEvent::Type::ControlChange, message.getChannel(),
                              message.getControllerNumber(),
                              static_cast<float>(message.getControllerValue()) / 127.0f};
        } else if (message.isNoteOn()) {
            event = MidiEvent{MidiEvent::Type::NoteOn, message.getChannel(),
                              message.getNoteNumber(),
                              static_cast<float>(message.getVelocity()) / 127.0f};
        } else {
            return;
        }

        const auto target = midiBindings_.handleEvent(*event);
        if (target.has_value()) {
            applyMidiTarget(*target, event->value);
        }
    }

    void applyMidiTarget(MidiTarget target, float value) {
        const int focused = focusedVoiceIndex_.load(std::memory_order_relaxed);
        auto& voice = voices_[static_cast<size_t>(focused)];
        auto& evolution = evolutionEngines_[static_cast<size_t>(focused)];

        switch (target) {
            case MidiTarget::VoiceVolume:
                voice.setVolume(value);
                evolution.resyncVolume(value);
                break;
            case MidiTarget::VoicePitchCenter: {
                // Remap the knob's full 0..1 throw directly onto the
                // achievable center range [halfWidth, 1-halfWidth] — a
                // range's center can never sit closer to an edge than
                // half its own width, so a naive value-halfWidth offset
                // (clamped) leaves dead zones at both ends where the
                // knob keeps turning but the range stops moving.
                const float width = voice.getPitchRangeHigh() - voice.getPitchRangeLow();
                const float span = 1.0f - width;
                const float low = span > 0.0f ? value * span : 0.0f;
                voice.setPitchRange(low, low + width);
                evolution.resyncPitchRange(low, low + width);
                break;
            }
            case MidiTarget::VoiceTimbre:
                voice.setTimbre(value);
                evolution.resyncTimbre(value);
                break;
            // VoiceMotion/VoiceComplexity keep their original MIDI-target
            // identity (see MidiBindingManager.h) even though the
            // VoiceModel fields underneath were renamed groove_/wander_ —
            // for Bass this now means Groove/Wander, for Drone/Spark/Haze
            // it's exactly the same Motion/Complexity behavior as before.
            case MidiTarget::VoiceMotion:
                voice.setGroove(value);
                evolution.resyncGroove(value);
                break;
            case MidiTarget::VoiceComplexity:
                voice.setWander(value);
                evolution.resyncWander(value);
                break;
            case MidiTarget::VoiceDissonance:
                voice.setDissonance(value);
                evolution.resyncDissonance(value);
                break;
            // Bass-only — no-op for the other three voices, since Busy/
            // Sustain have no defined meaning for them (rather than
            // silently reinterpreting a Groove-labeled MIDI input as some
            // other behavior).
            case MidiTarget::VoiceBusy:
                if (focused == 0) {
                    voice.setBusy(value);
                    evolution.resyncBusy(value);
                }
                break;
            case MidiTarget::VoiceSustain:
                if (focused == 0) {
                    voice.setSustain(value);
                    evolution.resyncSustain(value);
                }
                break;
            case MidiTarget::VoiceAttack:
                if (focused == 0) {
                    voice.setAttack(value);
                    evolution.resyncAttack(value);
                }
                break;
            // Ambient-only — same no-op-elsewhere reasoning as Busy/
            // Sustain above.
            case MidiTarget::VoiceCleanliness:
                if (focused == 1) {
                    voice.setCleanliness(value);
                    evolution.resyncCleanliness(value);
                }
                break;
            case MidiTarget::VoiceEnabledToggle:
                voice.setEnabled(!voice.isEnabled());
                break;
            case MidiTarget::VoicePitchRangeEvoToggle: {
                const bool on = !evolution.isPitchRangeEnabled();
                evolution.setPitchRangeEnabled(on);
                if (on) evolution.resyncPitchRange(voice.getPitchRangeLow(), voice.getPitchRangeHigh());
                break;
            }
            case MidiTarget::VoiceVolumeEvoToggle: {
                const bool on = !evolution.isVolumeEnabled();
                evolution.setVolumeEnabled(on);
                if (on) evolution.resyncVolume(voice.getVolume());
                break;
            }
            case MidiTarget::VoiceTimbreEvoToggle: {
                const bool on = !evolution.isTimbreEnabled();
                evolution.setTimbreEnabled(on);
                if (on) evolution.resyncTimbre(voice.getTimbre());
                break;
            }
            case MidiTarget::VoiceMotionEvoToggle: {
                const bool on = !evolution.isGrooveEnabled();
                evolution.setGrooveEnabled(on);
                if (on) evolution.resyncGroove(voice.getGroove());
                break;
            }
            case MidiTarget::VoiceComplexityEvoToggle: {
                const bool on = !evolution.isWanderEnabled();
                evolution.setWanderEnabled(on);
                if (on) evolution.resyncWander(voice.getWander());
                break;
            }
            case MidiTarget::VoiceDissonanceEvoToggle: {
                const bool on = !evolution.isDissonanceEnabled();
                evolution.setDissonanceEnabled(on);
                if (on) evolution.resyncDissonance(voice.getDissonance());
                break;
            }
            case MidiTarget::VoiceBusyEvoToggle: {
                if (focused == 0) {
                    const bool on = !evolution.isBusyEnabled();
                    evolution.setBusyEnabled(on);
                    if (on) evolution.resyncBusy(voice.getBusy());
                }
                break;
            }
            case MidiTarget::VoiceSustainEvoToggle: {
                if (focused == 0) {
                    const bool on = !evolution.isSustainEnabled();
                    evolution.setSustainEnabled(on);
                    if (on) evolution.resyncSustain(voice.getSustain());
                }
                break;
            }
            case MidiTarget::VoiceAttackEvoToggle: {
                if (focused == 0) {
                    const bool on = !evolution.isAttackEnabled();
                    evolution.setAttackEnabled(on);
                    if (on) evolution.resyncAttack(voice.getAttack());
                }
                break;
            }
            case MidiTarget::VoiceCleanlinessEvoToggle: {
                if (focused == 1) {
                    const bool on = !evolution.isCleanlinessEnabled();
                    evolution.setCleanlinessEnabled(on);
                    if (on) evolution.resyncCleanliness(voice.getCleanliness());
                }
                break;
            }
            case MidiTarget::SelectVoice1:
                focusedVoiceIndex_.store(0, std::memory_order_relaxed);
                break;
            case MidiTarget::SelectVoice2:
                focusedVoiceIndex_.store(1, std::memory_order_relaxed);
                break;
            case MidiTarget::SelectVoice3:
                focusedVoiceIndex_.store(2, std::memory_order_relaxed);
                break;
            case MidiTarget::SelectVoice4:
                focusedVoiceIndex_.store(3, std::memory_order_relaxed);
                break;
            case MidiTarget::TransportPlay:
                handlePlayPressed();
                break;
            case MidiTarget::TransportStop:
                handleStopPressed();
                break;
            case MidiTarget::TransportReset:
                handleResetPressed();
                break;
            case MidiTarget::TransportRandomize:
                handleRandomizePressed();
                break;
            case MidiTarget::EvolutionAmount:
                evolutionAmount_.store(value, std::memory_order_relaxed);
                break;
            case MidiTarget::EvolutionSpeed:
                evolutionSpeed_.store(value, std::memory_order_relaxed);
                break;
            case MidiTarget::ReverbRoom:
                reverbRoom_.store(value, std::memory_order_relaxed);
                break;
            case MidiTarget::ReverbDecay:
                reverbDecay_.store(value, std::memory_order_relaxed);
                break;
            case MidiTarget::MasterVolume:
                masterVolume_.store(value, std::memory_order_relaxed);
                break;
            case MidiTarget::Tempo: {
                const float bpm = 40.0f + value * (240.0f - 40.0f);
                tempo_.store(bpm, std::memory_order_relaxed);
                patternClock_.setBpm(bpm);
                break;
            }
            // Quantizes the continuous 0..1 MIDI value onto the fixed set
            // of 7 meters, same idiom as Marmite's DelayTime/Meter targets.
            case MidiTarget::Meter: {
                const int index = juce::jlimit(
                    0, static_cast<int>(MeterTable::kMeters.size()) - 1,
                    static_cast<int>(value * static_cast<float>(MeterTable::kMeters.size())));
                requestMeter(MeterTable::kMeters[static_cast<std::size_t>(index)].numerator,
                            MeterTable::kMeters[static_cast<std::size_t>(index)].denominator);
                break;
            }
        }
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        juce::ScopedNoDenormals noDenormals;

        for (const auto metadata : midiMessages) {
            handleMidiMessage(metadata.getMessage());
        }

        // Meter changes touch non-atomic state (currentBassAccentProfile_,
        // bassGroovePattern_'s internal mask), so a change requested from
        // the UI/MIDI thread is queued here and consumed once per block on
        // the audio thread, rather than racing a torn write.
        const int pendingMeter = pendingMeterIndex_.exchange(-1, std::memory_order_relaxed);
        if (pendingMeter >= 0) {
            applyMeterChange(pendingMeter);
        }
        if (pendingPhaseReset_.exchange(false, std::memory_order_relaxed)) {
            resetPatternPhase();
        }

        processHostSync();

        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
        const int numSamples = buffer.getNumSamples();

        const bool playing = isPlaying_.load(std::memory_order_relaxed);
        const float evolutionAmount = evolutionAmount_.load(std::memory_order_relaxed);
        const float evolutionSpeed = evolutionSpeed_.load(std::memory_order_relaxed);
        const float masterVolume = masterVolume_.load(std::memory_order_relaxed);

        for (int sample = 0; sample < numSamples; ++sample) {
            const bool onGridBoundary = playing && patternClock_.tick();
            if (onGridBoundary) {
                currentSlot16_ = (currentSlot16_ + 1) % currentMeterSlotCount_;
                currentSlot16Display_.store(currentSlot16_, std::memory_order_relaxed);
            }

            float mixedLeft = 0.0f;
            float mixedRight = 0.0f;

            for (size_t i = 0; i < voices_.size(); ++i) {
                auto& voice = voices_[i];
                auto& cloud = grainClouds_[i];

                evolutionEngines_[i].update(voice, playing ? evolutionAmount : 0.0f, evolutionSpeed);

                Grain::StereoSample voiceSample;
                if (i == 0) {
                    // Bass: fully metered — Busy/Groove drive
                    // BassGroovePattern's onset scheduling instead of
                    // GrainCloud's own continuous stochastic spawn model,
                    // so there's zero leftover stochastic texture. update()
                    // still runs even when stopped/disabled (so a pending
                    // delayed trigger's countdown gets consumed rather than
                    // frozen), but the resulting trigger is only acted on
                    // while actually playing and enabled.
                    const auto trigger = bassGroovePattern_.update(
                        onGridBoundary, currentSlot16_, voice.getBusy(), voice.getGroove(),
                        evolutionAmount, patternClock_.getSamplesPerSubdivision());
                    if (trigger.has_value() && playing && voice.isEnabled()) {
                        cloud.spawnGrainNow(voice.getPitchRangeLow(), voice.getPitchRangeHigh(),
                                            voice.getTimbre(), voice.getWander(), voice.getSustain(),
                                            voice.getDissonance(), voice.getAttack(),
                                            voice.getRootSemitoneOffset());
                    }
                    // 1.7x: Bass is structurally a single melodic line
                    // (mostly one grain at a time) vs. the other voices'
                    // continuously overlapping textures, so it needs its
                    // own headroom boost to read as comparably loud — the
                    // clean (Dirt=0) end in particular was reading quiet
                    // relative to the rest of the mix.
                    voiceSample =
                        cloud.renderActiveGrainsCorrelated(voice.getVolume(), voice.getWander(), 1.7f);
                } else if (i == 1) {
                    // Ambient: Speed scales grain duration (GrainCloud::
                    // maybeSpawnAmbientGrain), Layers (Complexity relabeled)
                    // drives grain density, Material/Cleanliness drive
                    // Grain::triggerAmbient's tone instead of the random
                    // pickWaveform lottery the other three voices use.
                    // `active` (not a zeroed density) gates whether new
                    // grains spawn at all — Layers=0 must still mean
                    // "sparse", not double as the stop/start switch.
                    voiceSample = cloud.renderAmbientSample(
                        voice.getPitchRangeLow(), voice.getPitchRangeHigh(), voice.getTimbre(),
                        voice.getGroove(), voice.getWander(), voice.getVolume(), voice.getDissonance(),
                        voice.getCleanliness(), playing && voice.isEnabled(),
                        voice.getRootSemitoneOffset());
                } else {
                    // Spark/Haze: unchanged continuous stochastic model.
                    // getGroove()/getWander() are the same underlying
                    // fields as the old Motion/Complexity (renamed at the
                    // VoiceModel level when Bass's redesign gave those
                    // names new meaning for voice 0 only) — values and
                    // behavior are identical to before.
                    const float spawnDensity =
                        (playing && voice.isEnabled()) ? voice.getWander() : 0.0f;
                    voiceSample = cloud.renderSample(voice.getPitchRangeLow(), voice.getPitchRangeHigh(),
                                                     voice.getTimbre(), voice.getGroove(), spawnDensity,
                                                     voice.getVolume(), voice.getDissonance(),
                                                     voice.getRootSemitoneOffset());
                }
                mixedLeft += voiceSample.left;
                mixedRight += voiceSample.right;
            }

            constexpr float headroom = 0.5f;
            left[sample] = mixedLeft * headroom * masterVolume;
            if (right != nullptr) {
                right[sample] = mixedRight * headroom * masterVolume;
            }
        }

        if (right != nullptr) {
            const float room = reverbRoom_.load(std::memory_order_relaxed);
            const float decay = reverbDecay_.load(std::memory_order_relaxed);

            juce::Reverb::Parameters reverbParams;
            reverbParams.wetLevel = room * 0.5f;
            reverbParams.dryLevel = 0.5f;
            reverbParams.roomSize = juce::jlimit(0.0f, 1.0f, 0.25f + decay * 0.65f + room * 0.1f);
            reverbParams.damping = juce::jlimit(0.0f, 1.0f, 1.0f - decay * 0.75f);
            reverbParams.width = 1.0f;
            reverbParams.freezeMode = 0.0f;
            reverb_.setParameters(reverbParams);
            reverb_.processStereo(left, right, numSamples);
        }

        const float* recordChannels[2] = {left, right != nullptr ? right : left};
        recorder_.recordBlock(recordChannels, numSamples);
    }

    void getStateInformation(juce::MemoryBlock& destData) override {
        const auto text = ScenePresetStore::serialize(captureSceneState());
        destData.replaceAll(text.data(), text.size());
    }

    void setStateInformation(const void* data, int sizeInBytes) override {
        const std::string text(static_cast<const char*>(data), static_cast<size_t>(sizeInBytes));
        applySceneState(ScenePresetStore::deserialize(text));
    }

    // Reads every control's current value — everything a Scene (or a
    // plugin instance's own save/reload) captures, deliberately excluding
    // transport run/stop state (isPlaying_).
    SceneState captureSceneState() const {
        SceneState scene;
        for (std::size_t i = 0; i < voices_.size(); ++i) {
            const auto& voice = voices_[i];
            const auto& evolution = evolutionEngines_[i];
            auto& voiceScene = scene.voices[i];
            voiceScene.enabled = voice.isEnabled();
            voiceScene.volume = voice.getVolume();
            voiceScene.pitchLow = voice.getPitchRangeLow();
            voiceScene.pitchHigh = voice.getPitchRangeHigh();
            voiceScene.timbre = voice.getTimbre();
            voiceScene.motion = voice.getGroove();
            voiceScene.complexity = voice.getWander();
            voiceScene.dissonance = voice.getDissonance();
            voiceScene.rootSemitoneOffset = voice.getRootSemitoneOffset();
            voiceScene.busy = voice.getBusy();
            voiceScene.sustain = voice.getSustain();
            voiceScene.cleanliness = voice.getCleanliness();
            voiceScene.attack = voice.getAttack();
            voiceScene.volumeEvoEnabled = evolution.isVolumeEnabled();
            voiceScene.pitchRangeEvoEnabled = evolution.isPitchRangeEnabled();
            voiceScene.timbreEvoEnabled = evolution.isTimbreEnabled();
            voiceScene.motionEvoEnabled = evolution.isGrooveEnabled();
            voiceScene.complexityEvoEnabled = evolution.isWanderEnabled();
            voiceScene.dissonanceEvoEnabled = evolution.isDissonanceEnabled();
            voiceScene.busyEvoEnabled = evolution.isBusyEnabled();
            voiceScene.sustainEvoEnabled = evolution.isSustainEnabled();
            voiceScene.cleanlinessEvoEnabled = evolution.isCleanlinessEnabled();
            voiceScene.attackEvoEnabled = evolution.isAttackEnabled();
        }
        scene.evolutionAmount = evolutionAmount_.load(std::memory_order_relaxed);
        scene.evolutionSpeed = evolutionSpeed_.load(std::memory_order_relaxed);
        scene.reverbRoom = reverbRoom_.load(std::memory_order_relaxed);
        scene.reverbDecay = reverbDecay_.load(std::memory_order_relaxed);
        scene.masterVolume = masterVolume_.load(std::memory_order_relaxed);
        scene.tempo = tempo_.load(std::memory_order_relaxed);
        scene.meterNumerator = meterNumeratorDisplay_.load(std::memory_order_relaxed);
        scene.meterDenominator = meterDenominatorDisplay_.load(std::memory_order_relaxed);
        return scene;
    }

    // Writes a full snapshot back. Touches only atomics/VoiceModel/
    // EvolutionEngine — safe from any thread under the lock-free pattern
    // used throughout. Any attached editor re-syncs its own Components
    // from these on its next timer tick (see JerricanAudioProcessorEditor
    // ::timerCallback/refreshFromModel) rather than being poked directly
    // from here, so this stays editor-agnostic.
    void applySceneState(const SceneState& scene) {
        for (std::size_t i = 0; i < voices_.size(); ++i) {
            const auto& voiceScene = scene.voices[i];
            voices_[i].setEnabled(voiceScene.enabled);
            voices_[i].setVolume(voiceScene.volume);
            voices_[i].setPitchRange(voiceScene.pitchLow, voiceScene.pitchHigh);
            voices_[i].setTimbre(voiceScene.timbre);
            voices_[i].setGroove(voiceScene.motion);
            voices_[i].setWander(voiceScene.complexity);
            voices_[i].setDissonance(voiceScene.dissonance);
            voices_[i].setRootSemitoneOffset(voiceScene.rootSemitoneOffset);
            voices_[i].setBusy(voiceScene.busy);
            voices_[i].setSustain(voiceScene.sustain);
            voices_[i].setCleanliness(voiceScene.cleanliness);
            voices_[i].setAttack(voiceScene.attack);

            const float center = (voiceScene.pitchLow + voiceScene.pitchHigh) * 0.5f;
            const float width = voiceScene.pitchHigh - voiceScene.pitchLow;
            evolutionEngines_[i].resetTo(center, width, voiceScene.volume, voiceScene.timbre,
                                         voiceScene.motion, voiceScene.complexity,
                                         voiceScene.dissonance, voiceScene.busy, voiceScene.sustain,
                                         voiceScene.cleanliness, voiceScene.attack);
            evolutionEngines_[i].setVolumeEnabled(voiceScene.volumeEvoEnabled);
            evolutionEngines_[i].setPitchRangeEnabled(voiceScene.pitchRangeEvoEnabled);
            evolutionEngines_[i].setTimbreEnabled(voiceScene.timbreEvoEnabled);
            evolutionEngines_[i].setGrooveEnabled(voiceScene.motionEvoEnabled);
            evolutionEngines_[i].setWanderEnabled(voiceScene.complexityEvoEnabled);
            evolutionEngines_[i].setDissonanceEnabled(voiceScene.dissonanceEvoEnabled);
            evolutionEngines_[i].setBusyEnabled(voiceScene.busyEvoEnabled);
            evolutionEngines_[i].setSustainEnabled(voiceScene.sustainEvoEnabled);
            evolutionEngines_[i].setCleanlinessEnabled(voiceScene.cleanlinessEvoEnabled);
            evolutionEngines_[i].setAttackEnabled(voiceScene.attackEvoEnabled);
        }

        evolutionAmount_.store(scene.evolutionAmount, std::memory_order_relaxed);
        evolutionSpeed_.store(scene.evolutionSpeed, std::memory_order_relaxed);
        reverbRoom_.store(scene.reverbRoom, std::memory_order_relaxed);
        reverbDecay_.store(scene.reverbDecay, std::memory_order_relaxed);
        masterVolume_.store(scene.masterVolume, std::memory_order_relaxed);
        setTempo(scene.tempo);
        requestMeter(scene.meterNumerator, scene.meterDenominator);
    }

    void resetVoicesToInitialState() {
        for (size_t i = 0; i < voices_.size(); ++i) {
            const auto& initial = kInitialVoices[i];
            voices_[i].setEnabled(initial.enabled);
            voices_[i].setVolume(initial.volume);
            voices_[i].setPitchRange(initial.pitchLow, initial.pitchHigh);
            voices_[i].setTimbre(initial.timbre);
            voices_[i].setGroove(initial.groove);
            voices_[i].setWander(initial.wander);
            voices_[i].setDissonance(initial.dissonance);
            voices_[i].setRootSemitoneOffset(initial.rootSemitoneOffset);
            voices_[i].setBusy(initial.busy);
            voices_[i].setSustain(initial.sustain);
            voices_[i].setCleanliness(initial.cleanliness);
            voices_[i].setAttack(initial.attack);

            const float center = (initial.pitchLow + initial.pitchHigh) * 0.5f;
            const float width = initial.pitchHigh - initial.pitchLow;
            evolutionEngines_[i].resetTo(center, width, initial.volume, initial.timbre,
                                          initial.groove, initial.wander, initial.dissonance,
                                          initial.busy, initial.sustain, initial.cleanliness,
                                          initial.attack);
        }
        // Same "back to defaults" contract for the time signature.
        requestMeter(4, 4);
    }

    // The four transport actions — shared by a mouse click (via the
    // editor) and a MIDI-bound Play/Stop/Reset/Randomize (via
    // applyMidiTarget above), so both paths run identical logic.
    void handlePlayPressed() { isPlaying_.store(true, std::memory_order_relaxed); }

    void handleStopPressed() {
        // Just halts new grain spawning — existing grains ring out on
        // their own, and every knob stays exactly where it was, so
        // Play again picks up right where you left off.
        isPlaying_.store(false, std::memory_order_relaxed);
    }

    void handleResetPressed() { resetVoicesToInitialState(); }

    void handleRandomizePressed() {
        // A hard reroll of every lever, regardless of transport or
        // Evolution state — not a subtle nudge. Must also reset each
        // EvolutionEngine's internal target, otherwise (when Evolution
        // > 0) its next tick would immediately smooth the freshly
        // randomized values back toward its own stale pre-randomize
        // target, silently undoing this.
        for (size_t i = 0; i < voices_.size(); ++i) {
            const float a = randomizeRandom_.nextFloat01();
            const float b = randomizeRandom_.nextFloat01();
            const float low = std::min(a, b);
            const float high = std::max(a, b);
            const float timbre = randomizeRandom_.nextFloat01();
            const float groove = randomizeRandom_.nextFloat01();
            const float wander = randomizeRandom_.nextFloat01();
            const float volume = randomizeRandom_.nextFloat01();
            const float dissonance = randomizeRandom_.nextFloat01();
            // Bass-only: also reroll Busy/Sustain/Attack, its other
            // bespoke controls — meaningless for Spark/Haze, left
            // untouched. Ambient-only: same for Cleanliness.
            const float busy = i == 0 ? randomizeRandom_.nextFloat01() : voices_[i].getBusy();
            const float sustain = i == 0 ? randomizeRandom_.nextFloat01() : voices_[i].getSustain();
            const float attack = i == 0 ? randomizeRandom_.nextFloat01() : voices_[i].getAttack();
            const float cleanliness =
                i == 1 ? randomizeRandom_.nextFloat01() : voices_[i].getCleanliness();

            voices_[i].setPitchRange(low, high);
            voices_[i].setTimbre(timbre);
            voices_[i].setGroove(groove);
            voices_[i].setWander(wander);
            voices_[i].setVolume(volume);
            voices_[i].setDissonance(dissonance);
            voices_[i].setBusy(busy);
            voices_[i].setSustain(sustain);
            voices_[i].setCleanliness(cleanliness);
            voices_[i].setAttack(attack);

            const float center = (low + high) * 0.5f;
            const float width = high - low;
            evolutionEngines_[i].resetTo(center, width, volume, timbre, groove, wander, dissonance,
                                          busy, sustain, cleanliness, attack);
            grainClouds_[i].rerollDrift(low, high);
        }
    }

    // Toggled from the Editor's Record button. No file-save dialog on
    // start — like an instrument's own record button rather than a DAW
    // export flow, it starts immediately into a timestamped file under
    // ~/Music, and calling again finalizes it. Recording state is
    // independent of the transport: you can record silence (Stop) as
    // easily as a running performance. Returns false only on start
    // failure (e.g. couldn't create the file); a stop always succeeds.
    bool toggleRecording() {
        if (recorder_.isRecording()) {
            recorder_.stop();
            return true;
        }

        const auto directory =
            juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Jerrican Recordings");
        const auto filename =
            "Jerrican-" + juce::Time::getCurrentTime().formatted("%Y-%m-%d-%H%M%S") + ".wav";
        currentRecordingFile_ = directory.getChildFile(filename);
        return recorder_.startRecording(currentRecordingFile_, sampleRate_);
    }

    bool isRecording() const { return recorder_.isRecording(); }
    const juce::File& getCurrentRecordingFile() const { return currentRecordingFile_; }

    bool isPlaying() const { return isPlaying_.load(std::memory_order_relaxed); }
    int getFocusedVoiceIndex() const { return focusedVoiceIndex_.load(std::memory_order_relaxed); }

    bool hostSyncEnabled() const { return hostSyncEnabled_.load(std::memory_order_relaxed); }
    void setHostSyncEnabled(bool enabled) { hostSyncEnabled_.store(enabled, std::memory_order_relaxed); }

    VoiceModel& voice(std::size_t index) { return voices_[index]; }
    EvolutionEngine& evolutionEngine(std::size_t index) { return evolutionEngines_[index]; }

    std::atomic<float>& evolutionAmount() { return evolutionAmount_; }
    std::atomic<float>& evolutionSpeed() { return evolutionSpeed_; }
    std::atomic<float>& reverbRoom() { return reverbRoom_; }
    std::atomic<float>& reverbDecay() { return reverbDecay_; }
    std::atomic<float>& masterVolume() { return masterVolume_; }
    std::atomic<float>& tempo() { return tempo_; }

    void setTempo(float bpm) {
        tempo_.store(bpm, std::memory_order_relaxed);
        patternClock_.setBpm(bpm);
    }

    // Queues a meter change, consumed on the audio thread at the top of
    // the next processBlock (see applyMeterChange) — meter changes touch
    // non-atomic state, so they can't be applied directly from the
    // UI/MIDI thread. Falls back to 4/4 if the pair isn't one of the 7
    // supported meters.
    void requestMeter(int numerator, int denominator) {
        pendingMeterIndex_.store(MeterTable::findMeterIndex(numerator, denominator),
                                 std::memory_order_relaxed);
    }

    // Re-snaps Bass's pattern back to beat 1 without touching any knob or
    // Scene state — a lightweight "resync the clock" independent of Reset.
    void requestPhaseReset() { pendingPhaseReset_.store(true, std::memory_order_relaxed); }

    int meterNumeratorDisplay() const { return meterNumeratorDisplay_.load(std::memory_order_relaxed); }
    int meterDenominatorDisplay() const { return meterDenominatorDisplay_.load(std::memory_order_relaxed); }
    int currentSlot16Display() const { return currentSlot16Display_.load(std::memory_order_relaxed); }

    MidiBindingManager midiBindings_;
    MidiPresetStore midiPresetStore_{
        juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("Jerrican")
            .getChildFile("MidiPresets")
            .getFullPathName()
            .toStdString()};
    ScenePresetStore scenePresetStore_{
        juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("Jerrican")
            .getChildFile("Scenes")
            .getFullPathName()
            .toStdString()};
    // Which preset each popup is "on" — persisted here (rather than in the
    // popup, which is rebuilt from scratch every time its CallOutBox
    // reopens) so reopening after editing a loaded preset doesn't lose
    // track of which one to Override.
    juce::String currentMidiPresetName_;
    juce::String currentSceneName_;

private:
    // Regenerates Bass's accent profile for the new meter, reseats
    // bassGroovePattern_ onto it, and forces a fresh mask (the old one was
    // sized/shaped for the previous meter). Only ever called from the
    // audio thread (top of processBlock), so no locking needed.
    void applyMeterChange(int meterIndex) {
        const auto& meter = MeterTable::kMeters[static_cast<std::size_t>(meterIndex)];
        currentBassAccentProfile_ = MeterTable::generateBassAccentProfile(meter);
        bassGroovePattern_.setAccentProfile(&currentBassAccentProfile_, meter.totalSlots);
        bassGroovePattern_.forceRegenerateNextBoundary();
        currentMeterSlotCount_ = meter.totalSlots;
        currentSlot16_ = -1;
        currentSlot16Display_.store(-1, std::memory_order_relaxed);
        meterNumeratorDisplay_.store(meter.numerator, std::memory_order_relaxed);
        meterDenominatorDisplay_.store(meter.denominator, std::memory_order_relaxed);
    }

    // Snaps back to beat 1 (slot 0) without touching the meter or any
    // voice/knob state — just re-phases the existing pattern.
    void resetPatternPhase() {
        currentSlot16_ = -1;
        currentSlot16Display_.store(-1, std::memory_order_relaxed);
        bassGroovePattern_.forceRegenerateNextBoundary();
    }

    // Reads the host's transport once per block when Host Sync is
    // enabled: mirrors host Play/Stop into isPlaying_, snaps Bass's
    // pattern phase to the host's bar position on a transport start (or a
    // large ppq jump mid-playback, e.g. a host loop point), and
    // continuously locks tempo to the host's reported BPM. Standalone's
    // playhead never reports real transport/tempo data, so this is a
    // no-op there even if somehow left enabled. Ported from Marmite's
    // processHostSync.
    void processHostSync() {
        if (!hostSyncEnabled_.load(std::memory_order_relaxed)) {
            return;
        }
        auto* playHead = getPlayHead();
        if (playHead == nullptr) {
            return;
        }
        const auto position = playHead->getPosition();
        if (!position.hasValue()) {
            return;
        }

        const bool hostPlaying = position->getIsPlaying();
        bool shouldSnap = false;

        if (hostPlaying && !lastHostPlaying_) {
            shouldSnap = true;
        } else if (hostPlaying) {
            if (const auto ppq = position->getPpqPosition()) {
                const double barBeats = static_cast<double>(currentMeterSlotCount_) / 4.0;
                if (hasLastHostPpq_ && (*ppq < lastHostPpq_ - 1.0e-6 || *ppq - lastHostPpq_ > barBeats)) {
                    shouldSnap = true;
                }
                lastHostPpq_ = *ppq;
                hasLastHostPpq_ = true;
            }
        } else {
            hasLastHostPpq_ = false;
        }

        if (shouldSnap) {
            isPlaying_.store(true, std::memory_order_relaxed);
            const auto ppq = position->getPpqPosition();
            const auto barStart = position->getPpqPositionOfLastBarStart();
            if (ppq.hasValue() && barStart.hasValue()) {
                const double ppqIntoBar = *ppq - *barStart;
                const int rawSlot = static_cast<int>(ppqIntoBar * 4.0) % currentMeterSlotCount_;
                const int slot = ((rawSlot % currentMeterSlotCount_) + currentMeterSlotCount_) %
                                 currentMeterSlotCount_;
                currentSlot16_ = slot - 1;  // the next tick's ++ lands exactly on `slot`
                patternClock_.reset();
                bassGroovePattern_.forceRegenerateNextBoundary();
            }
        } else if (!hostPlaying && lastHostPlaying_) {
            isPlaying_.store(false, std::memory_order_relaxed);
        }
        lastHostPlaying_ = hostPlaying;

        if (const auto bpm = position->getBpm()) {
            const float bpmFloat = static_cast<float>(*bpm);
            tempo_.store(bpmFloat, std::memory_order_relaxed);
            patternClock_.setBpm(bpmFloat);
        }
    }

    std::array<VoiceModel, 4> voices_;
    std::array<GrainCloud, 4> grainClouds_;
    std::array<EvolutionEngine, 4> evolutionEngines_;
    juce::Reverb reverb_;

    // Declared (and thus constructed) before bassGroovePattern_, since it
    // depends on currentMeterSlotCount_/currentBassAccentProfile_ — C++
    // initializes members in declaration order regardless of the
    // initializer list's order.
    int currentMeterSlotCount_ = MeterTable::kMeters[MeterTable::kDefaultMeterIndex].totalSlots;
    MeterTable::AccentProfile currentBassAccentProfile_;
    PatternClock patternClock_;
    BassGroovePattern bassGroovePattern_;
    // Shared bar-position counter for Bass — advanced once per
    // PatternClock grid tick. Starts at -1 so the first tick's increment
    // lands on slot 0, not 1.
    int currentSlot16_ = -1;
    std::atomic<int> currentSlot16Display_{-1};
    std::atomic<int> meterNumeratorDisplay_{
        MeterTable::kMeters[MeterTable::kDefaultMeterIndex].numerator};
    std::atomic<int> meterDenominatorDisplay_{
        MeterTable::kMeters[MeterTable::kDefaultMeterIndex].denominator};
    std::atomic<int> pendingMeterIndex_{-1};
    std::atomic<bool> pendingPhaseReset_{false};
    std::atomic<float> tempo_{120.0f};

    std::atomic<bool> isPlaying_{false};
    std::atomic<bool> hostSyncEnabled_{false};
    // Host transport sync — plain, audio-thread-only, only ever read/
    // written from processBlock/processHostSync.
    bool lastHostPlaying_ = false;
    bool hasLastHostPpq_ = false;
    double lastHostPpq_ = 0.0;

    std::atomic<float> evolutionAmount_{0.0f};
    std::atomic<float> evolutionSpeed_{0.5f};
    std::atomic<float> reverbRoom_{0.0f};
    std::atomic<float> reverbDecay_{0.0f};
    std::atomic<float> masterVolume_{1.0f};
    std::atomic<int> focusedVoiceIndex_{0};
    FastRandom randomizeRandom_{0xc0ffeeu};
    juce::TimeSliceThread recordingThread_{"Jerrican Recording Thread"};
    AudioRecorder recorder_{recordingThread_};
    juce::File currentRecordingFile_;
    double sampleRate_ = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerricanAudioProcessor)
};

#include "JerricanEditor.h"

inline juce::AudioProcessorEditor* JerricanAudioProcessor::createEditor() {
    return new JerricanAudioProcessorEditor(*this);
}

// Standard JUCE plugin factory hook — called by every wrapper format
// (Standalone/AU/VST3) to create the one processor instance they host.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JerricanAudioProcessor(); }
