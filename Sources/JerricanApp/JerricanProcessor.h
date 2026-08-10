#pragma once

#include <JuceHeader.h>

#include <array>
#include <optional>

#include "AudioRecorder.h"
#include "EvolutionEngine.h"
#include "FastRandom.h"
#include "Grain.h"
#include "GrainCloud.h"
#include "MidiBindingManager.h"
#include "MidiPresetStore.h"
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
    struct InitialVoice {
        const char* name;
        bool enabled;
        float volume;
        float pitchLow;
        float pitchHigh;
        float timbre;
        float motion;
        float complexity;
        float dissonance;
        int rootSemitoneOffset;
        float minGrainDurationMs;
        float maxGrainDurationMs;
        Grain::Character character;
    };

    // Grain duration range is the main lever for a voice's fundamental
    // character (see GrainCloud) — short & sparse reads as pointillistic,
    // long & overlapping reads as a sustained drone. Complexity is tuned
    // per archetype to suit that duration range. Character::Plucked
    // (Pulse/Spark) gets a softened fast-attack envelope and a gentle
    // bright-to-dark filter sweep per grain; Character::Ambient (Drone/
    // Haze) is the original unfiltered symmetric envelope. Dissonance
    // near 0 for everyone by default so voices quantize mostly to their
    // (rooted) consonant scale out of the box; roots default to an A
    // major triad across the four voices.
    static constexpr std::array<InitialVoice, 4> kInitialVoices{
        {{"Pulse", true, 0.65f, 0.40f, 0.65f, 0.40f, 0.45f, 0.35f, 0.15f, 0, 200.0f, 500.0f,
          Grain::Character::Plucked},
         {"Drone", true, 0.60f, 0.05f, 0.20f, 0.15f, 0.10f, 0.12f, 0.15f, 0, 1500.0f, 4000.0f,
          Grain::Character::Ambient},
         {"Spark", true, 0.55f, 0.60f, 0.85f, 0.70f, 0.50f, 0.45f, 0.15f, 7, 150.0f, 400.0f,
          Grain::Character::Plucked},
         {"Haze", true, 0.50f, 0.15f, 0.35f, 0.75f, 0.20f, 0.15f, 0.15f, 4, 2000.0f, 5000.0f,
          Grain::Character::Ambient}}};

    JerricanAudioProcessor()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo())),
          voices_{VoiceModel(kInitialVoices[0].name, kInitialVoices[0].enabled,
                              kInitialVoices[0].volume, kInitialVoices[0].pitchLow,
                              kInitialVoices[0].pitchHigh, kInitialVoices[0].timbre,
                              kInitialVoices[0].motion, kInitialVoices[0].complexity,
                              kInitialVoices[0].dissonance, kInitialVoices[0].rootSemitoneOffset),
                  VoiceModel(kInitialVoices[1].name, kInitialVoices[1].enabled,
                              kInitialVoices[1].volume, kInitialVoices[1].pitchLow,
                              kInitialVoices[1].pitchHigh, kInitialVoices[1].timbre,
                              kInitialVoices[1].motion, kInitialVoices[1].complexity,
                              kInitialVoices[1].dissonance, kInitialVoices[1].rootSemitoneOffset),
                  VoiceModel(kInitialVoices[2].name, kInitialVoices[2].enabled,
                              kInitialVoices[2].volume, kInitialVoices[2].pitchLow,
                              kInitialVoices[2].pitchHigh, kInitialVoices[2].timbre,
                              kInitialVoices[2].motion, kInitialVoices[2].complexity,
                              kInitialVoices[2].dissonance, kInitialVoices[2].rootSemitoneOffset),
                  VoiceModel(kInitialVoices[3].name, kInitialVoices[3].enabled,
                              kInitialVoices[3].volume, kInitialVoices[3].pitchLow,
                              kInitialVoices[3].pitchHigh, kInitialVoices[3].timbre,
                              kInitialVoices[3].motion, kInitialVoices[3].complexity,
                              kInitialVoices[3].dissonance, kInitialVoices[3].rootSemitoneOffset)},
          grainClouds_{GrainCloud(0x1a2b3c4du, kInitialVoices[0].minGrainDurationMs,
                                   kInitialVoices[0].maxGrainDurationMs, kInitialVoices[0].character),
                       GrainCloud(0x5e6f7081u, kInitialVoices[1].minGrainDurationMs,
                                   kInitialVoices[1].maxGrainDurationMs, kInitialVoices[1].character),
                       GrainCloud(0x92a3b4c5u, kInitialVoices[2].minGrainDurationMs,
                                   kInitialVoices[2].maxGrainDurationMs, kInitialVoices[2].character),
                       GrainCloud(0xd6e7f809u, kInitialVoices[3].minGrainDurationMs,
                                   kInitialVoices[3].maxGrainDurationMs, kInitialVoices[3].character)},
          evolutionEngines_{EvolutionEngine(0x37a1f2c9u), EvolutionEngine(0x6b4d8e12u),
                             EvolutionEngine(0xa9c3f501u), EvolutionEngine(0xe1d47b6au)} {
        for (size_t i = 0; i < voices_.size(); ++i) {
            const auto& initial = kInitialVoices[i];
            const float center = (initial.pitchLow + initial.pitchHigh) * 0.5f;
            const float width = initial.pitchHigh - initial.pitchLow;
            evolutionEngines_[i].resetTo(center, width, initial.volume, initial.timbre,
                                          initial.motion, initial.complexity, initial.dissonance);
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
            case MidiTarget::VoiceMotion:
                voice.setMotion(value);
                evolution.resyncMotion(value);
                break;
            case MidiTarget::VoiceComplexity:
                voice.setComplexity(value);
                evolution.resyncComplexity(value);
                break;
            case MidiTarget::VoiceDissonance:
                voice.setDissonance(value);
                evolution.resyncDissonance(value);
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
                const bool on = !evolution.isMotionEnabled();
                evolution.setMotionEnabled(on);
                if (on) evolution.resyncMotion(voice.getMotion());
                break;
            }
            case MidiTarget::VoiceComplexityEvoToggle: {
                const bool on = !evolution.isComplexityEnabled();
                evolution.setComplexityEnabled(on);
                if (on) evolution.resyncComplexity(voice.getComplexity());
                break;
            }
            case MidiTarget::VoiceDissonanceEvoToggle: {
                const bool on = !evolution.isDissonanceEnabled();
                evolution.setDissonanceEnabled(on);
                if (on) evolution.resyncDissonance(voice.getDissonance());
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
        }
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        juce::ScopedNoDenormals noDenormals;

        for (const auto metadata : midiMessages) {
            handleMidiMessage(metadata.getMessage());
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
            float mixedLeft = 0.0f;
            float mixedRight = 0.0f;

            for (size_t i = 0; i < voices_.size(); ++i) {
                auto& voice = voices_[i];
                auto& cloud = grainClouds_[i];

                evolutionEngines_[i].update(voice, playing ? evolutionAmount : 0.0f, evolutionSpeed);

                const float complexity = (playing && voice.isEnabled()) ? voice.getComplexity() : 0.0f;

                const auto voiceSample =
                    cloud.renderSample(voice.getPitchRangeLow(), voice.getPitchRangeHigh(),
                                        voice.getTimbre(), voice.getMotion(), complexity,
                                        voice.getVolume(), voice.getDissonance(),
                                        voice.getRootSemitoneOffset());
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
            voiceScene.motion = voice.getMotion();
            voiceScene.complexity = voice.getComplexity();
            voiceScene.dissonance = voice.getDissonance();
            voiceScene.rootSemitoneOffset = voice.getRootSemitoneOffset();
            voiceScene.volumeEvoEnabled = evolution.isVolumeEnabled();
            voiceScene.pitchRangeEvoEnabled = evolution.isPitchRangeEnabled();
            voiceScene.timbreEvoEnabled = evolution.isTimbreEnabled();
            voiceScene.motionEvoEnabled = evolution.isMotionEnabled();
            voiceScene.complexityEvoEnabled = evolution.isComplexityEnabled();
            voiceScene.dissonanceEvoEnabled = evolution.isDissonanceEnabled();
        }
        scene.evolutionAmount = evolutionAmount_.load(std::memory_order_relaxed);
        scene.evolutionSpeed = evolutionSpeed_.load(std::memory_order_relaxed);
        scene.reverbRoom = reverbRoom_.load(std::memory_order_relaxed);
        scene.reverbDecay = reverbDecay_.load(std::memory_order_relaxed);
        scene.masterVolume = masterVolume_.load(std::memory_order_relaxed);
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
            voices_[i].setMotion(voiceScene.motion);
            voices_[i].setComplexity(voiceScene.complexity);
            voices_[i].setDissonance(voiceScene.dissonance);
            voices_[i].setRootSemitoneOffset(voiceScene.rootSemitoneOffset);

            const float center = (voiceScene.pitchLow + voiceScene.pitchHigh) * 0.5f;
            const float width = voiceScene.pitchHigh - voiceScene.pitchLow;
            evolutionEngines_[i].resetTo(center, width, voiceScene.volume, voiceScene.timbre,
                                         voiceScene.motion, voiceScene.complexity,
                                         voiceScene.dissonance);
            evolutionEngines_[i].setVolumeEnabled(voiceScene.volumeEvoEnabled);
            evolutionEngines_[i].setPitchRangeEnabled(voiceScene.pitchRangeEvoEnabled);
            evolutionEngines_[i].setTimbreEnabled(voiceScene.timbreEvoEnabled);
            evolutionEngines_[i].setMotionEnabled(voiceScene.motionEvoEnabled);
            evolutionEngines_[i].setComplexityEnabled(voiceScene.complexityEvoEnabled);
            evolutionEngines_[i].setDissonanceEnabled(voiceScene.dissonanceEvoEnabled);
        }

        evolutionAmount_.store(scene.evolutionAmount, std::memory_order_relaxed);
        evolutionSpeed_.store(scene.evolutionSpeed, std::memory_order_relaxed);
        reverbRoom_.store(scene.reverbRoom, std::memory_order_relaxed);
        reverbDecay_.store(scene.reverbDecay, std::memory_order_relaxed);
        masterVolume_.store(scene.masterVolume, std::memory_order_relaxed);
    }

    void resetVoicesToInitialState() {
        for (size_t i = 0; i < voices_.size(); ++i) {
            const auto& initial = kInitialVoices[i];
            voices_[i].setEnabled(initial.enabled);
            voices_[i].setVolume(initial.volume);
            voices_[i].setPitchRange(initial.pitchLow, initial.pitchHigh);
            voices_[i].setTimbre(initial.timbre);
            voices_[i].setMotion(initial.motion);
            voices_[i].setComplexity(initial.complexity);
            voices_[i].setDissonance(initial.dissonance);
            voices_[i].setRootSemitoneOffset(initial.rootSemitoneOffset);

            const float center = (initial.pitchLow + initial.pitchHigh) * 0.5f;
            const float width = initial.pitchHigh - initial.pitchLow;
            evolutionEngines_[i].resetTo(center, width, initial.volume, initial.timbre,
                                          initial.motion, initial.complexity, initial.dissonance);
        }
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
            const float motion = randomizeRandom_.nextFloat01();
            const float complexity = randomizeRandom_.nextFloat01();
            const float volume = randomizeRandom_.nextFloat01();
            const float dissonance = randomizeRandom_.nextFloat01();

            voices_[i].setPitchRange(low, high);
            voices_[i].setTimbre(timbre);
            voices_[i].setMotion(motion);
            voices_[i].setComplexity(complexity);
            voices_[i].setVolume(volume);
            voices_[i].setDissonance(dissonance);

            const float center = (low + high) * 0.5f;
            const float width = high - low;
            evolutionEngines_[i].resetTo(center, width, volume, timbre, motion, complexity,
                                          dissonance);
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
    // Mirrors the host's transport Play/Stop into isPlaying_ once per
    // block, when Host Sync is enabled — so recording with a host count-
    // in starts grain spawning on the same downbeat. Jerrican has no
    // tempo/clock concept, so unlike Marmite's processHostSync there's no
    // phase to snap or tempo to lock, just transport state. Standalone's
    // playhead never reports real transport data, so this is a no-op
    // there even if somehow left enabled.
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
        isPlaying_.store(position->getIsPlaying(), std::memory_order_relaxed);
    }

    std::array<VoiceModel, 4> voices_;
    std::array<GrainCloud, 4> grainClouds_;
    std::array<EvolutionEngine, 4> evolutionEngines_;
    juce::Reverb reverb_;
    std::atomic<bool> isPlaying_{false};
    std::atomic<bool> hostSyncEnabled_{false};
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
