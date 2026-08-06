#include <JuceHeader.h>

#include <BinaryData.h>

#include <algorithm>
#include <array>
#include <functional>
#include <vector>

#include "AudioRecorder.h"
#include "EvolutionEngine.h"
#include "FastRandom.h"
#include "Grain.h"
#include "GrainCloud.h"
#include "JerricanLookAndFeel.h"
#include "JerricanTheme.h"
#include "MidiBindingManager.h"
#include "MidiPresetStore.h"
#include "ScenePresetStore.h"
#include "SceneState.h"
#include "VoiceModel.h"

// Content shown in the help popup (launched from the "?" button next to
// Output). Read-only, scrollable if the window is short, styled to match
// the rest of the app rather than the OS-native AlertWindow look.
class HelpContent : public juce::Component {
public:
    HelpContent() {
        editor_.setMultiLine(true);
        editor_.setReadOnly(true);
        editor_.setScrollbarsShown(true);
        editor_.setCaretVisible(false);
        editor_.setPopupMenuEnabled(false);
        editor_.setColour(juce::TextEditor::backgroundColourId, JerricanTheme::panel);
        editor_.setColour(juce::TextEditor::textColourId, JerricanTheme::textPrimary);
        editor_.setColour(juce::TextEditor::outlineColourId, JerricanTheme::panelBorder);
        editor_.setColour(juce::TextEditor::focusedOutlineColourId, JerricanTheme::panelBorder);
        editor_.setFont(juce::Font(juce::FontOptions(12.5f)));

        const juce::String bodyText =
            "Jerrican\n"
            "A self-composing granular instrument by Alban Bailly.\n\n"
            "Composes itself continuously from a field of possibilities you "
            "shape, rather than a fixed sequence: each voice is a cloud of "
            "short synthesized grains whose pitch, timbre, and timing are "
            "drawn fresh from the ranges you set.\n\n"
            "TRANSPORT\n"
            "Play - starts new grains spawning.\n"
            "Stop - halts new grains (existing ones ring out); every knob "
            "stays put, so Play picks up right where you left off.\n"
            "Reset - snaps every voice back to its starting values.\n"
            "Randomize - rerolls every voice's levers, whether playing or "
            "stopped.\n"
            "Evolution Amount - how often a voice's levers wander on their "
            "own while playing; 0, or Stop, leaves them alone.\n"
            "Evolution Speed - how fast a change glides in once Amount picks "
            "one - near-instant to almost imperceptibly slow.\n\n"
            "PER-VOICE CONTROLS\n"
            "Enabled - mutes/unmutes the voice.  Volume - overall level.\n"
            "Pitch Range - band grains draw their pitch from.\n"
            "Timbre - blends grain character, smooth to metallic/textured.\n"
            "Motion - how far the sampling point wanders within Pitch Range.\n"
            "Complexity - how dense the grain cloud is.\n"
            "Dissonance - 0 quantizes to this voice's scale so it harmonizes "
            "with others; 1 is fully free/chromatic.\n"
            "Key - which note that scale is rooted on; give two voices "
            "different Keys (e.g. a fifth apart) to build a deliberate "
            "chord at low Dissonance instead of unison. Not affected by "
            "Evolution or Randomize.\n\n"
            "Each control has its own small Evolution switch (on by default, "
            "teal) - turn one off to keep it under manual control while the "
            "rest keep drifting.\n\n"
            "REVERB\n"
            "Room - a global send amount/space size for the whole mix. "
            "Decay - how long the tail rings out. Both are 0 by default "
            "(no reverb, output unchanged) and never evolve on their own.\n\n"
            "VOLUME\n"
            "Master output level, applied after everything else. Full by "
            "default (unchanged output).\n\n"
            "OUTPUT / MIDI\n"
            "Output chooses which audio device the instrument plays through. "
            "MIDI In connects a controller; Bindings opens MIDI Learn, where "
            "each per-voice control applies to whichever voice is currently "
            "focused (switch focus with Voice Select pads). Transport "
            "(Play/Stop/Reset/Randomize) is bindable too, as a global "
            "action rather than a per-voice one.\n\n"
            "RECORDING\n"
            "Record captures the exact final mix (everything, post-Reverb) "
            "to a timestamped WAV under ~/Music/Jerrican Recordings - click "
            "again to stop and finalize the file. Independent of the "
            "transport: you can record silence as easily as a running "
            "performance. \"Open Folder\" reveals the most recent "
            "recording in Finder.\n\n" +
            juce::String(juce::CharPointer_UTF8("\xc2\xa9")) +
            " 2026 Alban Bailly. All rights reserved.";

        editor_.setText(bodyText, false);
        addAndMakeVisible(editor_);
    }

    void resized() override { editor_.setBounds(getLocalBounds()); }

private:
    juce::TextEditor editor_;
};

