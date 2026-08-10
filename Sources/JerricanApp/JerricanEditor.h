#pragma once

#include <JuceHeader.h>

#include <BinaryData.h>

#include <algorithm>
#include <array>
#include <functional>
#include <vector>

#include "JerricanLookAndFeel.h"
#include "JerricanProcessor.h"
#include "JerricanTheme.h"
#include "MidiBindingManager.h"
#include "MidiPresetStore.h"
#include "ScenePresetStore.h"
#include "SceneState.h"
#include "VoiceModel.h"

// For StandalonePluginHolder::getInstance()/showAudioSettingsDialog() —
// the Audio/MIDI Settings button below only does anything when running
// as Standalone (see its onClick), but this header compiles fine
// regardless of format (same as StandaloneApp.h), so it's included
// unconditionally rather than guarded per-format.
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

// Content shown in the help popup (launched from the "?" button). Read-
// only, scrollable if the window is short, styled to match the rest of
// the app rather than the OS-native AlertWindow look.
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
            "Audio device and MIDI input routing are handled outside this "
            "window: in Standalone, via Options > Audio/MIDI Settings; as "
            "an AU/VST3 plugin, by your host. Bindings opens MIDI Learn, "
            "where each per-voice control applies to whichever voice is "
            "currently focused (switch focus with Voice Select pads). "
            "Transport (Play/Stop/Reset/Randomize) is bindable too, as a "
            "global action rather than a per-voice one.\n\n"
            "HOST SYNC (AU/VST3 only)\n"
            "Available only when hosted in a DAW, since Standalone has no "
            "host transport to follow. When enabled, Play/Stop follows "
            "the host's transport - so a count-in before recording starts "
            "Jerrican's grain spawning on the same downbeat. Off by "
            "default, and not part of Scenes.\n\n"
            "RECORDING (Standalone only)\n"
            "Record captures the exact final mix (everything, post-Reverb) "
            "to a timestamped WAV under ~/Music/Jerrican Recordings - click "
            "again to stop and finalize the file. Independent of the "
            "transport: you can record silence as easily as a running "
            "performance. \"Open Folder\" reveals the most recent "
            "recording in Finder. As an AU/VST3 plugin, use your host's "
            "own recording/bounce workflow instead.\n\n" +
            juce::String(juce::CharPointer_UTF8("\xc2\xa9")) +
            " 2026 Alban Bailly. All rights reserved.";

        editor_.setText(bodyText, false);
        addAndMakeVisible(editor_);
    }

    void resized() override { editor_.setBounds(getLocalBounds()); }

private:
    juce::TextEditor editor_;
};

class JerricanAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Button::Listener,
                                      private juce::Slider::Listener,
                                      private juce::Timer {
public:
    explicit JerricanAudioProcessorEditor(JerricanAudioProcessor& processorRef)
        : juce::AudioProcessorEditor(&processorRef), processor_(processorRef) {
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

        // Standalone's own window chrome used to have an "Options" menu
        // (Audio/MIDI Settings/Save state/Load state/Reset) baked into
        // its title bar — switching to a native title bar (see
        // StandaloneApp.h) makes JUCE's version of that button collapse
        // to zero height, since it's positioned relative to the
        // non-native title bar height JUCE itself no longer draws. Audio
        // device/MIDI routing still needs to be reachable somehow, so
        // this reproduces just that one entry point, themed to match the
        // rest of the header. Only meaningful in Standalone — AU/VST3
        // hosts own that routing entirely, so it's hidden there.
        addAndMakeVisible(audioSettingsButton);
        audioSettingsButton.setButtonText("Audio/MIDI...");
        audioSettingsButton.setVisible(processor_.wrapperType == juce::AudioProcessor::wrapperType_Standalone);
        audioSettingsButton.onClick = [] {
            if (auto* holder = juce::StandalonePluginHolder::getInstance()) {
                holder->showAudioSettingsDialog();
            }
        };

        // Only meaningful once actually hosted in a DAW — Standalone's
        // playhead never reports real transport data, so the toggle
        // would be inert there (see JerricanProcessor::processHostSync).
        addAndMakeVisible(hostSyncButton);
        hostSyncButton.setButtonText("Host Sync");
        hostSyncButton.setClickingTogglesState(true);
        hostSyncButton.setToggleState(processor_.hostSyncEnabled(), juce::dontSendNotification);
        hostSyncButton.setVisible(processor_.wrapperType != juce::AudioProcessor::wrapperType_Standalone);
        hostSyncButton.onClick = [this] {
            processor_.setHostSyncEnabled(hostSyncButton.getToggleState());
        };

        addAndMakeVisible(recordButton);
        recordButton.setButtonText("Record");
        recordButton.setClickingTogglesState(false);
        recordButton.onClick = [this] { toggleRecording(); };
        // A DAW hosting this as a plugin already has its own record/
        // bounce workflow — a plugin silently writing its own WAV to
        // ~/Music independent of the host is redundant there. Standalone
        // has no such host, so it's the only place this earns its keep.
        recordButton.setVisible(processor_.wrapperType == juce::AudioProcessor::wrapperType_Standalone);

        addAndMakeVisible(scenesButton);
        scenesButton.setButtonText("Scenes");
        scenesButton.onClick = [this] { showScenesPopup(); };

        addAndMakeVisible(bindingsButton);
        bindingsButton.setButtonText("Bindings");
        bindingsButton.onClick = [this] { showBindingsPopup(); };

        addAndMakeVisible(helpButton);
        helpButton.setButtonText("?");
        helpButton.onClick = [this] { showHelpPopup(); };

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

        setUpTransportKnob(masterVolumeSlider, masterVolumeLabel, "", false);
        masterVolumeSlider.setValue(1.0);

        addAndMakeVisible(statusLabel);
        statusLabel.setText("Transport idle", juce::dontSendNotification);
        statusLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
        statusLabel.setColour(juce::Label::textColourId, JerricanTheme::textSecondary);

        addAndMakeVisible(openRecordingFolderButton);
        openRecordingFolderButton.setButtonText("Open Folder");
        openRecordingFolderButton.setVisible(false);
        openRecordingFolderButton.onClick = [this] { processor_.getCurrentRecordingFile().revealToUser(); };

        for (size_t i = 0; i < voiceRows_.size(); ++i) {
            voiceRows_[i] = std::make_unique<VoiceRow>(processor_.voice(i), processor_.evolutionEngine(i), this);
            addAndMakeVisible(*voiceRows_[i]);
        }

        // Reflect whatever the processor's initial atomics already are
        // (relevant when a saved plugin instance state was restored
        // before the editor was first opened).
        refreshGlobalKnobFromAtomic(evolutionAmountSlider, processor_.evolutionAmount());
        refreshGlobalKnobFromAtomic(evolutionSpeedSlider, processor_.evolutionSpeed());
        refreshGlobalKnobFromAtomic(roomSlider, processor_.reverbRoom());
        refreshGlobalKnobFromAtomic(decaySlider, processor_.reverbDecay());
        refreshGlobalKnobFromAtomic(masterVolumeSlider, processor_.masterVolume());
        for (auto& row : voiceRows_) {
            row->refreshFromModel();
            row->refreshEvolutionToggles();
        }

        updateStatusSummary();

        setSize(1310, 900);
        // Fixed-position bottom-row layout (knob blocks, status label)
        // assumes at least this much room, so shrinking below the design
        // size would clip content — growing is fine, resized() is
        // already fully width/height-relative for everything else.
        setResizable(true, true);
        setResizeLimits(1310, 900, 2400, 1400);
        startTimerHz(30);
    }

    ~JerricanAudioProcessorEditor() override { setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override { g.fillAll(JerricanTheme::background); }

    void resized() override {
        logoImage_.setBounds(40, 16, 34, 34);
        titleLabel.setBounds(82, 16, 300, 34);
        subtitleLabel.setBounds(82, 48, getWidth() - 340, 22);

        // Header cluster: Help (rightmost), Bindings, Scenes, Record,
        // Audio/MIDI (Standalone only) — no Output/MIDI In pickers here,
        // those are the host's/Standalone's job now.
        helpButton.setBounds(getWidth() - 64, 32, 24, 24);
        bindingsButton.setBounds(getWidth() - 152, 32, 80, 24);
        scenesButton.setBounds(getWidth() - 240, 32, 70, 24);
        recordButton.setBounds(getWidth() - 330, 32, 80, 24);
        audioSettingsButton.setBounds(getWidth() - 450, 32, 110, 24);
        // Mutually exclusive with audioSettingsButton/recordButton
        // (Standalone-only vs. hosted-only), so this can share the same
        // slot without ever colliding with either.
        hostSyncButton.setBounds(getWidth() - 450, 32, 110, 24);

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
        // same bottom baseline, placed immediately to its right.
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
        // the knob blocks) so it stays put regardless of window width.
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

    // Toggled from the Record button.
    void toggleRecording() {
        if (processor_.isRecording()) {
            processor_.toggleRecording();
            recordButton.setToggleState(false, juce::dontSendNotification);
            statusLabel.setText("Recording saved", juce::dontSendNotification);
            openRecordingFolderButton.setVisible(true);
            return;
        }

        openRecordingFolderButton.setVisible(false);
        if (processor_.toggleRecording()) {
            recordButton.setToggleState(true, juce::dontSendNotification);
            statusLabel.setText("Recording...", juce::dontSendNotification);
        } else {
            statusLabel.setText("Couldn't start recording", juce::dontSendNotification);
        }
    }

    void showBindingsPopup() {
        auto content = std::make_unique<MidiBindingsPopup>(this);
        content->setSize(440, 660);
        juce::CallOutBox::launchAsynchronously(std::move(content), bindingsButton.getScreenBounds(),
                                               nullptr);
    }

    void showScenesPopup() {
        auto content = std::make_unique<ScenesPopup>(this);
        content->setSize(400, 140);
        juce::CallOutBox::launchAsynchronously(std::move(content), scenesButton.getScreenBounds(),
                                               nullptr);
    }

    SceneState captureSceneState() const { return processor_.captureSceneState(); }

    // Applies a Scene to the processor, then refreshes every Component
    // that reflects engine state — the processor itself never touches
    // juce::Component, so that part is this editor's job.
    void applySceneState(const SceneState& scene) {
        processor_.applySceneState(scene);
        for (auto& row : voiceRows_) {
            row->refreshFromModel();
            row->refreshEvolutionToggles();
        }
        refreshGlobalKnobFromAtomic(evolutionAmountSlider, processor_.evolutionAmount());
        refreshGlobalKnobFromAtomic(evolutionSpeedSlider, processor_.evolutionSpeed());
        refreshGlobalKnobFromAtomic(roomSlider, processor_.reverbRoom());
        refreshGlobalKnobFromAtomic(decaySlider, processor_.reverbDecay());
        refreshGlobalKnobFromAtomic(masterVolumeSlider, processor_.masterVolume());
        updateStatusSummary();
    }

private:
    // A self-contained voice "card": name + LED enable indicator, a
    // full-width Pitch Range band, a row of five knobs, and — below a
    // divider — a themed (teal) row of six switches opting each of those
    // six controls in/out of autonomous Evolution drift. Visuals come
    // entirely from JerricanLookAndFeel — this class only owns layout and
    // VoiceModel/EvolutionEngine wiring.
    class VoiceRow : public juce::Component, private juce::Button::Listener, private juce::Slider::Listener {
    public:
        VoiceRow(VoiceModel& voice, EvolutionEngine& evolutionEngine, JerricanAudioProcessorEditor* owner)
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
        // MIDI-bound per-voice knobs currently drive.
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
                    owner_->updateStatusSummary();
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
            // match, regardless of that parameter's toggle state — without
            // this, an Evolution-enabled parameter keeps writing its own
            // stale internal value back over your drag within ~1.5ms
            // (every 64 samples).
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
                owner_->updateStatusSummary();
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
        // toggle LEDs, without touching the values they control — needed
        // since MIDI Learn can flip these flags from off-screen.
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
        JerricanAudioProcessorEditor* owner_;
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
    // ScenesPopup and MidiBindingsPopup so the two behave identically.
    class PresetControls : public juce::Component, private juce::Timer {
    public:
        struct Callbacks {
            std::function<std::vector<std::string>()> listNames;
            std::function<bool(const std::string&)> loadNamed;
            std::function<bool(const std::string&)> saveNamed;
            std::function<bool(const std::string&)> removeNamed;
            std::function<bool(const std::string&)> matchesNamed;
            std::function<bool()> hasMeaningfulContent;
            std::function<void()> onDeleted;
            std::function<juce::String()> getCurrentName;
            std::function<void(const juce::String&)> setCurrentName;
        };

        static constexpr int kPreferredHeight = 22 + 6 + 22;

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

    // Scenes popup, opened from the "Scenes" button — a full state
    // snapshot (every voice's knobs/enabled/Evolution-toggle state, plus
    // the global Evolution/Reverb/Volume controls), distinct from MIDI
    // Learn's controller-mapping presets.
    class ScenesPopup : public juce::Component {
    public:
        explicit ScenesPopup(JerricanAudioProcessorEditor* owner)
            : presetControls_(
                  "Scene",
                  PresetControls::Callbacks{
                      .listNames = [owner] { return owner->processor_.scenePresetStore_.listPresetNames(); },
                      .loadNamed =
                          [owner](const std::string& name) {
                              SceneState scene;
                              if (!owner->processor_.scenePresetStore_.load(name, scene)) {
                                  return false;
                              }
                              owner->applySceneState(scene);
                              return true;
                          },
                      .saveNamed =
                          [owner](const std::string& name) {
                              return owner->processor_.scenePresetStore_.save(name, owner->captureSceneState());
                          },
                      .removeNamed = [owner](const std::string& name) {
                          return owner->processor_.scenePresetStore_.remove(name);
                      },
                      .matchesNamed =
                          [owner](const std::string& name) {
                              SceneState scene;
                              return owner->processor_.scenePresetStore_.load(name, scene) &&
                                     scene == owner->captureSceneState();
                          },
                      // A Scene is always a complete, meaningful snapshot
                      // — there's no "nothing to save" state.
                      .hasMeaningfulContent = [] { return true; },
                      .onDeleted = [owner] { owner->handleResetPressed(); },
                      .getCurrentName = [owner] { return owner->processor_.currentSceneName_; },
                      .setCurrentName = [owner](const juce::String& name) {
                          owner->processor_.currentSceneName_ = name;
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

    // MIDI Learn popup, opened from the "Bindings" button. One row per
    // MidiTarget (label, live binding readout, Learn, Clear), grouped:
    // per-voice controls (apply to whichever voice is currently focused),
    // voice select (moves focus), transport, and global controls. A
    // preset row at the top saves/loads/deletes named binding sets.
    class MidiBindingsPopup : public juce::Component, private juce::Timer {
    public:
        explicit MidiBindingsPopup(JerricanAudioProcessorEditor* owner)
            : owner_(owner),
              presetControls_(
                  "Preset",
                  PresetControls::Callbacks{
                      .listNames = [owner] { return owner->processor_.midiPresetStore_.listPresetNames(); },
                      .loadNamed =
                          [owner](const std::string& name) {
                              return owner->processor_.midiPresetStore_.load(name, owner->processor_.midiBindings_);
                          },
                      .saveNamed =
                          [owner](const std::string& name) {
                              return owner->processor_.midiPresetStore_.save(name, owner->processor_.midiBindings_);
                          },
                      .removeNamed = [owner](const std::string& name) {
                          return owner->processor_.midiPresetStore_.remove(name);
                      },
                      .matchesNamed =
                          [owner](const std::string& name) {
                              MidiBindingManager temp;
                              return owner->processor_.midiPresetStore_.load(name, temp) &&
                                     temp.equals(owner->processor_.midiBindings_);
                          },
                      .hasMeaningfulContent =
                          [owner] {
                              for (const auto target : kAllMidiTargets) {
                                  if (owner->processor_.midiBindings_.getBinding(target).has_value()) {
                                      return true;
                                  }
                              }
                              return false;
                          },
                      .onDeleted = [owner] { owner->processor_.midiBindings_.clearAll(); },
                      .getCurrentName = [owner] { return owner->processor_.currentMidiPresetName_; },
                      .setCurrentName = [owner](const juce::String& name) {
                          owner->processor_.currentMidiPresetName_ = name;
                      },
                  }) {
            addAndMakeVisible(presetControls_);

            // 26 target rows no longer reliably fit a fixed popup height
            // on smaller screens — everything below the preset row lives
            // in rowsContainer_, scrolled via viewport_.
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
                learnButtons_[i].onClick = [this, target] { owner_->processor_.midiBindings_.armLearn(target); };

                rowsContainer_.addAndMakeVisible(clearButtons_[i]);
                clearButtons_[i].setButtonText("Clear");
                clearButtons_[i].onClick = [this, target] {
                    owner_->processor_.midiBindings_.clearBinding(target);
                };
            }

            refreshReadouts();
            startTimerHz(10);
        }

        void resized() override {
            constexpr int padding = 12;
            const int contentWidth = getWidth() - padding * 2;

            presetControls_.setBounds(padding, padding, contentWidth, PresetControls::kPreferredHeight);
            const int viewportTop = padding + PresetControls::kPreferredHeight + 10;

            viewport_.setBounds(0, viewportTop, getWidth(), getHeight() - viewportTop);

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
                if (owner_->processor_.midiBindings_.isLearning() &&
                    owner_->processor_.midiBindings_.learningTarget() == target) {
                    targetReadouts_[i].setText("Listening...", juce::dontSendNotification);
                    continue;
                }
                targetReadouts_[i].setText(
                    describeBinding(owner_->processor_.midiBindings_.getBinding(target)),
                    juce::dontSendNotification);
            }
        }

        void timerCallback() override { refreshReadouts(); }

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

        JerricanAudioProcessorEditor* owner_;
        PresetControls presetControls_;
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

    // The four transport actions. MIDI-bound Play/Stop/Reset/Randomize
    // (see JerricanAudioProcessor::applyMidiTarget) call the processor's
    // half of this directly from the audio thread — safe, since those
    // only touch atomics/VoiceModel/EvolutionEngine, never a Component —
    // and this editor's timerCallback polls processor_.isPlaying() every
    // tick to keep Play/Stop/status in sync with MIDI-driven changes too.
    void handlePlayPressed() {
        processor_.handlePlayPressed();
        playButton.setToggleState(true, juce::dontSendNotification);
        stopButton.setEnabled(true);
        updateStatusSummary();
    }

    void handleStopPressed() {
        processor_.handleStopPressed();
        playButton.setToggleState(false, juce::dontSendNotification);
        stopButton.setEnabled(false);
        updateStatusSummary();
    }

    void handleResetPressed() {
        processor_.handleResetPressed();
        for (auto& row : voiceRows_) {
            row->refreshFromModel();
            row->resetEvolutionToggles();
        }
        statusLabel.setText("Voices reset to defaults", juce::dontSendNotification);
        updateStatusSummary();
    }

    void handleRandomizePressed() {
        processor_.handleRandomizePressed();
        for (auto& row : voiceRows_) {
            row->refreshFromModel();
        }
        updateStatusSummary();
    }

    void sliderValueChanged(juce::Slider* slider) override {
        if (slider == &evolutionAmountSlider) {
            processor_.evolutionAmount().store(static_cast<float>(evolutionAmountSlider.getValue()),
                                               std::memory_order_relaxed);
        } else if (slider == &evolutionSpeedSlider) {
            processor_.evolutionSpeed().store(static_cast<float>(evolutionSpeedSlider.getValue()),
                                              std::memory_order_relaxed);
        } else if (slider == &roomSlider) {
            processor_.reverbRoom().store(static_cast<float>(roomSlider.getValue()),
                                          std::memory_order_relaxed);
        } else if (slider == &decaySlider) {
            processor_.reverbDecay().store(static_cast<float>(decaySlider.getValue()),
                                           std::memory_order_relaxed);
        } else if (slider == &masterVolumeSlider) {
            processor_.masterVolume().store(static_cast<float>(masterVolumeSlider.getValue()),
                                            std::memory_order_relaxed);
        }
    }

    void timerCallback() override {
        // Voice rows, focus highlight, transport buttons, and the global
        // knobs all need to reflect state regardless of who changed it —
        // MIDI can change any of it at any time now, not just Evolution
        // while playing. refreshFromModel()/the drag-guard already skip
        // any control the user is actively touching.
        const int focused = processor_.getFocusedVoiceIndex();
        for (size_t i = 0; i < voiceRows_.size(); ++i) {
            voiceRows_[i]->refreshFromModel();
            voiceRows_[i]->refreshEvolutionToggles();
            voiceRows_[i]->setFocused(static_cast<int>(i) == focused);
        }

        if (playButton.getToggleState() != processor_.isPlaying()) {
            playButton.setToggleState(processor_.isPlaying(), juce::dontSendNotification);
            stopButton.setEnabled(processor_.isPlaying());
        }

        refreshGlobalKnobFromAtomic(evolutionAmountSlider, processor_.evolutionAmount());
        refreshGlobalKnobFromAtomic(evolutionSpeedSlider, processor_.evolutionSpeed());
        refreshGlobalKnobFromAtomic(roomSlider, processor_.reverbRoom());
        refreshGlobalKnobFromAtomic(decaySlider, processor_.reverbDecay());
        refreshGlobalKnobFromAtomic(masterVolumeSlider, processor_.masterVolume());
    }

    static void refreshGlobalKnobFromAtomic(juce::Slider& slider, std::atomic<float>& value) {
        if (!slider.isMouseButtonDown()) {
            slider.setValue(value.load(std::memory_order_relaxed), juce::dontSendNotification);
        }
    }

    // Called explicitly after transport/model changes, not from the 30Hz
    // timer — a recording-status message left by toggleRecording() should
    // persist on screen until the next real transport action, not get
    // stomped on the very next tick.
    void updateStatusSummary() {
        int activeVoices = 0;
        for (size_t i = 0; i < 4; ++i) {
            if (processor_.voice(i).isEnabled()) {
                ++activeVoices;
            }
        }

        const juce::String transportState = processor_.isPlaying() ? "playing" : "stopped";
        statusLabel.setText("Transport " + transportState + " — " + juce::String(activeVoices) +
                                 " active voices",
                             juce::dontSendNotification);
    }

    JerricanAudioProcessor& processor_;
    JerricanLookAndFeel lookAndFeel_;
    juce::ImageComponent logoImage_;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::TextButton helpButton;
    juce::TextButton audioSettingsButton;
    juce::TextButton hostSyncButton;
    juce::TextButton recordButton;
    juce::TextButton scenesButton;
    juce::TextButton bindingsButton;
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
    std::array<std::unique_ptr<VoiceRow>, 4> voiceRows_;
};