class JerricanEditor : public juce::AudioAppComponent,
                        private juce::Button::Listener,
                        private juce::Slider::Listener,
                        private juce::Timer,
                        private juce::MidiInputCallback {
public:
    JerricanEditor()
        : voices_{VoiceModel(kInitialVoices[0].name, kInitialVoices[0].enabled,
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

        setLookAndFeel(&lookAndFeel_);

        logoImage_.setImage(
            juce::ImageCache::getFromMemory(BinaryData::AppIcon_png, BinaryData::AppIcon_pngSize));
        addAndMakeVisible(logoImage_);

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Jerrican", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(juce::FontOptions(28.0f)).withStyle(juce::Font::bold));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        titleLabel.setColour(juce::Label::textColourId, JerricanTheme::textPrimary);

        addAndMakeVisible(subtitleLabel);
        subtitleLabel.setText("A self-composing granular instrument by Alban Bailly",
                              juce::dontSendNotification);
        subtitleLabel.setFont(juce::Font(juce::FontOptions(16.0f)));
        subtitleLabel.setJustificationType(juce::Justification::centredLeft);
        subtitleLabel.setColour(juce::Label::textColourId, JerricanTheme::textSecondary);

        addAndMakeVisible(recordButton);
        recordButton.setButtonText("Record");
        recordButton.setClickingTogglesState(false);
        recordButton.onClick = [this] { toggleRecording(); };
        recordingThread_.startThread();

        addAndMakeVisible(scenesButton);
        scenesButton.setButtonText("Scenes");
        scenesButton.onClick = [this] { showScenesPopup(); };

        addAndMakeVisible(bindingsButton);
        bindingsButton.setButtonText("Bindings");
        bindingsButton.onClick = [this] { showBindingsPopup(); };

        addAndMakeVisible(midiInputLabel);
        midiInputLabel.setText("MIDI In", juce::dontSendNotification);
        midiInputLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        midiInputLabel.setColour(juce::Label::textColourId, JerricanTheme::textSecondary);

        addAndMakeVisible(midiInputDeviceBox);

        addAndMakeVisible(helpButton);
        helpButton.setButtonText("?");
        helpButton.onClick = [this] { showHelpPopup(); };

        addAndMakeVisible(outputLabel);
        outputLabel.setText("Output", juce::dontSendNotification);
        outputLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        outputLabel.setColour(juce::Label::textColourId, JerricanTheme::textSecondary);

        addAndMakeVisible(outputDeviceBox);

        addAndMakeVisible(playButton);
        playButton.setButtonText("Play");
        playButton.setClickingTogglesState(false);
        playButton.addListener(this);

        addAndMakeVisible(stopButton);
        stopButton.setButtonText("Stop");
        stopButton.setEnabled(false);  // nothing to stop until Play is pressed
        stopButton.addListener(this);

        addAndMakeVisible(resetButton);
        resetButton.setButtonText("Reset");
        resetButton.addListener(this);

        addAndMakeVisible(randomizeButton);
        randomizeButton.setButtonText("Randomize");
        randomizeButton.addListener(this);

        addAndMakeVisible(evolutionTitleLabel);
        evolutionTitleLabel.setText("Evolution", juce::dontSendNotification);
        evolutionTitleLabel.setFont(juce::Font(juce::FontOptions(13.0f)).withStyle(juce::Font::bold));
        evolutionTitleLabel.setColour(juce::Label::textColourId, JerricanTheme::textPrimary);
        evolutionTitleLabel.setJustificationType(juce::Justification::centred);

        setUpTransportKnob(evolutionAmountSlider, evolutionAmountLabel, "Amount");
        evolutionAmountSlider.setValue(0.0);

        setUpTransportKnob(evolutionSpeedSlider, evolutionSpeedLabel, "Speed");
        evolutionSpeedSlider.setValue(0.5);

        addAndMakeVisible(reverbTitleLabel);
        reverbTitleLabel.setText("Reverb", juce::dontSendNotification);
        reverbTitleLabel.setFont(juce::Font(juce::FontOptions(13.0f)).withStyle(juce::Font::bold));
        reverbTitleLabel.setColour(juce::Label::textColourId, JerricanTheme::textPrimary);
        reverbTitleLabel.setJustificationType(juce::Justification::centred);

        // Deliberately plain amber knobs (no "evolutionKnob" name tag) —
        // Reverb sits outside the Evolution mechanic entirely, it never
        // drifts on its own.
        setUpTransportKnob(roomSlider, roomLabel, "Room", false);
        roomSlider.setValue(0.0);

        setUpTransportKnob(decaySlider, decayLabel, "Decay", false);
        decaySlider.setValue(0.0);

        addAndMakeVisible(masterVolumeTitleLabel);
        masterVolumeTitleLabel.setText("Volume", juce::dontSendNotification);
        masterVolumeTitleLabel.setFont(juce::Font(juce::FontOptions(13.0f)).withStyle(juce::Font::bold));
        masterVolumeTitleLabel.setColour(juce::Label::textColourId, JerricanTheme::textPrimary);
        masterVolumeTitleLabel.setJustificationType(juce::Justification::centred);

        // Default 1.0 (unity) so out-of-the-box output is identical to
        // before this control existed — same "neutral default" pattern
        // Reverb used with its 0-default, just inverted since this knob's
        // neutral position is fully up rather than fully down.
        // Blank sub-label — with only one knob in this block, the "Volume"
        // group title above it already names it; repeating it directly
        // underneath read as redundant (unlike Evolution/Reverb's two
        // differently-named knobs, which each need their own label).
        setUpTransportKnob(masterVolumeSlider, masterVolumeLabel, "", false);
        masterVolumeSlider.setValue(1.0);

        addAndMakeVisible(statusLabel);
        statusLabel.setText("Transport idle", juce::dontSendNotification);
        statusLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
        statusLabel.setColour(juce::Label::textColourId, JerricanTheme::textSecondary);

        addAndMakeVisible(openRecordingFolderButton);
        openRecordingFolderButton.setButtonText("Open Folder");
        openRecordingFolderButton.setVisible(false);
        openRecordingFolderButton.onClick = [this] { currentRecordingFile_.revealToUser(); };

        for (size_t i = 0; i < voices_.size(); ++i) {
            voiceRows_[i] = std::make_unique<VoiceRow>(voices_[i], evolutionEngines_[i], this);
            addAndMakeVisible(*voiceRows_[i]);
        }

        updateStatus();

        setSize(1310, 900);
        setAudioChannels(0, 2);
        populateOutputDeviceBox();
        outputDeviceBox.onChange = [this] { outputDeviceSelected(); };
        populateMidiInputBox();
        midiInputDeviceBox.onChange = [this] { midiInputDeviceSelected(); };
        startTimerHz(30);
    }

    ~JerricanEditor() override {
        if (currentMidiInputId_.isNotEmpty()) {
            deviceManager.removeMidiInputDeviceCallback(currentMidiInputId_, this);
        }
        recorder_.stop();
        recordingThread_.stopThread(2000);
        shutdownAudio();
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override { g.fillAll(JerricanTheme::background); }

    void resized() override {
        logoImage_.setBounds(40, 16, 34, 34);
        titleLabel.setBounds(82, 16, 300, 34);
        subtitleLabel.setBounds(82, 48, getWidth() - 340, 22);

        outputLabel.setBounds(getWidth() - 260, 16, 220, 14);
        helpButton.setBounds(getWidth() - 292, 32, 24, 24);
        outputDeviceBox.setBounds(getWidth() - 260, 32, 220, 24);

        // MIDI cluster sits left of Help/Output, same top-aligned row.
        midiInputLabel.setBounds(getWidth() - 460, 16, 160, 14);
        midiInputDeviceBox.setBounds(getWidth() - 460, 32, 160, 24);
        bindingsButton.setBounds(getWidth() - 548, 32, 80, 24);
        scenesButton.setBounds(getWidth() - 626, 32, 70, 24);
        recordButton.setBounds(getWidth() - 716, 32, 80, 24);

        // Shared bottom baseline: every transport control's bottom edge
        // sits on this line, even though the Evolution knobs are taller
        // (label + circle + value box) than the transport buttons.
        const int bottomY = getHeight() - 20;

        // 2x2 transport grid: Play/Stop on top, Reset/Randomize below,
        // bottom row's bottom edge on the shared baseline.
        constexpr int buttonColumnWidth = 150;
        constexpr int buttonColumnGap = 10;
        constexpr int buttonRowHeight = 36;
        constexpr int buttonRowGap = 8;
        const int buttonCol1X = 40;
        const int buttonCol2X = buttonCol1X + buttonColumnWidth + buttonColumnGap;
        const int buttonRow2Y = bottomY - buttonRowHeight;
        const int buttonRow1Y = buttonRow2Y - buttonRowGap - buttonRowHeight;

        playButton.setBounds(buttonCol1X, buttonRow1Y, buttonColumnWidth, buttonRowHeight);
        stopButton.setBounds(buttonCol2X, buttonRow1Y, buttonColumnWidth, buttonRowHeight);
        resetButton.setBounds(buttonCol1X, buttonRow2Y, buttonColumnWidth, buttonRowHeight);
        randomizeButton.setBounds(buttonCol2X, buttonRow2Y, buttonColumnWidth, buttonRowHeight);

        // Evolution block: a title above two knobs (Amount, Speed), whole
        // block's bottom (the knobs' value boxes) sitting on bottomY.
        constexpr int knobSize = 56;
        constexpr int knobLabelHeight = 14;
        constexpr int knobTextBoxHeight = 16;
        constexpr int knobColumnWidth = 84;
        // Shifted left from the original 500 to make room for the Volume
        // block (one knob-column + gap, ~114px) at the right without
        // pushing the status label further right than before.
        constexpr int evolutionBlockX = 386;
        constexpr int evolutionBlockWidth = knobColumnWidth * 2;
        constexpr int evolutionTitleHeight = 16;
        constexpr int evolutionTitleGap = 4;

        const int knobBoxTop = bottomY - knobSize - knobTextBoxHeight;
        const int knobLabelTop = knobBoxTop - knobLabelHeight - 2;
        const int evolutionTitleTop = knobLabelTop - evolutionTitleGap - evolutionTitleHeight;

        evolutionTitleLabel.setBounds(evolutionBlockX, evolutionTitleTop, evolutionBlockWidth,
                                      evolutionTitleHeight);

        const int amountColumnX = evolutionBlockX;
        const int speedColumnX = evolutionBlockX + knobColumnWidth;

        evolutionAmountLabel.setBounds(amountColumnX, knobLabelTop, knobColumnWidth,
                                       knobLabelHeight);
        evolutionAmountSlider.setBounds(amountColumnX + (knobColumnWidth - knobSize) / 2, knobBoxTop,
                                        knobSize, knobSize + knobTextBoxHeight);

        evolutionSpeedLabel.setBounds(speedColumnX, knobLabelTop, knobColumnWidth, knobLabelHeight);
        evolutionSpeedSlider.setBounds(speedColumnX + (knobColumnWidth - knobSize) / 2, knobBoxTop,
                                       knobSize, knobSize + knobTextBoxHeight);

        // Reverb block: same shape as Evolution's (title over two knobs),
        // same bottom baseline, placed immediately to its right. Plain
        // amber (not teal) since Reverb never drifts on its own.
        const int reverbBlockX = evolutionBlockX + evolutionBlockWidth + 30;
        const int reverbBlockWidth = knobColumnWidth * 2;

        reverbTitleLabel.setBounds(reverbBlockX, evolutionTitleTop, reverbBlockWidth,
                                   evolutionTitleHeight);

        const int roomColumnX = reverbBlockX;
        const int decayColumnX = reverbBlockX + knobColumnWidth;

        roomLabel.setBounds(roomColumnX, knobLabelTop, knobColumnWidth, knobLabelHeight);
        roomSlider.setBounds(roomColumnX + (knobColumnWidth - knobSize) / 2, knobBoxTop, knobSize,
                             knobSize + knobTextBoxHeight);

        decayLabel.setBounds(decayColumnX, knobLabelTop, knobColumnWidth, knobLabelHeight);
        decaySlider.setBounds(decayColumnX + (knobColumnWidth - knobSize) / 2, knobBoxTop, knobSize,
                              knobSize + knobTextBoxHeight);

        // Master Volume block: same title-over-knob shape as Evolution and
        // Reverb, single knob, placed immediately to Reverb's right.
        const int volumeBlockX = reverbBlockX + reverbBlockWidth + 30;
        const int volumeBlockWidth = knobColumnWidth;

        masterVolumeTitleLabel.setBounds(volumeBlockX, evolutionTitleTop, volumeBlockWidth,
                                         evolutionTitleHeight);
        masterVolumeLabel.setBounds(volumeBlockX, knobLabelTop, volumeBlockWidth, knobLabelHeight);
        masterVolumeSlider.setBounds(volumeBlockX + (volumeBlockWidth - knobSize) / 2, knobBoxTop,
                                     knobSize, knobSize + knobTextBoxHeight);

        // Floated from the right edge (rather than left-anchored after
        // the knob blocks) so it stays put regardless of window width —
        // a fixed left-anchor + offset for the button broke as soon as
        // the window got narrower than expected (e.g. clamped to fit a
        // smaller screen).
        constexpr int statusBlockWidth = 300;
        const int statusRight = getWidth() - 40;
        statusLabel.setJustificationType(juce::Justification::centredRight);
        statusLabel.setBounds(statusRight - statusBlockWidth, bottomY - 48, statusBlockWidth, 22);
        openRecordingFolderButton.setBounds(statusRight - 100, bottomY - 22, 100, 22);

        // 2x2 grid of voice cards, filling the space between the header and
        // the transport row.
        constexpr int columns = 2;
        constexpr int rows = 2;
        const int gridLeft = 40;
        const int gridTop = 86;
        const int gridRight = getWidth() - 40;
        const int gridBottom = bottomY - 130;
        constexpr int gap = 16;

        const int cardWidth = (gridRight - gridLeft - gap * (columns - 1)) / columns;
        const int cardHeight = (gridBottom - gridTop - gap * (rows - 1)) / rows;

        for (size_t i = 0; i < voiceRows_.size(); ++i) {
            const int column = static_cast<int>(i) % columns;
            const int row = static_cast<int>(i) / columns;
            voiceRows_[i]->setBounds(gridLeft + column * (cardWidth + gap),
                                      gridTop + row * (cardHeight + gap), cardWidth, cardHeight);
        }
    }

    void setUpTransportKnob(juce::Slider& slider, juce::Label& label, const char* labelText,
                            bool evolutionThemed = true) {
        addAndMakeVisible(label);
        label.setText(labelText, juce::dontSendNotification);
        label.setFont(juce::Font(juce::FontOptions(12.0f)));
        label.setColour(juce::Label::textColourId, JerricanTheme::textSecondary);
        label.setJustificationType(juce::Justification::centred);

        addAndMakeVisible(slider);
        if (evolutionThemed) {
            slider.setName("evolutionKnob");
        }
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRange(0.0, 1.0);
        slider.setNumDecimalPlacesToDisplay(2);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
        slider.addListener(this);
    }

    void showHelpPopup() {
        auto content = std::make_unique<HelpContent>();
        content->setSize(480, 620);
        juce::CallOutBox::launchAsynchronously(std::move(content), helpButton.getScreenBounds(),
                                                nullptr);
    }

    // Toggled from the Record button. No file-save dialog on start — like
    // an instrument's own record button rather than a DAW export flow, it
    // starts immediately into a timestamped file under ~/Music, and Stop
    // finalizes it. Recording state is independent of the transport: you
    // can record silence (Stop) as easily as a running performance.
    void toggleRecording() {
        if (recorder_.isRecording()) {
            recorder_.stop();
            recordButton.setToggleState(false, juce::dontSendNotification);
            statusLabel.setText("Recording saved", juce::dontSendNotification);
            openRecordingFolderButton.setVisible(true);
            return;
        }

        openRecordingFolderButton.setVisible(false);
        const auto directory =
            juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Jerrican Recordings");
        const auto filename =
            "Jerrican-" + juce::Time::getCurrentTime().formatted("%Y-%m-%d-%H%M%S") + ".wav";
        currentRecordingFile_ = directory.getChildFile(filename);

        if (recorder_.startRecording(currentRecordingFile_, sampleRate_)) {
            recordButton.setToggleState(true, juce::dontSendNotification);
            statusLabel.setText("Recording...", juce::dontSendNotification);
        } else {
            statusLabel.setText("Couldn't start recording", juce::dontSendNotification);
        }
    }

    void populateOutputDeviceBox() {
        outputDeviceBox.clear(juce::dontSendNotification);

        auto* deviceType = deviceManager.getCurrentDeviceTypeObject();
        if (deviceType == nullptr) {
            return;
        }

        deviceType->scanForDevices();
        const auto deviceNames = deviceType->getDeviceNames(false);
        for (int i = 0; i < deviceNames.size(); ++i) {
            outputDeviceBox.addItem(deviceNames[i], i + 1);
        }

        if (auto* currentDevice = deviceManager.getCurrentAudioDevice()) {
            const int index = deviceNames.indexOf(currentDevice->getName());
            if (index >= 0) {
                outputDeviceBox.setSelectedId(index + 1, juce::dontSendNotification);
            }
        }
    }

    void outputDeviceSelected() {
        auto setup = deviceManager.getAudioDeviceSetup();
        setup.outputDeviceName = outputDeviceBox.getText();
        deviceManager.setAudioDeviceSetup(setup, true);
    }

    void populateMidiInputBox() {
        midiInputDeviceBox.clear(juce::dontSendNotification);
        midiInputDeviceBox.addItem("None", 1);

        const auto devices = juce::MidiInput::getAvailableDevices();
        for (int i = 0; i < devices.size(); ++i) {
            midiInputDeviceBox.addItem(devices[i].name, i + 2);
        }
        midiInputDeviceBox.setSelectedId(1, juce::dontSendNotification);
    }

    void midiInputDeviceSelected() {
        if (currentMidiInputId_.isNotEmpty()) {
            deviceManager.setMidiInputDeviceEnabled(currentMidiInputId_, false);
            deviceManager.removeMidiInputDeviceCallback(currentMidiInputId_, this);
            currentMidiInputId_ = {};
        }

        const int selectedId = midiInputDeviceBox.getSelectedId();
        if (selectedId <= 1) {
            return;  // "None"
        }

        const auto devices = juce::MidiInput::getAvailableDevices();
        const int index = selectedId - 2;
        if (index < 0 || index >= devices.size()) {
            return;
        }

        currentMidiInputId_ = devices[index].identifier;
        deviceManager.setMidiInputDeviceEnabled(currentMidiInputId_, true);
        deviceManager.addMidiInputDeviceCallback(currentMidiInputId_, this);
    }

    // Runs on the MIDI thread. Every write below goes through the same
    // atomic VoiceModel/EvolutionEngine setters already used from the UI
    // thread (see VoiceRow::sliderValueChanged) — safe under the
    // lock-free pattern already established throughout this codebase, no
    // new synchronization needed.
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message) override {
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
                // knob keeps turning but the range stops moving. This
                // way, value=0 reaches the lowest possible center and
                // value=1 reaches the highest, with nothing left over.
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
            // Each flips the matching Evolution opt-in/out flag for the
            // focused voice and, when switching on, resyncs it — the
            // same logic VoiceRow::buttonClicked's manual toggle click
            // already applies, just against the focused voice's engine
            // instead of a fixed one. Only touches EvolutionEngine's
            // atomics, so — unlike transport — this is safe to apply
            // directly from the MIDI thread.
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
            // Transport is the one exception to "just touch atomics" —
            // handling a press means touching JUCE Components
            // (playButton/stopButton/statusLabel, and Reset/Randomize's
            // slider refreshes), which are only safe on the message
            // thread. Dispatch there via callAsync, guarded by a
            // SafePointer so a message still in flight when the app
            // quits becomes a no-op instead of touching a freed
            // JerricanEditor (same pattern used for the Save-As dialog
            // in MidiBindingsPopup).
            case MidiTarget::TransportPlay: {
                juce::Component::SafePointer<JerricanEditor> safeThis(this);
                juce::MessageManager::callAsync([safeThis] {
                    if (safeThis != nullptr) safeThis->handlePlayPressed();
                });
                break;
            }
            case MidiTarget::TransportStop: {
                juce::Component::SafePointer<JerricanEditor> safeThis(this);
                juce::MessageManager::callAsync([safeThis] {
                    if (safeThis != nullptr) safeThis->handleStopPressed();
                });
                break;
            }
            case MidiTarget::TransportReset: {
                juce::Component::SafePointer<JerricanEditor> safeThis(this);
                juce::MessageManager::callAsync([safeThis] {
                    if (safeThis != nullptr) safeThis->handleResetPressed();
                });
                break;
            }
            case MidiTarget::TransportRandomize: {
                juce::Component::SafePointer<JerricanEditor> safeThis(this);
                juce::MessageManager::callAsync([safeThis] {
                    if (safeThis != nullptr) safeThis->handleRandomizePressed();
                });
                break;
            }
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

    void showBindingsPopup() {
        auto content = std::make_unique<MidiBindingsPopup>(this);
        content->setSize(440, 720);
        juce::CallOutBox::launchAsynchronously(std::move(content), bindingsButton.getScreenBounds(),
                                               nullptr);
    }

    void showScenesPopup() {
        auto content = std::make_unique<ScenesPopup>(this);
        content->setSize(400, 140);
        juce::CallOutBox::launchAsynchronously(std::move(content), scenesButton.getScreenBounds(),
                                               nullptr);
    }

    // Reads every control's current value — everything a Scene captures,
    // deliberately excluding transport run/stop state (isPlaying_).
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

    // Writes a full snapshot back — only ever called from the message
    // thread (the Scenes popup's UI), same as Randomize, so no threading
    // concerns despite touching Components (via refreshFromModel()/
    // refreshEvolutionToggles()) as well as atomics. Transport run/stop
    // state is untouched, matching what a Scene does and doesn't capture.
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

            voiceRows_[i]->refreshFromModel();
            voiceRows_[i]->refreshEvolutionToggles();
        }

        evolutionAmount_.store(scene.evolutionAmount, std::memory_order_relaxed);
        evolutionSpeed_.store(scene.evolutionSpeed, std::memory_order_relaxed);
        reverbRoom_.store(scene.reverbRoom, std::memory_order_relaxed);
        reverbDecay_.store(scene.reverbDecay, std::memory_order_relaxed);
        masterVolume_.store(scene.masterVolume, std::memory_order_relaxed);

        refreshGlobalKnobFromAtomic(evolutionAmountSlider, evolutionAmount_);
        refreshGlobalKnobFromAtomic(evolutionSpeedSlider, evolutionSpeed_);
        refreshGlobalKnobFromAtomic(roomSlider, reverbRoom_);
        refreshGlobalKnobFromAtomic(decaySlider, reverbDecay_);
        refreshGlobalKnobFromAtomic(masterVolumeSlider, masterVolume_);

        updateStatusSummary();
    }

    void prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) override {
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

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override {
        auto* left = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
        auto* right = bufferToFill.buffer->getNumChannels() > 1
                          ? bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample)
                          : nullptr;

        const bool playing = isPlaying_.load(std::memory_order_relaxed);
        const float evolutionAmount = evolutionAmount_.load(std::memory_order_relaxed);
        const float evolutionSpeed = evolutionSpeed_.load(std::memory_order_relaxed);
        const float masterVolume = masterVolume_.load(std::memory_order_relaxed);

        for (int sample = 0; sample < bufferToFill.numSamples; ++sample) {
            float mixedLeft = 0.0f;
            float mixedRight = 0.0f;

            for (size_t i = 0; i < voices_.size(); ++i) {
                auto& voice = voices_[i];
                auto& cloud = grainClouds_[i];

                // Evolution only runs while actually playing — Stop means
                // genuinely frozen (matching Stop no longer resetting
                // values either): nothing moves the knobs on its own while
                // stopped, not even Evolution. update() already treats
                // amount<=0 as fully inert, so gating amount to 0 here
                // reuses that existing contract rather than adding a new
                // one.
                evolutionEngines_[i].update(voice, playing ? evolutionAmount : 0.0f, evolutionSpeed);

                // Already-active grains ring out on their own envelope even
                // after Stop; only new spawning is gated by the transport,
                // so stopping fades gracefully instead of clicking.
                const float complexity = (playing && voice.isEnabled()) ? voice.getComplexity() : 0.0f;

                const auto voiceSample =
                    cloud.renderSample(voice.getPitchRangeLow(), voice.getPitchRangeHigh(),
                                        voice.getTimbre(), voice.getMotion(), complexity,
                                        voice.getVolume(), voice.getDissonance(),
                                        voice.getRootSemitoneOffset());
                mixedLeft += voiceSample.left;
                mixedRight += voiceSample.right;
            }

            // masterVolume folds in here (pre-reverb) rather than as a
            // separate post-reverb pass — Reverb is linear for fixed
            // parameters, so scaling the input scales both the dry and
            // wet signal together, equivalent to a true output fader.
            constexpr float headroom = 0.5f;
            left[sample] = mixedLeft * headroom * masterVolume;
            if (right != nullptr) {
                right[sample] = mixedRight * headroom * masterVolume;
            }
        }

        // Reverb sits outside the Evolution mechanic entirely — it's a
        // fixed global send, never drifted. Room=0/Decay=0 (the defaults)
        // give wetLevel=0 and unity dry gain, so this is a no-op and the
        // output is identical to before Reverb existed.
        if (right != nullptr) {
            const float room = reverbRoom_.load(std::memory_order_relaxed);
            const float decay = reverbDecay_.load(std::memory_order_relaxed);

            juce::Reverb::Parameters reverbParams;
            reverbParams.wetLevel = room * 0.5f;
            reverbParams.dryLevel = 0.5f;  // dryGain = dryLevel * 2.0 = unity
            reverbParams.roomSize = juce::jlimit(0.0f, 1.0f, 0.25f + decay * 0.65f + room * 0.1f);
            reverbParams.damping = juce::jlimit(0.0f, 1.0f, 1.0f - decay * 0.75f);
            reverbParams.width = 1.0f;
            reverbParams.freezeMode = 0.0f;
            reverb_.setParameters(reverbParams);
            reverb_.processStereo(left, right, bufferToFill.numSamples);
        }

        // Tap the exact signal being sent to the output device — after
        // every other stage, so a recording matches what's actually
        // audible. Duplicates left into both channels for mono devices,
        // since the recorder always writes a stereo file.
        const float* recordChannels[2] = {left, right != nullptr ? right : left};
        recorder_.recordBlock(recordChannels, bufferToFill.numSamples);
    }

private:
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
    // per archetype to suit that duration range: expected concurrent grains
    // is roughly complexity * 40/sec * average-duration-seconds, so a
    // Drone's long grains need a much lower Complexity number than a
    // Pulse's short ones to reach a comparable density.
    //
    // Character::Plucked (Pulse/Spark) gets a softened fast-attack envelope
    // and a gentle bright-to-dark filter sweep per grain, with grains long
    // and dense enough to overlap into a continuous evolving texture rather
    // than discrete pings. Character::Ambient (Drone/Haze) is the original
    // unfiltered symmetric envelope. Drone's row is unchanged from before
    // Character existed — Ambient just names what it already did.
    //
    // Dissonance near 0 for everyone by default: voices quantize mostly to
    // their (rooted) consonant scale out of the box (see HarmonicScale),
    // so nothing is constantly dissonant/spooky unless deliberately
    // opened up. Roots default to an A major triad across the four
    // voices — Drone on the root, Pulse doubling it, Spark on the fifth,
    // Haze on the third — so the harmony this Key control enables is
    // audible immediately at Dissonance=0, not just a theoretical option.
    static constexpr std::array<InitialVoice, 4> kInitialVoices{
        {{"Pulse", true, 0.65f, 0.40f, 0.65f, 0.40f, 0.45f, 0.35f, 0.15f, 0, 200.0f, 500.0f,
          Grain::Character::Plucked},
         {"Drone", true, 0.60f, 0.05f, 0.20f, 0.15f, 0.10f, 0.12f, 0.15f, 0, 1500.0f, 4000.0f,
          Grain::Character::Ambient},
         {"Spark", true, 0.55f, 0.60f, 0.85f, 0.70f, 0.50f, 0.45f, 0.15f, 7, 150.0f, 400.0f,
          Grain::Character::Plucked},
         {"Haze", true, 0.50f, 0.15f, 0.35f, 0.75f, 0.20f, 0.15f, 0.15f, 4, 2000.0f, 5000.0f,
          Grain::Character::Ambient}}};

    // A self-contained voice "card": name + LED enable indicator, a
    // full-width Pitch Range band, a row of five knobs, and — below a
    // divider — a themed (teal) row of six switches opting each of those
    // six controls in/out of autonomous Evolution drift. Visuals come
    // entirely from JerricanLookAndFeel — this class only owns layout and
    // VoiceModel/EvolutionEngine wiring.
    class VoiceRow : public juce::Component, private juce::Button::Listener, private juce::Slider::Listener {
    public:
        VoiceRow(VoiceModel& voice, EvolutionEngine& evolutionEngine, JerricanEditor* owner)
            : voiceRef_(voice), evolutionEngineRef_(evolutionEngine), owner_(owner) {
            addAndMakeVisible(nameLabel_);
            nameLabel_.setFont(juce::Font(juce::FontOptions(18.0f)).withStyle(juce::Font::bold));
            nameLabel_.setColour(juce::Label::textColourId, JerricanTheme::textPrimary);
            nameLabel_.setText(voiceRef_.getName(), juce::dontSendNotification);

            addAndMakeVisible(enabledButton_);
            enabledButton_.setButtonText("");
            enabledButton_.setToggleState(voiceRef_.isEnabled(), juce::dontSendNotification);
            enabledButton_.addListener(this);

            setUpRangeSlider(pitchRangeSlider_, pitchRangeLabel_, "Pitch range");
            pitchRangeSlider_.setMinAndMaxValues(voiceRef_.getPitchRangeLow(),
                                                  voiceRef_.getPitchRangeHigh(),
                                                  juce::dontSendNotification);

            // Key: which degree of the (Dissonance-quantized) scale this
            // voice's pitches gravitate toward — see HarmonicScale. Unlike
            // every other control here, it's never touched by Evolution or
            // Randomize; it's a compositional choice, set once and left.
            addAndMakeVisible(keyLabel_);
            keyLabel_.setText("Key", juce::dontSendNotification);
            keyLabel_.setFont(juce::Font(juce::FontOptions(12.0f)));
            keyLabel_.setColour(juce::Label::textColourId, JerricanTheme::textSecondary);
            keyLabel_.setJustificationType(juce::Justification::centredRight);

            addAndMakeVisible(rootCombo_);
            for (int i = 0; i < 12; ++i) {
                rootCombo_.addItem(kNoteNames[i], i + 1);
            }
            rootCombo_.setSelectedId(voiceRef_.getRootSemitoneOffset() + 1, juce::dontSendNotification);
            rootCombo_.onChange = [this] {
                voiceRef_.setRootSemitoneOffset(rootCombo_.getSelectedId() - 1);
            };

            setUpKnob(volumeSlider_, volumeLabel_, "Volume");
            volumeSlider_.setValue(voiceRef_.getVolume());

            setUpKnob(timbreSlider_, timbreLabel_, "Timbre");
            timbreSlider_.setValue(voiceRef_.getTimbre());

            setUpKnob(motionSlider_, motionLabel_, "Motion");
            motionSlider_.setValue(voiceRef_.getMotion());

            setUpKnob(complexitySlider_, complexityLabel_, "Complexity");
            complexitySlider_.setValue(voiceRef_.getComplexity());

            setUpKnob(dissonanceSlider_, dissonanceLabel_, "Dissonance");
            dissonanceSlider_.setValue(voiceRef_.getDissonance());

            addAndMakeVisible(evolutionSectionLabel_);
            evolutionSectionLabel_.setText("Evolution", juce::dontSendNotification);
            evolutionSectionLabel_.setFont(juce::Font(juce::FontOptions(11.0f)).withStyle(juce::Font::bold));
            evolutionSectionLabel_.setColour(juce::Label::textColourId, JerricanTheme::evolutionAccent);
            evolutionSectionLabel_.setJustificationType(juce::Justification::centredLeft);

            for (size_t i = 0; i < kEvolutionToggleCount; ++i) {
                setUpEvolutionCaption(*evolutionCaptionLabels()[i], kEvolutionCaptions[i]);
                setUpEvolutionToggle(*evolutionToggles()[i]);
            }
        }

        void paint(juce::Graphics& g) override {
            const auto bounds = getLocalBounds().toFloat();
            g.setColour(JerricanTheme::panel);
            g.fillRoundedRectangle(bounds, 10.0f);
            g.setColour(focused_ ? JerricanTheme::accent : JerricanTheme::panelBorder);
            g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, focused_ ? 2.0f : 1.0f);

            g.setColour(JerricanTheme::panelBorder);
            g.drawHorizontalLine(dividerY_, 14.0f, static_cast<float>(getWidth() - 14));
        }

        // Highlights this card when it's the "focused" voice — the one
        // MIDI-bound per-voice knobs currently drive (see
        // JerricanEditor::focusedVoiceIndex_). No-op if unchanged, so the
        // 30Hz timer polling this is cheap.
        void setFocused(bool focused) {
            if (focused_ != focused) {
                focused_ = focused;
                repaint();
            }
        }

        void resized() override {
            constexpr int padding = 14;
            const int contentWidth = getWidth() - padding * 2;

            nameLabel_.setBounds(padding, padding, contentWidth - 30, 26);
            enabledButton_.setBounds(getWidth() - padding - 22, padding + 2, 22, 22);

            const int pitchY = padding + 26 + 8;
            pitchRangeLabel_.setBounds(padding, pitchY, 160, 14);

            constexpr int rootComboWidth = 66;
            constexpr int keyLabelWidth = 28;
            keyLabel_.setBounds(padding + contentWidth - keyLabelWidth - rootComboWidth - 4, pitchY,
                                keyLabelWidth, 16);
            rootCombo_.setBounds(padding + contentWidth - rootComboWidth, pitchY, rootComboWidth, 16);

            pitchRangeSlider_.setBounds(padding, pitchY + 16, contentWidth, 22);

            const int knobRowY = pitchY + 16 + 22 + 12;
            constexpr int knobCount = 5;
            const int knobColumnWidth = contentWidth / knobCount;
            const int knobSize = std::min(84, knobColumnWidth - 12);
            const int knobLabelHeight = 14;
            const int knobTextBoxHeight = 16;

            juce::Slider* knobs[knobCount] = {&volumeSlider_, &timbreSlider_, &motionSlider_,
                                               &complexitySlider_, &dissonanceSlider_};
            juce::Label* knobLabels[knobCount] = {&volumeLabel_, &timbreLabel_, &motionLabel_,
                                                   &complexityLabel_, &dissonanceLabel_};

            for (int i = 0; i < knobCount; ++i) {
                const int columnX = padding + i * knobColumnWidth;
                const int knobX = columnX + (knobColumnWidth - knobSize) / 2;
                knobLabels[i]->setBounds(columnX, knobRowY, knobColumnWidth, knobLabelHeight);
                knobs[i]->setBounds(knobX, knobRowY + knobLabelHeight + 2, knobSize,
                                     knobSize + knobTextBoxHeight);
            }

            const int knobRowBottom = knobRowY + knobLabelHeight + 2 + knobSize + knobTextBoxHeight;
            dividerY_ = knobRowBottom + 10;

            const int evolutionLabelY = knobRowBottom + 16;
            evolutionSectionLabel_.setBounds(padding, evolutionLabelY, 100, 14);

            const int toggleRowY = evolutionLabelY + 18;
            const int toggleColumnWidth = contentWidth / kEvolutionToggleCount;
            constexpr int toggleCaptionHeight = 12;
            constexpr int toggleSize = 16;

            for (size_t i = 0; i < kEvolutionToggleCount; ++i) {
                const int columnX = padding + static_cast<int>(i) * toggleColumnWidth;
                evolutionCaptionLabels()[i]->setBounds(columnX, toggleRowY, toggleColumnWidth,
                                                       toggleCaptionHeight);
                const int toggleX = columnX + (toggleColumnWidth - toggleSize) / 2;
                evolutionToggles()[i]->setBounds(toggleX, toggleRowY + toggleCaptionHeight + 2,
                                                 toggleSize, toggleSize);
            }
        }

        void buttonClicked(juce::Button* button) override {
            if (button == &enabledButton_) {
                voiceRef_.setEnabled(enabledButton_.getToggleState());
                if (owner_ != nullptr) {
                    owner_->updateStatus();
                }
                return;
            }

            if (button == &volumeEvoToggle_) {
                const bool on = volumeEvoToggle_.getToggleState();
                evolutionEngineRef_.setVolumeEnabled(on);
                if (on) evolutionEngineRef_.resyncVolume(voiceRef_.getVolume());
            } else if (button == &pitchRangeEvoToggle_) {
                const bool on = pitchRangeEvoToggle_.getToggleState();
                evolutionEngineRef_.setPitchRangeEnabled(on);
                if (on) {
                    evolutionEngineRef_.resyncPitchRange(voiceRef_.getPitchRangeLow(),
                                                          voiceRef_.getPitchRangeHigh());
                }
            } else if (button == &timbreEvoToggle_) {
                const bool on = timbreEvoToggle_.getToggleState();
                evolutionEngineRef_.setTimbreEnabled(on);
                if (on) evolutionEngineRef_.resyncTimbre(voiceRef_.getTimbre());
            } else if (button == &motionEvoToggle_) {
                const bool on = motionEvoToggle_.getToggleState();
                evolutionEngineRef_.setMotionEnabled(on);
                if (on) evolutionEngineRef_.resyncMotion(voiceRef_.getMotion());
            } else if (button == &complexityEvoToggle_) {
                const bool on = complexityEvoToggle_.getToggleState();
                evolutionEngineRef_.setComplexityEnabled(on);
                if (on) evolutionEngineRef_.resyncComplexity(voiceRef_.getComplexity());
            } else if (button == &dissonanceEvoToggle_) {
                const bool on = dissonanceEvoToggle_.getToggleState();
                evolutionEngineRef_.setDissonanceEnabled(on);
                if (on) evolutionEngineRef_.resyncDissonance(voiceRef_.getDissonance());
            }
        }

        void sliderValueChanged(juce::Slider* slider) override {
            // Every manual edit resyncs EvolutionEngine's internal state to
            // match, regardless of that parameter's toggle state. Without
            // this, if a parameter's Evolution toggle is on (the default),
            // the engine keeps writing its own stale internal value back
            // over your drag within ~1.5ms (every 64 samples) — the value
            // visibly snaps back to wherever Evolution had drifted to,
            // fighting you the whole time you're not actively dragging.
            // Resyncing means Evolution picks up and drifts onward *from*
            // the value you just set, instead of discarding it.
            if (slider == &volumeSlider_) {
                const float value = static_cast<float>(volumeSlider_.getValue());
                voiceRef_.setVolume(value);
                evolutionEngineRef_.resyncVolume(value);
            } else if (slider == &pitchRangeSlider_) {
                const float low = static_cast<float>(pitchRangeSlider_.getMinValue());
                const float high = static_cast<float>(pitchRangeSlider_.getMaxValue());
                voiceRef_.setPitchRange(low, high);
                evolutionEngineRef_.resyncPitchRange(low, high);
            } else if (slider == &timbreSlider_) {
                const float value = static_cast<float>(timbreSlider_.getValue());
                voiceRef_.setTimbre(value);
                evolutionEngineRef_.resyncTimbre(value);
            } else if (slider == &motionSlider_) {
                const float value = static_cast<float>(motionSlider_.getValue());
                voiceRef_.setMotion(value);
                evolutionEngineRef_.resyncMotion(value);
            } else if (slider == &complexitySlider_) {
                const float value = static_cast<float>(complexitySlider_.getValue());
                voiceRef_.setComplexity(value);
                evolutionEngineRef_.resyncComplexity(value);
            } else if (slider == &dissonanceSlider_) {
                const float value = static_cast<float>(dissonanceSlider_.getValue());
                voiceRef_.setDissonance(value);
                evolutionEngineRef_.resyncDissonance(value);
            }

            if (owner_ != nullptr) {
                owner_->updateStatus();
            }
        }

        // Reflects the current model state into the controls, without
        // triggering listener callbacks (used after Stop/Reset and by the
        // Evolution auto-refresh timer). Skips any control the user is
        // currently dragging, so autonomous evolution doesn't fight a live
        // gesture. Does not touch the Evolution toggle states — those are
        // independent user preference, only reset via resetEvolutionToggles().
        void refreshFromModel() {
            if (!enabledButton_.isMouseButtonDown()) {
                enabledButton_.setToggleState(voiceRef_.isEnabled(), juce::dontSendNotification);
            }
            if (!volumeSlider_.isMouseButtonDown()) {
                volumeSlider_.setValue(voiceRef_.getVolume(), juce::dontSendNotification);
            }
            if (!pitchRangeSlider_.isMouseButtonDown()) {
                pitchRangeSlider_.setMinAndMaxValues(voiceRef_.getPitchRangeLow(),
                                                      voiceRef_.getPitchRangeHigh(),
                                                      juce::dontSendNotification);
            }
            if (!timbreSlider_.isMouseButtonDown()) {
                timbreSlider_.setValue(voiceRef_.getTimbre(), juce::dontSendNotification);
            }
            if (!motionSlider_.isMouseButtonDown()) {
                motionSlider_.setValue(voiceRef_.getMotion(), juce::dontSendNotification);
            }
            if (!complexitySlider_.isMouseButtonDown()) {
                complexitySlider_.setValue(voiceRef_.getComplexity(), juce::dontSendNotification);
            }
            if (!dissonanceSlider_.isMouseButtonDown()) {
                dissonanceSlider_.setValue(voiceRef_.getDissonance(), juce::dontSendNotification);
            }
            if (!rootCombo_.isPopupActive()) {
                rootCombo_.setSelectedId(voiceRef_.getRootSemitoneOffset() + 1,
                                         juce::dontSendNotification);
            }
        }

        // Restores all 6 Evolution toggles to their default (on) state —
        // part of a full Stop/Reset, not the ordinary model-sync above.
        void resetEvolutionToggles() {
            for (auto* toggle : evolutionToggles()) {
                toggle->setToggleState(true, juce::dontSendNotification);
            }
            evolutionEngineRef_.setVolumeEnabled(true);
            evolutionEngineRef_.setPitchRangeEnabled(true);
            evolutionEngineRef_.setTimbreEnabled(true);
            evolutionEngineRef_.setMotionEnabled(true);
            evolutionEngineRef_.setComplexityEnabled(true);
            evolutionEngineRef_.setDissonanceEnabled(true);
        }

        // Reflects EvolutionEngine's current opt-in/out flags into the six
        // toggle LEDs, without touching the values they control — the
        // counterpart to refreshFromModel() above, needed now that MIDI
        // Learn can flip these flags from off-screen (a manual click
        // already updates its own toggle directly, so this is purely for
        // externally-driven changes). Order matches evolutionToggles().
        void refreshEvolutionToggles() {
            volumeEvoToggle_.setToggleState(evolutionEngineRef_.isVolumeEnabled(),
                                            juce::dontSendNotification);
            pitchRangeEvoToggle_.setToggleState(evolutionEngineRef_.isPitchRangeEnabled(),
                                                juce::dontSendNotification);
            timbreEvoToggle_.setToggleState(evolutionEngineRef_.isTimbreEnabled(),
                                            juce::dontSendNotification);
            motionEvoToggle_.setToggleState(evolutionEngineRef_.isMotionEnabled(),
                                            juce::dontSendNotification);
            complexityEvoToggle_.setToggleState(evolutionEngineRef_.isComplexityEnabled(),
                                                juce::dontSendNotification);
            dissonanceEvoToggle_.setToggleState(evolutionEngineRef_.isDissonanceEnabled(),
                                                juce::dontSendNotification);
        }

    private:
        static constexpr int kEvolutionToggleCount = 6;
        static constexpr const char* kEvolutionCaptions[kEvolutionToggleCount] = {
            "Volume", "Range", "Timbre", "Motion", "Complexity", "Dissonance"};
        static constexpr const char* kNoteNames[12] = {"A",  "A#", "B", "C",  "C#", "D",
                                                        "D#", "E",  "F", "F#", "G",  "G#"};

        void setUpLabel(juce::Label& label, const char* labelText) {
            addAndMakeVisible(label);
            label.setText(labelText, juce::dontSendNotification);
            label.setFont(juce::Font(juce::FontOptions(12.0f)));
            label.setColour(juce::Label::textColourId, JerricanTheme::textSecondary);
            label.setJustificationType(juce::Justification::centred);
        }

        void setUpKnob(juce::Slider& slider, juce::Label& label, const char* labelText) {
            setUpLabel(label, labelText);

            addAndMakeVisible(slider);
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setRange(0.0, 1.0);
            slider.setNumDecimalPlacesToDisplay(2);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
            slider.addListener(this);
        }

        void setUpRangeSlider(juce::Slider& slider, juce::Label& label, const char* labelText) {
            setUpLabel(label, labelText);
            label.setJustificationType(juce::Justification::centredLeft);

            addAndMakeVisible(slider);
            slider.setSliderStyle(juce::Slider::TwoValueHorizontal);
            slider.setRange(0.0, 1.0);
            slider.setNumDecimalPlacesToDisplay(2);
            slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            slider.addListener(this);
        }

        void setUpEvolutionCaption(juce::Label& label, const char* labelText) {
            addAndMakeVisible(label);
            label.setText(labelText, juce::dontSendNotification);
            label.setFont(juce::Font(juce::FontOptions(9.0f)));
            label.setColour(juce::Label::textColourId, JerricanTheme::textSecondary);
            label.setJustificationType(juce::Justification::centred);
        }

        void setUpEvolutionToggle(juce::ToggleButton& toggle) {
            addAndMakeVisible(toggle);
            toggle.setName("evolutionToggle");
            toggle.setButtonText("");
            toggle.setToggleState(true, juce::dontSendNotification);
            toggle.addListener(this);
        }

        std::array<juce::Label*, kEvolutionToggleCount> evolutionCaptionLabels() {
            return {&volumeEvoLabel_, &pitchRangeEvoLabel_, &timbreEvoLabel_, &motionEvoLabel_,
                    &complexityEvoLabel_, &dissonanceEvoLabel_};
        }

        std::array<juce::ToggleButton*, kEvolutionToggleCount> evolutionToggles() {
            return {&volumeEvoToggle_,     &pitchRangeEvoToggle_, &timbreEvoToggle_,
                    &motionEvoToggle_,     &complexityEvoToggle_, &dissonanceEvoToggle_};
        }

        VoiceModel& voiceRef_;
        EvolutionEngine& evolutionEngineRef_;
        JerricanEditor* owner_;
        int dividerY_ = 0;
        bool focused_ = false;
        juce::Label nameLabel_;
        juce::Label volumeLabel_;
        juce::Label pitchRangeLabel_;
        juce::Label keyLabel_;
        juce::ComboBox rootCombo_;
        juce::Label timbreLabel_;
        juce::Label motionLabel_;
        juce::Label complexityLabel_;
        juce::Label dissonanceLabel_;
        juce::ToggleButton enabledButton_;
        juce::Slider volumeSlider_;
        juce::Slider pitchRangeSlider_;
        juce::Slider timbreSlider_;
        juce::Slider motionSlider_;
        juce::Slider complexitySlider_;
        juce::Slider dissonanceSlider_;
        juce::Label evolutionSectionLabel_;
        juce::Label volumeEvoLabel_;
        juce::Label pitchRangeEvoLabel_;
        juce::Label timbreEvoLabel_;
        juce::Label motionEvoLabel_;
        juce::Label complexityEvoLabel_;
        juce::Label dissonanceEvoLabel_;
        juce::ToggleButton volumeEvoToggle_;
        juce::ToggleButton pitchRangeEvoToggle_;
        juce::ToggleButton timbreEvoToggle_;
        juce::ToggleButton motionEvoToggle_;
        juce::ToggleButton complexityEvoToggle_;
        juce::ToggleButton dissonanceEvoToggle_;
    };

    // Shared preset combo/Save-As/Delete/Override control, used by both
    // ScenesPopup and MidiBindingsPopup so the two behave identically —
    // driven entirely by callbacks rather than templated on the
    // underlying data type, since MidiBindingManager/MidiPresetStore and
    // SceneState/ScenePresetStore have nothing in common beyond "named,
    // save/load/list/remove, comparable for equality".
    //
    // State machine (re-run continuously on a timer, and right after any
    // load/save/delete, so it stays correct no matter how live state
    // changes underneath it — manual edits, Randomize, MIDI input):
    //  - Combo has a name selected: if live state still matches what's
    //    saved under that name, show Save As + Delete. If it no longer
    //    matches (dirty), also show "Override "{name}"" to write the
    //    live state back under the same name without a prompt.
    //  - Combo is blank: scan every saved name for one that matches live
    //    state and auto-select it if found (this is the "select it if it
    //    matches" behavior, re-checked every tick rather than once).
    //    Still blank after that: Save As is only enabled if there's
    //    something meaningful to save; Delete stays disabled (nothing to
    //    delete).
    class PresetControls : public juce::Component, private juce::Timer {
    public:
        struct Callbacks {
            std::function<std::vector<std::string>()> listNames;
            std::function<bool(const std::string&)> loadNamed;
            std::function<bool(const std::string&)> saveNamed;
            std::function<bool(const std::string&)> removeNamed;
            std::function<bool(const std::string&)> matchesNamed;
            std::function<bool()> hasMeaningfulContent;
            // Called after a confirmed delete, so the live state doesn't
            // keep pointing at a preset that no longer exists on disk —
            // e.g. clearing bindings, or resetting voices to defaults.
            std::function<void()> onDeleted;
            // PresetControls itself is rebuilt from scratch every time its
            // popup reopens (a fresh CallOutBox each click), so "which
            // preset am I on" can't live only in this Component's combo
            // box — it has to be persisted by the owner (JerricanEditor)
            // across opens/closes, otherwise editing a loaded preset and
            // reopening the popup loses track of it, leaving only the
            // "Save As" path (no way to Override) even though you're
            // still clearly working from that preset.
            std::function<juce::String()> getCurrentName;
            std::function<void(const juce::String&)> setCurrentName;
        };

        static constexpr int kPreferredHeight = 22 + 6 + 22;

        // enableRevert: whether to offer a "Revert" button (discards
        // unsaved edits, reloads the selected preset's saved values) —
        // Scenes-only per explicit request; Bindings' UI stays as-is.
        PresetControls(const juce::String& labelText, Callbacks callbacks,
                       bool enableRevert = false)
            : callbacks_(std::move(callbacks)), enableRevert_(enableRevert) {
            addAndMakeVisible(label_);
            label_.setText(labelText, juce::dontSendNotification);
            label_.setFont(juce::Font(juce::FontOptions(12.0f)));
            label_.setColour(juce::Label::textColourId, JerricanTheme::textSecondary);

            addAndMakeVisible(combo_);
            combo_.onChange = [this] {
                const auto name = combo_.getText();
                if (name.isNotEmpty()) {
                    callbacks_.loadNamed(name.toStdString());
                }
                callbacks_.setCurrentName(name);
                refreshState();
            };

            addAndMakeVisible(overrideButton_);
            overrideButton_.setVisible(false);
            overrideButton_.onClick = [this] {
                const auto name = combo_.getText();
                if (name.isNotEmpty()) {
                    callbacks_.saveNamed(name.toStdString());
                    refreshState();
                }
            };

            addAndMakeVisible(revertButton_);
            revertButton_.setButtonText("Revert");
            revertButton_.setVisible(false);
            revertButton_.onClick = [this] {
                const auto name = combo_.getText();
                if (name.isNotEmpty()) {
                    callbacks_.loadNamed(name.toStdString());
                    refreshState();
                }
            };

            addAndMakeVisible(saveAsButton_);
            saveAsButton_.setButtonText("Save As...");
            saveAsButton_.onClick = [this] { showSaveAsPrompt(); };

            addAndMakeVisible(deleteButton_);
            deleteButton_.setButtonText("Delete");
            deleteButton_.onClick = [this] { showDeleteConfirmPrompt(); };

            // Re-hydrate from whatever preset the owner remembers being
            // "on", so reopening this popup after editing a loaded preset
            // still offers Override rather than only Save As.
            combo_.setText(callbacks_.getCurrentName(), juce::dontSendNotification);
            refreshNames();
            refreshState();
            startTimerHz(10);
        }

        void resized() override {
            const int width = getWidth();
            label_.setBounds(0, 0, 50, 22);
            combo_.setBounds(54, 0, width - 54, 22);

            constexpr int buttonHeight = 22;
            constexpr int buttonGap = 6;
            constexpr int saveAsWidth = 90;
            constexpr int deleteWidth = 70;
            const int row2Y = 22 + 6;

            int rightX = width;
            deleteButton_.setBounds(rightX - deleteWidth, row2Y, deleteWidth, buttonHeight);
            rightX -= deleteWidth + buttonGap;
            saveAsButton_.setBounds(rightX - saveAsWidth, row2Y, saveAsWidth, buttonHeight);
            rightX -= saveAsWidth + buttonGap;

            if (revertButton_.isVisible()) {
                constexpr int revertWidth = 64;
                revertButton_.setBounds(rightX - revertWidth, row2Y, revertWidth, buttonHeight);
                rightX -= revertWidth + buttonGap;
            }

            if (overrideButton_.isVisible()) {
                overrideButton_.setBounds(0, row2Y, std::max(0, rightX - buttonGap), buttonHeight);
            }
        }

    private:
        void refreshNames() {
            const auto currentText = combo_.getText();
            combo_.clear(juce::dontSendNotification);
            const auto names = callbacks_.listNames();
            for (int i = 0; i < static_cast<int>(names.size()); ++i) {
                combo_.addItem(names[static_cast<std::size_t>(i)], i + 1);
            }
            combo_.setText(currentText, juce::dontSendNotification);
        }

        void refreshState() {
            auto currentName = combo_.getText();

            if (currentName.isEmpty()) {
                for (const auto& name : callbacks_.listNames()) {
                    if (callbacks_.matchesNamed(name)) {
                        combo_.setText(name, juce::dontSendNotification);
                        currentName = name;
                        callbacks_.setCurrentName(name);
                        break;
                    }
                }
            }

            const bool wasOverrideVisible = overrideButton_.isVisible();
            const bool wasRevertVisible = revertButton_.isVisible();
            if (currentName.isEmpty()) {
                overrideButton_.setVisible(false);
                revertButton_.setVisible(false);
                saveAsButton_.setEnabled(callbacks_.hasMeaningfulContent());
                deleteButton_.setEnabled(false);
            } else {
                const bool matches = callbacks_.matchesNamed(currentName.toStdString());
                overrideButton_.setVisible(!matches);
                if (!matches) {
                    overrideButton_.setButtonText("Override \"" + currentName + "\"");
                }
                // Only worth offering once there's actually something to
                // discard — if live state already matches the saved
                // preset, Revert would be a no-op.
                revertButton_.setVisible(enableRevert_ && !matches);
                saveAsButton_.setEnabled(true);
                deleteButton_.setEnabled(true);
            }

            if (overrideButton_.isVisible() != wasOverrideVisible ||
                revertButton_.isVisible() != wasRevertVisible) {
                resized();
            }
        }

        void showSaveAsPrompt() {
            auto* window = new juce::AlertWindow("Save Preset", "Name this preset:",
                                                 juce::MessageBoxIconType::NoIcon);
            window->addTextEditor("name", combo_.getText(), "");
            window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
            window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

            // Same use-after-free guard as everywhere else this popup
            // pattern is used — the modal can outlive this component if
            // its hosting CallOutBox gets dismissed first.
            juce::Component::SafePointer<PresetControls> safeThis(this);
            window->enterModalState(
                true,
                juce::ModalCallbackFunction::create([safeThis, window](int result) {
                    if (result != 1 || safeThis == nullptr) {
                        return;
                    }
                    const auto name = window->getTextEditorContents("name");
                    if (name.isEmpty()) {
                        return;
                    }

                    // Save As is framed as "create a new preset" — silently
                    // overwriting an existing one because you happened to
                    // type (or mistype into) a name that already exists
                    // would be a destructive surprise, so confirm first.
                    const auto existingNames = safeThis->callbacks_.listNames();
                    const bool alreadyExists =
                        std::find(existingNames.begin(), existingNames.end(),
                                 name.toStdString()) != existingNames.end();
                    if (alreadyExists) {
                        safeThis->showOverwriteConfirmPrompt(name);
                    } else {
                        safeThis->commitSaveAs(name);
                    }
                }),
                true);
        }

        void showOverwriteConfirmPrompt(const juce::String& name) {
            auto* window =
                new juce::AlertWindow("Overwrite Preset",
                                      "A preset named \"" + name +
                                          "\" already exists. Overwrite it?",
                                      juce::MessageBoxIconType::WarningIcon);
            window->addButton("Overwrite", 1, juce::KeyPress(juce::KeyPress::returnKey));
            window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

            juce::Component::SafePointer<PresetControls> safeThis(this);
            window->enterModalState(
                true,
                juce::ModalCallbackFunction::create([safeThis, name](int result) {
                    if (result == 1 && safeThis != nullptr) {
                        safeThis->commitSaveAs(name);
                    }
                }),
                true);
        }

        void commitSaveAs(const juce::String& name) {
            callbacks_.saveNamed(name.toStdString());
            callbacks_.setCurrentName(name);
            refreshNames();
            combo_.setText(name, juce::dontSendNotification);
            refreshState();
        }

        void showDeleteConfirmPrompt() {
            const auto name = combo_.getText();
            if (name.isEmpty()) {
                return;
            }

            auto* window =
                new juce::AlertWindow("Delete Preset",
                                      "Are you sure you want to delete \"" + name + "\"?",
                                      juce::MessageBoxIconType::WarningIcon);
            window->addButton("Delete", 1, juce::KeyPress(juce::KeyPress::returnKey));
            window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

            juce::Component::SafePointer<PresetControls> safeThis(this);
            window->enterModalState(
                true,
                juce::ModalCallbackFunction::create([safeThis, name](int result) {
                    if (result == 1 && safeThis != nullptr) {
                        safeThis->callbacks_.removeNamed(name.toStdString());
                        safeThis->combo_.setText("", juce::dontSendNotification);
                        safeThis->callbacks_.setCurrentName("");
                        safeThis->refreshNames();
                        if (safeThis->callbacks_.onDeleted) {
                            safeThis->callbacks_.onDeleted();
                        }
                        safeThis->refreshState();
                    }
                }),
                true);
        }

        void timerCallback() override { refreshState(); }

        Callbacks callbacks_;
        bool enableRevert_;
        juce::Label label_;
        juce::ComboBox combo_;
        juce::TextButton overrideButton_;
        juce::TextButton revertButton_;
        juce::TextButton saveAsButton_;
        juce::TextButton deleteButton_;
    };

    // Scenes popup, opened from the "Scenes" button next to Bindings — a
    // full state snapshot (every voice's knobs/enabled/Evolution-toggle
    // state, plus the global Evolution/Reverb/Volume controls), distinct
    // from MIDI Learn's controller-mapping presets. Just a preset row —
    // no per-control rows, since a Scene captures everything at once.
    class ScenesPopup : public juce::Component {
    public:
        explicit ScenesPopup(JerricanEditor* owner)
            : presetControls_(
                  "Scene",
                  PresetControls::Callbacks{
                      .listNames = [owner] { return owner->scenePresetStore_.listPresetNames(); },
                      .loadNamed =
                          [owner](const std::string& name) {
                              SceneState scene;
                              if (!owner->scenePresetStore_.load(name, scene)) {
                                  return false;
                              }
                              owner->applySceneState(scene);
                              return true;
                          },
                      .saveNamed =
                          [owner](const std::string& name) {
                              return owner->scenePresetStore_.save(name, owner->captureSceneState());
                          },
                      .removeNamed = [owner](const std::string& name) {
                          return owner->scenePresetStore_.remove(name);
                      },
                      .matchesNamed =
                          [owner](const std::string& name) {
                              SceneState scene;
                              return owner->scenePresetStore_.load(name, scene) &&
                                     scene == owner->captureSceneState();
                          },
                      // A Scene is always a complete, meaningful snapshot
                      // — unlike an empty MIDI binding table, there's no
                      // "nothing to save" state, so Save As is never
                      // gated off here.
                      .hasMeaningfulContent = [] { return true; },
                      // Deleting the Scene you're on shouldn't leave live
                      // state pointing at a preset that no longer exists
                      // — fall back to the same defaults Reset restores.
                      .onDeleted = [owner] { owner->handleResetPressed(); },
                      .getCurrentName = [owner] { return owner->currentSceneName_; },
                      .setCurrentName = [owner](const juce::String& name) {
                          owner->currentSceneName_ = name;
                      },
                  },
                  /*enableRevert=*/true) {
            addAndMakeVisible(presetControls_);

            addAndMakeVisible(hintLabel_);
            hintLabel_.setText("Captures every knob/toggle. Transport state isn't included.",
                               juce::dontSendNotification);
            hintLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
            hintLabel_.setColour(juce::Label::textColourId, JerricanTheme::textSecondary);
            hintLabel_.setJustificationType(juce::Justification::topLeft);
        }

        void resized() override {
            constexpr int padding = 12;
            const int contentWidth = getWidth() - padding * 2;

            presetControls_.setBounds(padding, padding, contentWidth, PresetControls::kPreferredHeight);
            hintLabel_.setBounds(padding, padding + PresetControls::kPreferredHeight + 10, contentWidth,
                                 40);
        }

    private:
        PresetControls presetControls_;
        juce::Label hintLabel_;
    };

    // MIDI Learn popup, opened from the "Bindings" button next to the MIDI
    // input dropdown. One row per MidiTarget (label, live binding readout,
    // Learn, Clear), grouped exactly per the confirmed scope: per-voice
    // controls (apply to whichever voice is currently focused), voice
    // select (moves focus), and global controls. A preset row at the top
    // saves/loads/deletes named binding sets — explicit Save only, nothing
    // is written to disk until "Save As..." is clicked. Runs its own
    // lightweight Timer to poll owner_->midiBindings_ and keep every row's
    // readout current, including while a Learn is in flight — avoids
    // needing callback plumbing that could dangle if this popup closes
    // mid-learn (CallOutBox content is inherently ephemeral).
    class MidiBindingsPopup : public juce::Component, private juce::Timer {
    public:
        explicit MidiBindingsPopup(JerricanEditor* owner)
            : owner_(owner),
              presetControls_(
                  "Preset",
                  PresetControls::Callbacks{
                      .listNames = [owner] { return owner->midiPresetStore_.listPresetNames(); },
                      .loadNamed =
                          [owner](const std::string& name) {
                              return owner->midiPresetStore_.load(name, owner->midiBindings_);
                          },
                      .saveNamed =
                          [owner](const std::string& name) {
                              return owner->midiPresetStore_.save(name, owner->midiBindings_);
                          },
                      .removeNamed = [owner](const std::string& name) {
                          return owner->midiPresetStore_.remove(name);
                      },
                      .matchesNamed =
                          [owner](const std::string& name) {
                              MidiBindingManager temp;
                              return owner->midiPresetStore_.load(name, temp) &&
                                     temp.equals(owner->midiBindings_);
                          },
                      .hasMeaningfulContent =
                          [owner] {
                              for (const auto target : kAllMidiTargets) {
                                  if (owner->midiBindings_.getBinding(target).has_value()) {
                                      return true;
                                  }
                              }
                              return false;
                          },
                      // Deleting the preset you're on shouldn't leave the
                      // live bindings pointing at a saved file that no
                      // longer exists — clear them so it's an honest
                      // blank slate.
                      .onDeleted = [owner] { owner->midiBindings_.clearAll(); },
                      .getCurrentName = [owner] { return owner->currentMidiPresetName_; },
                      .setCurrentName = [owner](const juce::String& name) {
                          owner->currentMidiPresetName_ = name;
                      },
                  }) {
            addAndMakeVisible(presetControls_);

            addAndMakeVisible(midiWarningLabel_);
            midiWarningLabel_.setText(
                "No MIDI controller connected " +
                    juce::String(juce::CharPointer_UTF8("\xe2\x80\x94")) + " pick one from MIDI In.",
                juce::dontSendNotification);
            midiWarningLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
            midiWarningLabel_.setColour(juce::Label::textColourId, JerricanTheme::accentDeep);
            midiWarningLabel_.setVisible(owner_->currentMidiInputId_.isEmpty());

            // 26 target rows no longer reliably fit a fixed popup height
            // on smaller screens — everything below the preset row lives
            // in rowsContainer_, scrolled via viewport_, so the preset
            // row (always relevant) stays pinned at the top regardless.
            addAndMakeVisible(viewport_);
            viewport_.setViewedComponent(&rowsContainer_, false);
            viewport_.setScrollBarsShown(true, false);

            setUpSectionLabel(perVoiceSectionLabel_, "Per-Voice Controls (focused voice)");
            setUpSectionLabel(voiceSelectSectionLabel_, "Voice Select");
            setUpSectionLabel(transportSectionLabel_, "Transport");
            setUpSectionLabel(globalSectionLabel_, "Global");

            for (std::size_t i = 0; i < kTargetCount; ++i) {
                const auto target = kAllMidiTargets[i];
                rowsContainer_.addAndMakeVisible(targetLabels_[i]);
                targetLabels_[i].setText(friendlyTargetLabel(target), juce::dontSendNotification);
                targetLabels_[i].setFont(juce::Font(juce::FontOptions(12.0f)));
                targetLabels_[i].setColour(juce::Label::textColourId, JerricanTheme::textPrimary);

                rowsContainer_.addAndMakeVisible(targetReadouts_[i]);
                targetReadouts_[i].setFont(juce::Font(juce::FontOptions(11.0f)));
                targetReadouts_[i].setColour(juce::Label::textColourId, JerricanTheme::textSecondary);

                rowsContainer_.addAndMakeVisible(learnButtons_[i]);
                learnButtons_[i].setButtonText("Learn");
                learnButtons_[i].onClick = [this, target] { owner_->midiBindings_.armLearn(target); };

                rowsContainer_.addAndMakeVisible(clearButtons_[i]);
                clearButtons_[i].setButtonText("Clear");
                clearButtons_[i].onClick = [this, target] {
                    owner_->midiBindings_.clearBinding(target);
                };
            }

            refreshReadouts();
            startTimerHz(10);
        }

        void resized() override {
            constexpr int padding = 12;
            const int contentWidth = getWidth() - padding * 2;

            presetControls_.setBounds(padding, padding, contentWidth, PresetControls::kPreferredHeight);
            int viewportTop = padding + PresetControls::kPreferredHeight + 10;

            if (midiWarningLabel_.isVisible()) {
                midiWarningLabel_.setBounds(padding, viewportTop, contentWidth, 18);
                viewportTop += 18 + 8;
            }

            viewport_.setBounds(0, viewportTop, getWidth(), getHeight() - viewportTop);

            // rowsContainer_ is sized to its actual content height (which
            // can exceed the popup's own height) — that's what makes
            // viewport_ scroll instead of clipping.
            const int rowsContentWidth = getWidth() - viewport_.getScrollBarThickness();
            const int innerContentWidth = rowsContentWidth - padding * 2;
            int y = padding;
            y = layoutSection(perVoiceSectionLabel_, 0, 13, padding, innerContentWidth, y);
            y = layoutSection(voiceSelectSectionLabel_, 13, 17, padding, innerContentWidth, y);
            y = layoutSection(transportSectionLabel_, 17, 21, padding, innerContentWidth, y);
            y = layoutSection(globalSectionLabel_, 21, 26, padding, innerContentWidth, y);
            rowsContainer_.setSize(rowsContentWidth, y + padding);
        }

    private:
        static constexpr std::size_t kTargetCount = kAllMidiTargets.size();

        int layoutSection(juce::Label& sectionLabel, std::size_t begin, std::size_t end, int padding,
                          int contentWidth, int y) {
            sectionLabel.setBounds(padding, y, contentWidth, 16);
            y += 16 + 4;

            for (std::size_t i = begin; i < end; ++i) {
                targetLabels_[i].setBounds(padding, y, 150, 22);
                targetReadouts_[i].setBounds(padding + 150, y, 130, 22);
                learnButtons_[i].setBounds(padding + 150 + 130 + 4, y, 60, 22);
                clearButtons_[i].setBounds(padding + 150 + 130 + 4 + 64, y, 56, 22);
                y += 22;
            }
            return y + 12;
        }

        void setUpSectionLabel(juce::Label& label, const char* text) {
            rowsContainer_.addAndMakeVisible(label);
            label.setText(text, juce::dontSendNotification);
            label.setFont(juce::Font(juce::FontOptions(12.0f)).withStyle(juce::Font::bold));
            label.setColour(juce::Label::textColourId, JerricanTheme::evolutionAccent);
        }

        void refreshReadouts() {
            for (std::size_t i = 0; i < kTargetCount; ++i) {
                const auto target = kAllMidiTargets[i];
                if (owner_->midiBindings_.isLearning() && owner_->midiBindings_.learningTarget() == target) {
                    targetReadouts_[i].setText("Listening...", juce::dontSendNotification);
                    continue;
                }
                targetReadouts_[i].setText(describeBinding(owner_->midiBindings_.getBinding(target)),
                                           juce::dontSendNotification);
            }
        }

        void timerCallback() override {
            refreshReadouts();
            const bool shouldWarn = owner_->currentMidiInputId_.isEmpty();
            if (shouldWarn != midiWarningLabel_.isVisible()) {
                midiWarningLabel_.setVisible(shouldWarn);
                resized();
            }
        }

        static juce::String describeBinding(std::optional<MidiBinding> binding) {
            if (!binding.has_value()) {
                return "Not bound";
            }
            const juce::String kind = binding->type == MidiEvent::Type::ControlChange ? "CC" : "Note";
            return kind + " " + juce::String(binding->number) + " ch" + juce::String(binding->channel);
        }

        static const char* friendlyTargetLabel(MidiTarget target) {
            switch (target) {
                case MidiTarget::VoicePitchCenter: return "Pitch Range";
                case MidiTarget::VoiceVolume: return "Volume";
                case MidiTarget::VoiceTimbre: return "Timbre";
                case MidiTarget::VoiceMotion: return "Motion";
                case MidiTarget::VoiceComplexity: return "Complexity";
                case MidiTarget::VoiceDissonance: return "Dissonance";
                case MidiTarget::VoiceEnabledToggle: return "Enabled toggle";
                case MidiTarget::VoicePitchRangeEvoToggle: return "Evolve: Pitch Range";
                case MidiTarget::VoiceVolumeEvoToggle: return "Evolve: Volume";
                case MidiTarget::VoiceTimbreEvoToggle: return "Evolve: Timbre";
                case MidiTarget::VoiceMotionEvoToggle: return "Evolve: Motion";
                case MidiTarget::VoiceComplexityEvoToggle: return "Evolve: Complexity";
                case MidiTarget::VoiceDissonanceEvoToggle: return "Evolve: Dissonance";
                case MidiTarget::SelectVoice1: return "Voice 1";
                case MidiTarget::SelectVoice2: return "Voice 2";
                case MidiTarget::SelectVoice3: return "Voice 3";
                case MidiTarget::SelectVoice4: return "Voice 4";
                case MidiTarget::TransportPlay: return "Play";
                case MidiTarget::TransportStop: return "Stop";
                case MidiTarget::TransportReset: return "Reset";
                case MidiTarget::TransportRandomize: return "Randomize";
                case MidiTarget::EvolutionAmount: return "Evolution Amount";
                case MidiTarget::EvolutionSpeed: return "Evolution Speed";
                case MidiTarget::ReverbRoom: return "Reverb Room";
                case MidiTarget::ReverbDecay: return "Reverb Decay";
                case MidiTarget::MasterVolume: return "Master Volume";
            }
            return "";
        }

        JerricanEditor* owner_;
        PresetControls presetControls_;
        juce::Label midiWarningLabel_;
        juce::Viewport viewport_;
        juce::Component rowsContainer_;
        juce::Label perVoiceSectionLabel_;
        juce::Label voiceSelectSectionLabel_;
        juce::Label transportSectionLabel_;
        juce::Label globalSectionLabel_;
        std::array<juce::Label, kTargetCount> targetLabels_;
        std::array<juce::Label, kTargetCount> targetReadouts_;
        std::array<juce::TextButton, kTargetCount> learnButtons_;
        std::array<juce::TextButton, kTargetCount> clearButtons_;
    };

    void buttonClicked(juce::Button* button) override {
        if (button == &playButton) {
            handlePlayPressed();
        } else if (button == &stopButton) {
            handleStopPressed();
        } else if (button == &resetButton) {
            handleResetPressed();
        } else if (button == &randomizeButton) {
            handleRandomizePressed();
        }
    }

    // The four transport actions, factored out of buttonClicked so
    // applyMidiTarget's MIDI-bound Play/Stop/Reset/Randomize (dispatched
    // via callAsync onto this, the message thread — see applyMidiTarget)
    // can call the exact same logic a mouse click does, rather than
    // duplicating it.
    void handlePlayPressed() {
        isPlaying_.store(true, std::memory_order_relaxed);
        // Play stays enabled and just switches to its "active" colour —
        // greying it out read as broken/unclickable rather than "running".
        playButton.setToggleState(true, juce::dontSendNotification);
        stopButton.setEnabled(true);
        statusLabel.setText("Transport running", juce::dontSendNotification);
        updateStatusSummary();
    }

    void handleStopPressed() {
        // Just halts new grain spawning — existing grains ring out on
        // their own, and every knob stays exactly where it was, so
        // pressing Play again picks up right where you left off.
        isPlaying_.store(false, std::memory_order_relaxed);
        playButton.setToggleState(false, juce::dontSendNotification);
        stopButton.setEnabled(false);
        statusLabel.setText("Transport stopped", juce::dontSendNotification);
        updateStatusSummary();
    }

    void handleResetPressed() {
        // Explicitly snaps every voice back to its starting values —
        // separate from Stop, since that used to happen together and
        // made Stop feel like it was yanking the controls out from
        // under you.
        resetVoicesToInitialState();
        statusLabel.setText("Voices reset to defaults", juce::dontSendNotification);
        updateStatusSummary();
    }

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

            voiceRows_[i]->refreshFromModel();
        }

        updateStatusSummary();
    }

    void sliderValueChanged(juce::Slider* slider) override {
        if (slider == &evolutionAmountSlider) {
            evolutionAmount_.store(static_cast<float>(evolutionAmountSlider.getValue()),
                                   std::memory_order_relaxed);
        } else if (slider == &evolutionSpeedSlider) {
            evolutionSpeed_.store(static_cast<float>(evolutionSpeedSlider.getValue()),
                                  std::memory_order_relaxed);
        } else if (slider == &roomSlider) {
            reverbRoom_.store(static_cast<float>(roomSlider.getValue()), std::memory_order_relaxed);
        } else if (slider == &decaySlider) {
            reverbDecay_.store(static_cast<float>(decaySlider.getValue()), std::memory_order_relaxed);
        } else if (slider == &masterVolumeSlider) {
            masterVolume_.store(static_cast<float>(masterVolumeSlider.getValue()),
                                std::memory_order_relaxed);
        }
    }

    void timerCallback() override {
        // Voice rows, focus highlight, and the global knobs all need to
        // reflect state regardless of transport/Evolution — MIDI can
        // change any of it at any time now, not just Evolution while
        // playing. refreshFromModel()/the drag-guard below already skip
        // any control the user is actively touching.
        const int focused = focusedVoiceIndex_.load(std::memory_order_relaxed);
        for (size_t i = 0; i < voiceRows_.size(); ++i) {
            voiceRows_[i]->refreshFromModel();
            voiceRows_[i]->refreshEvolutionToggles();
            voiceRows_[i]->setFocused(static_cast<int>(i) == focused);
        }

        refreshGlobalKnobFromAtomic(evolutionAmountSlider, evolutionAmount_);
        refreshGlobalKnobFromAtomic(evolutionSpeedSlider, evolutionSpeed_);
        refreshGlobalKnobFromAtomic(roomSlider, reverbRoom_);
        refreshGlobalKnobFromAtomic(decaySlider, reverbDecay_);
        refreshGlobalKnobFromAtomic(masterVolumeSlider, masterVolume_);
    }

    static void refreshGlobalKnobFromAtomic(juce::Slider& slider, std::atomic<float>& value) {
        if (!slider.isMouseButtonDown()) {
            slider.setValue(value.load(std::memory_order_relaxed), juce::dontSendNotification);
        }
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
            voiceRows_[i]->refreshFromModel();
            voiceRows_[i]->resetEvolutionToggles();

            const float center = (initial.pitchLow + initial.pitchHigh) * 0.5f;
            const float width = initial.pitchHigh - initial.pitchLow;
            evolutionEngines_[i].resetTo(center, width, initial.volume, initial.timbre,
                                          initial.motion, initial.complexity, initial.dissonance);
        }
    }

    void updateStatus() { updateStatusSummary(); }

    void updateStatusSummary() {
        int activeVoices = 0;
        for (const auto& voice : voices_) {
            if (voice.isEnabled()) {
                ++activeVoices;
            }
        }

        const juce::String transportState =
            isPlaying_.load(std::memory_order_relaxed) ? "playing" : "stopped";
        statusLabel.setText("Transport " + transportState + " — " + juce::String(activeVoices) +
                                 " active voices",
                             juce::dontSendNotification);
    }

    JerricanLookAndFeel lookAndFeel_;
    juce::ImageComponent logoImage_;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::TextButton helpButton;
    juce::TextButton recordButton;
    juce::TimeSliceThread recordingThread_{"Jerrican Recording Thread"};
    AudioRecorder recorder_{recordingThread_};
    juce::File currentRecordingFile_;
    double sampleRate_ = 44100.0;
    juce::TextButton scenesButton;
    juce::TextButton bindingsButton;
    juce::Label midiInputLabel;
    juce::ComboBox midiInputDeviceBox;
    juce::String currentMidiInputId_;
    juce::Label outputLabel;
    juce::ComboBox outputDeviceBox;
    juce::Label statusLabel;
    juce::TextButton openRecordingFolderButton;
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::TextButton resetButton;
    juce::TextButton randomizeButton;
    juce::Label evolutionTitleLabel;
    juce::Label evolutionAmountLabel;
    juce::Slider evolutionAmountSlider;
    juce::Label evolutionSpeedLabel;
    juce::Slider evolutionSpeedSlider;
    juce::Label reverbTitleLabel;
    juce::Label roomLabel;
    juce::Slider roomSlider;
    juce::Label decayLabel;
    juce::Slider decaySlider;
    juce::Label masterVolumeTitleLabel;
    juce::Label masterVolumeLabel;
    juce::Slider masterVolumeSlider;
    std::atomic<bool> isPlaying_{false};
    std::atomic<float> evolutionAmount_{0.0f};
    std::atomic<float> evolutionSpeed_{0.5f};
    std::atomic<float> reverbRoom_{0.0f};
    std::atomic<float> reverbDecay_{0.0f};
    std::atomic<float> masterVolume_{1.0f};
    std::atomic<int> focusedVoiceIndex_{0};
    // Which preset each popup is "on", persisted here rather than in the
    // popup itself — MidiBindingsPopup/ScenesPopup are rebuilt from
    // scratch every time their CallOutBox reopens, so without this,
    // reopening after editing a loaded preset would lose track of which
    // one to Override.
    juce::String currentMidiPresetName_;
    juce::String currentSceneName_;
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
    juce::Reverb reverb_;
    std::array<VoiceModel, 4> voices_;
    std::array<GrainCloud, 4> grainClouds_;
    std::array<EvolutionEngine, 4> evolutionEngines_;
    std::array<std::unique_ptr<VoiceRow>, 4> voiceRows_;
    FastRandom randomizeRandom_{0xc0ffeeu};
};

class JerricanMainWindow : public juce::DocumentWindow {
public:
    JerricanMainWindow() : juce::DocumentWindow("Jerrican", juce::Colours::black, juce::DocumentWindow::allButtons) {
        setUsingNativeTitleBar(true);
        setContentOwned(new JerricanEditor(), true);
        setResizable(true, true);
        centreWithSize(1310, 900);
        setVisible(true);
    }

    void closeButtonPressed() override {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class JerricanApplication : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "Jerrican"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String&) override {
        window = std::make_unique<JerricanMainWindow>();
    }

    void shutdown() override {
        window = nullptr;
    }

    void systemRequestedQuit() override {
        quit();
    }

private:
    std::unique_ptr<JerricanMainWindow> window;
};

START_JUCE_APPLICATION(JerricanApplication)
