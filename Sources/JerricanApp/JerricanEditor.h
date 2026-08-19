#pragma once

#include <JuceHeader.h>

#include <BinaryData.h>

#include <algorithm>
#include <array>
#include <functional>
#include <vector>

#include "BeatPulseIndicator.h"
#include "JerricanLookAndFeel.h"
#include "JerricanProcessor.h"
#include "JerricanTheme.h"
#include "MeterTable.h"
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
            "TEMPO / METER\n"
            "Tempo - the instrument's clock speed in BPM. Meter - one of "
            "seven time signatures (4/4, 3/4, 5/4, 6/8, 7/8, 9/8, 12/8) "
            "that Bass's walking pattern is built against. The beat "
            "counter next to Meter shows the current beat and doubles as "
            "a click target - click it to resync the pattern back to beat "
            "1 without touching any knob. As an AU/VST3 plugin, enabling "
            "Host Sync locks Tempo to your DAW's transport (see below).\n\n"
            "PER-VOICE CONTROLS (Volume, Pitch Range, Dissonance, Key)\n"
            "These four are universal and mean the same thing on every "
            "voice. Enabled - mutes/unmutes the voice. Volume - overall "
            "level. Pitch Range - band this voice's pitches are drawn "
            "from. Dissonance - 0 quantizes to this voice's scale so it "
            "harmonizes with others; 1 is fully free/chromatic. Key - "
            "which note that scale is rooted on; give two voices different "
            "Keys (e.g. a fifth apart) to build a deliberate chord at low "
            "Dissonance instead of unison. Key is never affected by "
            "Evolution or Randomize.\n\n"
            "BASS\n"
            "A metered walking bass, built on the Tempo/Meter clock above "
            "- every note it plays lands on a grid position, no leftover "
            "random texture.\n"
            "Dirt - drives Bass's core tone through a saturation stage: "
            "clean and full-bodied at 0, progressively more compressed/"
            "edgy/synthetic-sounding toward 1 - one consistent bass tone "
            "underneath the whole range.\n"
            "Attack - how a note starts: 0 is a slow, soft swell in; 1 is "
            "an almost-instant, hard pluck.\n"
            "Groove - rhythmic placement: 0 sits on a fixed, repeating "
            "rhythm; higher values let the timing wander around the beat.\n"
            "Busy - note density: how many notes land per bar, sparse to "
            "busy.\n"
            "Wander - harmonic movement: how far the line roams from the "
            "root through the scale; 0 is a true pedal tone, always "
            "exactly the root.\n"
            "Sustain - note length, short/punchy to long/legato.\n\n"
            "AMBIENT\n"
            "A slow-evolving generative pad in the spirit of Brian Eno's "
            "tape-loop ambient works - long overlapping layers phasing in "
            "and out on their own.\n"
            "Material - the grain's sonic substance: Glass (pure, glassy) "
            "at 0, through Wood (warmer, softly textured) at the midpoint, "
            "to Bell (richer, more metallic) at 1.\n"
            "Speed - how long each layer lasts before fading and being "
            "replaced: 0 is glacial (layers can ring for many seconds), 1 "
            "turns over noticeably faster. This is the main lever for how "
            "fast the pad feels like it's evolving.\n"
            "Layers - how many notes are sounding at once, from a single "
            "sparse tone to a dense overlapping wash. Never fully silent, "
            "even at 0 - a bass floor keeps at least one quiet layer "
            "breathing.\n"
            "Dirt - how much grain/noise texture rides under the tone: 0 "
            "is pristine and clean, 1 is a dusty, tape-hiss-like character "
            "running continuously through the note.\n\n"
            "HAZE\n"
            "A synthetic, textured layer - deliberately not another clean "
            "pad, it's meant to add a bit of grain against Bass and "
            "Ambient's cleaner tones.\n"
            "Texture - a tonal morph, Glow (soft, still a bit synthetic) "
            "to Edge (bright, buzzy); always pitched, never collapses "
            "into noise.\n"
            "Fuzz - a dedicated saturation stage, independent of Texture: "
            "0 is untouched, 1 ramps to a thick, heavily saturated, tone-"
            "shaped character.\n"
            "Drift - how long each layer lasts before turning over; low "
            "is slow-moving, high cycles noticeably faster.\n"
            "Layers - how many notes are sounding at once, sparse to "
            "dense.\n\n"
            "SPARK\n"
            "A chord-comping keyboard voice - fires diatonic chords on a "
            "metered pattern rather than single scattered notes.\n"
            "Mode - a continuous morph, Piano to Organ to Wurlitzer.\n"
            "Dirt - a grit/detune amount layered on top, across the whole "
            "Mode range.\n"
            "Thickness - chord voicing spread: 0 is close position, 1 "
            "opens the voicing out across a wider register.\n"
            "Voicing - melody to chord: 0 plays a single melodic line, 1 "
            "plays the full chord, in between blends both continuously. "
            "The melody note also wanders more off the chord tone the "
            "closer Voicing sits to 0, for real melodic movement rather "
            "than a repeated top note.\n"
            "Groove - rhythmic feel, same role as Bass's Groove: 0 locks "
            "tightly to the beat grid, 1 loosens the timing/velocity.\n"
            "Busy - how often chords land; Sustain - how long each chord "
            "rings out, short stabs to lingering pads.\n"
            "Harmony is built from the same shared scale every other "
            "voice quantizes to, so Spark always stays in key with the "
            "rest of the mix - chord progressions favor a few common "
            "moves rather than picking at random.\n\n"
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
            "Transport (Play/Stop/Reset/Randomize) and Tempo/Meter are "
            "bindable too, as global actions rather than per-voice ones.\n\n"
            "HOST SYNC (AU/VST3 only)\n"
            "Available only when hosted in a DAW, since Standalone has no "
            "host transport to follow. When enabled, Play/Stop follows "
            "the host's transport, Tempo locks continuously to the host's "
            "BPM (and greys out - it's no longer manually adjustable), and "
            "the beat grid re-snaps to the host's position on transport "
            "start or a loop jump - so a count-in before recording starts "
            "Jerrican's grain spawning, and Bass's walking pattern, on the "
            "same downbeat. Off by default, and not part of Scenes.\n\n"
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
            refreshTempoSliderEnablement();
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

        addAndMakeVisible(meterTitleLabel);
        meterTitleLabel.setText("Meter", juce::dontSendNotification);
        meterTitleLabel.setFont(juce::Font(juce::FontOptions(13.0f)).withStyle(juce::Font::bold));
        meterTitleLabel.setColour(juce::Label::textColourId, JerricanTheme::textPrimary);
        meterTitleLabel.setJustificationType(juce::Justification::centred);

        // Doubles as a click target: resyncs the pattern back to beat 1
        // without touching any knob or Scene state.
        addAndMakeVisible(beatPulseIndicator_);
        beatPulseIndicator_.onClick = [this] { processor_.requestPhaseReset(); };

        addAndMakeVisible(meterLabel);
        meterLabel.setText("Meter", juce::dontSendNotification);
        meterLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        meterLabel.setColour(juce::Label::textColourId, JerricanTheme::textSecondary);
        meterLabel.setJustificationType(juce::Justification::centred);

        addAndMakeVisible(meterBox);
        for (int i = 0; i < static_cast<int>(MeterTable::kMeters.size()); ++i) {
            meterBox.addItem(MeterTable::kMeters[static_cast<std::size_t>(i)].label, i + 1);
        }
        meterBox.setSelectedId(MeterTable::kDefaultMeterIndex + 1, juce::dontSendNotification);
        meterBox.onChange = [this] {
            const int index = meterBox.getSelectedId() - 1;
            if (index >= 0 && index < static_cast<int>(MeterTable::kMeters.size())) {
                const auto& meter = MeterTable::kMeters[static_cast<std::size_t>(index)];
                processor_.requestMeter(meter.numerator, meter.denominator);
            }
        };

        setUpTransportKnob(tempoSlider, tempoLabel, "Tempo");
        tempoSlider.setRange(40.0, 240.0);
        tempoSlider.setValue(processor_.tempo().load(std::memory_order_relaxed));

        addAndMakeVisible(statusLabel);
        statusLabel.setText("Transport idle", juce::dontSendNotification);
        statusLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
        statusLabel.setColour(juce::Label::textColourId, JerricanTheme::textSecondary);

        addAndMakeVisible(openRecordingFolderButton);
        openRecordingFolderButton.setButtonText("Open Folder");
        openRecordingFolderButton.setVisible(false);
        openRecordingFolderButton.onClick = [this] { processor_.getCurrentRecordingFile().revealToUser(); };

        for (size_t i = 0; i < voiceRows_.size(); ++i) {
            if (i == 0) {
                voiceRows_[i] = std::make_unique<BassVoiceRow>(processor_.voice(i),
                                                                processor_.evolutionEngine(i), this);
            } else if (i == 1) {
                voiceRows_[i] = std::make_unique<AmbientVoiceRow>(processor_.voice(i),
                                                                   processor_.evolutionEngine(i), this);
            } else if (i == 3) {
                voiceRows_[i] = std::make_unique<HazeVoiceRow>(processor_.voice(i),
                                                                processor_.evolutionEngine(i), this);
            } else {
                voiceRows_[i] = std::make_unique<SparkVoiceRow>(processor_.voice(i),
                                                                 processor_.evolutionEngine(i), this);
            }
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
        refreshMeterBoxSelection(processor_.meterNumeratorDisplay(), processor_.meterDenominatorDisplay());
        beatPulseIndicator_.refresh(processor_.meterNumeratorDisplay(),
                                    processor_.meterDenominatorDisplay(),
                                    processor_.currentSlot16Display());
        refreshTempoSliderEnablement();

        updateStatusSummary();

        setSize(1480, 920);
        // Fixed-position bottom-row layout (knob blocks, status label)
        // assumes at least this much room, so shrinking below the design
        // size would clip content — growing is fine, resized() is
        // already fully width/height-relative for everything else (and
        // BassVoiceRow's knob sizes adapt to whatever height the grid
        // ends up giving it).
        setResizable(true, true);
        setResizeLimits(1480, 920, 2600, 1500);
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

        // Meter/Tempo block: same title-over-knobs shape, placed
        // immediately to Master Volume's right — Meter combo + beat
        // counter share one column, Tempo gets its own.
        const int meterBlockX = volumeBlockX + volumeBlockWidth + 30;
        const int meterBlockWidth = knobColumnWidth * 2;

        meterTitleLabel.setBounds(meterBlockX, evolutionTitleTop, meterBlockWidth, evolutionTitleHeight);
        meterLabel.setBounds(meterBlockX, knobLabelTop, knobColumnWidth, knobLabelHeight);
        meterBox.setBounds(meterBlockX + 6, knobBoxTop + (knobSize - 24) / 2, knobColumnWidth - 12, 24);
        const int beatClockColumnX = meterBlockX + knobColumnWidth;
        beatPulseIndicator_.setBounds(beatClockColumnX + (knobColumnWidth - knobSize) / 2, knobBoxTop,
                                      knobSize, knobSize);

        const int tempoBlockX = meterBlockX + meterBlockWidth + 30;
        tempoLabel.setBounds(tempoBlockX, knobLabelTop, knobColumnWidth, knobLabelHeight);
        tempoSlider.setBounds(tempoBlockX + (knobColumnWidth - knobSize) / 2, knobBoxTop, knobSize,
                              knobSize + knobTextBoxHeight);

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

    // Tempo becomes host-driven (read-only) whenever hosted with Host
    // Sync enabled — otherwise it's fully manual, same as Standalone
    // always is (Standalone's playhead never reports real tempo, so the
    // toggle can't even be shown there).
    void refreshTempoSliderEnablement() {
        const bool hosted = processor_.wrapperType != juce::AudioProcessor::wrapperType_Standalone;
        tempoSlider.setEnabled(!(hosted && processor_.hostSyncEnabled()));
    }

    // Reflects the meter combo's selection onto whichever MeterTable::
    // kMeters entry matches (numerator, denominator) — used by both the
    // 30Hz refresh (meter can change via MIDI) and Scene recall.
    void refreshMeterBoxSelection(int numerator, int denominator) {
        const int index = MeterTable::findMeterIndex(numerator, denominator);
        if (meterBox.getSelectedId() != index + 1) {
            meterBox.setSelectedId(index + 1, juce::dontSendNotification);
        }
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
        tempoSlider.setValue(scene.tempo, juce::dontSendNotification);
        refreshMeterBoxSelection(scene.meterNumerator, scene.meterDenominator);
        updateStatusSummary();
    }

private:
    // Shared base for a voice "card": name + Solo/enable toggles, a
    // full-width Pitch Range band, and a Key combo — the controls that
    // stay universal and unchanged across every voice. Each of the four
    // voices now has its own bespoke subclass (BassVoiceRow, AmbientVoiceRow,
    // SparkVoiceRow, HazeVoiceRow) adding its own knob layout and Evolution
    // toggle row below the shared divider. Visuals come from
    // JerricanLookAndFeel — this class only owns layout and VoiceModel/
    // EvolutionEngine wiring.
    class VoiceRowBase : public juce::Component,
                         protected juce::Button::Listener,
                         protected juce::Slider::Listener {
    public:
        VoiceRowBase(VoiceModel& voice, EvolutionEngine& evolutionEngine,
                    JerricanAudioProcessorEditor* owner)
            : voiceRef_(voice), evolutionEngineRef_(evolutionEngine), owner_(owner) {
            addAndMakeVisible(nameLabel_);
            nameLabel_.setFont(juce::Font(juce::FontOptions(18.0f)).withStyle(juce::Font::bold));
            nameLabel_.setColour(juce::Label::textColourId, JerricanTheme::textPrimary);
            nameLabel_.setText(voiceRef_.getName(), juce::dontSendNotification);

            addAndMakeVisible(soloButton_);
            soloButton_.setButtonText("S");
            soloButton_.setName("soloToggle");
            soloButton_.setToggleState(voiceRef_.isSoloed(), juce::dontSendNotification);
            soloButton_.addListener(this);

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
        }

        ~VoiceRowBase() override = default;

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

        // Reflects the base's own controls from the model, without
        // triggering listener callbacks. Subclasses override and call this
        // first, then refresh their own bespoke controls.
        virtual void refreshFromModel() {
            if (!enabledButton_.isMouseButtonDown()) {
                enabledButton_.setToggleState(voiceRef_.isEnabled(), juce::dontSendNotification);
            }
            if (!soloButton_.isMouseButtonDown()) {
                soloButton_.setToggleState(voiceRef_.isSoloed(), juce::dontSendNotification);
            }
            if (!pitchRangeSlider_.isMouseButtonDown()) {
                pitchRangeSlider_.setMinAndMaxValues(voiceRef_.getPitchRangeLow(),
                                                      voiceRef_.getPitchRangeHigh(),
                                                      juce::dontSendNotification);
            }
            if (!rootCombo_.isPopupActive()) {
                rootCombo_.setSelectedId(voiceRef_.getRootSemitoneOffset() + 1,
                                         juce::dontSendNotification);
            }
        }

        virtual void resetEvolutionToggles() = 0;
        virtual void refreshEvolutionToggles() = 0;

    protected:
        // Lays out the four shared controls; returns the y just below the
        // Pitch Range band, where a subclass's own knob row should start.
        int layoutHeader(int padding, int contentWidth) {
            nameLabel_.setBounds(padding, padding, contentWidth - 58, 26);
            enabledButton_.setBounds(getWidth() - padding - 22, padding + 2, 22, 22);
            soloButton_.setBounds(getWidth() - padding - 22 - 6 - 22, padding + 2, 22, 22);

            const int pitchY = padding + 26 + 8;
            pitchRangeLabel_.setBounds(padding, pitchY, 160, 14);

            constexpr int rootComboWidth = 66;
            constexpr int keyLabelWidth = 28;
            keyLabel_.setBounds(padding + contentWidth - keyLabelWidth - rootComboWidth - 4, pitchY,
                                keyLabelWidth, 16);
            rootCombo_.setBounds(padding + contentWidth - rootComboWidth, pitchY, rootComboWidth, 16);

            pitchRangeSlider_.setBounds(padding, pitchY + 16, contentWidth, 22);
            return pitchY + 16 + 22 + 12;
        }

        // Returns true if `button`/`slider` was one of the base's own
        // controls (and thus already handled) — subclasses call these
        // first in their own buttonClicked/sliderValueChanged overrides,
        // before checking their own bespoke controls.
        bool handleBaseButtonClicked(juce::Button* button) {
            if (button == &enabledButton_) {
                voiceRef_.setEnabled(enabledButton_.getToggleState());
                if (owner_ != nullptr) {
                    owner_->updateStatusSummary();
                }
                return true;
            }
            if (button == &soloButton_) {
                voiceRef_.setSoloed(soloButton_.getToggleState());
                if (owner_ != nullptr) {
                    owner_->updateStatusSummary();
                }
                return true;
            }
            return false;
        }

        bool handleBaseSliderChanged(juce::Slider* slider) {
            if (slider == &pitchRangeSlider_) {
                const float low = static_cast<float>(pitchRangeSlider_.getMinValue());
                const float high = static_cast<float>(pitchRangeSlider_.getMaxValue());
                voiceRef_.setPitchRange(low, high);
                evolutionEngineRef_.resyncPitchRange(low, high);
                if (owner_ != nullptr) {
                    owner_->updateStatusSummary();
                }
                return true;
            }
            return false;
        }

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

        VoiceModel& voiceRef_;
        EvolutionEngine& evolutionEngineRef_;
        JerricanAudioProcessorEditor* owner_;
        int dividerY_ = 0;
        bool focused_ = false;

    private:
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

        static constexpr const char* kNoteNames[12] = {"A",  "A#", "B", "C",  "C#", "D",
                                                        "D#", "E",  "F", "F#", "G",  "G#"};

        juce::Label nameLabel_;
        juce::Label pitchRangeLabel_;
        juce::Label keyLabel_;
        juce::ComboBox rootCombo_;
        juce::ToggleButton enabledButton_;
        juce::ToggleButton soloButton_;
        juce::Slider pitchRangeSlider_;
    };

    // Spark's card (the sole remaining user — Bass/Ambient/Haze each grew
    // their own bespoke class): a chord-comping keyboard voice — Volume
    // alone left, then a 4+4 grid on the right (row 1: Mode/Dirt/Thickness/
    // Voicing — the tone-and-texture quartet; row 2: Groove/Busy/Sustain/
    // Dissonance — the pattern/harmony trio + Dissonance). Mode reuses the
    // old Timbre slot (getTimbre/setTimbre — a continuous Piano<->Organ
    // <->Wurlitzer morph now, see Grain::triggerSpark); Dirt reuses
    // Ambient's Cleanliness slot, same inverted-display convention (slider
    // shows 1-getCleanliness()); Thickness reuses the old Complexity slot
    // (getWander/setWander — chord voicing register spread now, see
    // ChordScale::chordTones, not grain density); Voicing reuses Bass's
    // Attack slot (getAttack/setAttack — a third reuse, after Haze's Fuzz
    // — a continuous melody<->chord morph, see GrainCloud::spawnChordNow);
    // Groove/Busy/Sustain reuse Bass's slots directly (getGroove/getBusy/
    // getSustain — same rhythmic-pattern meaning, driving SparkChordPattern
    // instead of BassGroovePattern).
    class SparkVoiceRow : public VoiceRowBase {
    public:
        SparkVoiceRow(VoiceModel& voice, EvolutionEngine& evolutionEngine,
                      JerricanAudioProcessorEditor* owner)
            : VoiceRowBase(voice, evolutionEngine, owner) {
            setUpKnob(volumeSlider_, volumeLabel_, "Volume");
            volumeSlider_.setValue(voiceRef_.getVolume());
            setUpKnob(modeSlider_, modeLabel_, "Mode");
            modeSlider_.setValue(voiceRef_.getTimbre());
            setUpKnob(dirtSlider_, dirtLabel_, "Dirt");
            dirtSlider_.setValue(1.0f - voiceRef_.getCleanliness());
            setUpKnob(thicknessSlider_, thicknessLabel_, "Thickness");
            thicknessSlider_.setValue(voiceRef_.getWander());
            setUpKnob(voicingSlider_, voicingLabel_, "Voicing");
            voicingSlider_.setValue(voiceRef_.getAttack());

            setUpKnob(grooveSlider_, grooveLabel_, "Groove");
            grooveSlider_.setValue(voiceRef_.getGroove());
            setUpKnob(busySlider_, busyLabel_, "Busy");
            busySlider_.setValue(voiceRef_.getBusy());
            setUpKnob(sustainSlider_, sustainLabel_, "Sustain");
            sustainSlider_.setValue(voiceRef_.getSustain());
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

        // Same Volume-alone-left + shared-column-width 3+4 grid template
        // as BassVoiceRow — see AmbientVoiceRow::resized() for the
        // reference shape this copies.
        void resized() override {
            constexpr int padding = 14;
            const int contentWidth = getWidth() - padding * 2;
            const int knobAreaTop = layoutHeader(padding, contentWidth);

            constexpr int knobLabelHeight = 14;
            constexpr int knobTextBoxHeight = 16;
            constexpr int rowGap = 8;
            constexpr int toggleCaptionHeight = 12;
            constexpr int toggleSize = 16;
            constexpr int footerHeight = 10 + 16 + 18 + toggleCaptionHeight + 2 + toggleSize;

            const int knobAreaHeight =
                std::max(80, getHeight() - knobAreaTop - footerHeight - padding);

            constexpr int leftColumnWidth = 96;
            const int smallKnobSize = std::max(
                32, (knobAreaHeight - rowGap - 2 * (knobLabelHeight + 2 + knobTextBoxHeight)) / 2);
            const int volumeKnobSize = std::max(
                smallKnobSize,
                std::min(84, knobAreaHeight - knobLabelHeight - 2 - knobTextBoxHeight));

            const int volumeBlockHeight = knobLabelHeight + 2 + volumeKnobSize + knobTextBoxHeight;
            const int volumeTop = knobAreaTop + std::max(0, (knobAreaHeight - volumeBlockHeight) / 2);
            volumeLabel_.setBounds(padding, volumeTop, leftColumnWidth, knobLabelHeight);
            volumeSlider_.setBounds(padding + (leftColumnWidth - volumeKnobSize) / 2,
                                    volumeTop + knobLabelHeight + 2, volumeKnobSize,
                                    volumeKnobSize + knobTextBoxHeight);

            const int rightX = padding + leftColumnWidth + 10;
            const int rightWidth = contentWidth - leftColumnWidth - 10;
            constexpr int row1Count = 4;
            constexpr int row2Count = 4;
            constexpr int gridCols = row2Count;
            const int colWidth = rightWidth / gridCols;

            juce::Slider* row1Knobs[row1Count] = {&modeSlider_, &dirtSlider_, &thicknessSlider_,
                                                  &voicingSlider_};
            juce::Label* row1Labels[row1Count] = {&modeLabel_, &dirtLabel_, &thicknessLabel_,
                                                  &voicingLabel_};
            for (int i = 0; i < row1Count; ++i) {
                const int columnX = rightX + i * colWidth;
                const int knobX = columnX + (colWidth - smallKnobSize) / 2;
                row1Labels[i]->setBounds(columnX, knobAreaTop, colWidth, knobLabelHeight);
                row1Knobs[i]->setBounds(knobX, knobAreaTop + knobLabelHeight + 2, smallKnobSize,
                                        smallKnobSize + knobTextBoxHeight);
            }

            const int row1Bottom = knobAreaTop + knobLabelHeight + 2 + smallKnobSize + knobTextBoxHeight;
            const int row2Y = row1Bottom + rowGap;

            juce::Slider* row2Knobs[row2Count] = {&grooveSlider_, &busySlider_, &sustainSlider_,
                                                  &dissonanceSlider_};
            juce::Label* row2Labels[row2Count] = {&grooveLabel_, &busyLabel_, &sustainLabel_,
                                                  &dissonanceLabel_};
            for (int i = 0; i < row2Count; ++i) {
                const int columnX = rightX + i * colWidth;
                const int knobX = columnX + (colWidth - smallKnobSize) / 2;
                row2Labels[i]->setBounds(columnX, row2Y, colWidth, knobLabelHeight);
                row2Knobs[i]->setBounds(knobX, row2Y + knobLabelHeight + 2, smallKnobSize,
                                        smallKnobSize + knobTextBoxHeight);
            }

            const int row2Bottom = row2Y + knobLabelHeight + 2 + smallKnobSize + knobTextBoxHeight;
            dividerY_ = row2Bottom + 10;

            const int evolutionLabelY = row2Bottom + 16;
            evolutionSectionLabel_.setBounds(padding, evolutionLabelY, 100, 14);

            const int toggleRowY = evolutionLabelY + 18;
            const int toggleColumnWidth = contentWidth / kEvolutionToggleCount;

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
            if (handleBaseButtonClicked(button)) {
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
            } else if (button == &modeEvoToggle_) {
                const bool on = modeEvoToggle_.getToggleState();
                evolutionEngineRef_.setTimbreEnabled(on);
                if (on) evolutionEngineRef_.resyncTimbre(voiceRef_.getTimbre());
            } else if (button == &dirtEvoToggle_) {
                const bool on = dirtEvoToggle_.getToggleState();
                evolutionEngineRef_.setCleanlinessEnabled(on);
                if (on) evolutionEngineRef_.resyncCleanliness(voiceRef_.getCleanliness());
            } else if (button == &thicknessEvoToggle_) {
                const bool on = thicknessEvoToggle_.getToggleState();
                evolutionEngineRef_.setWanderEnabled(on);
                if (on) evolutionEngineRef_.resyncWander(voiceRef_.getWander());
            } else if (button == &voicingEvoToggle_) {
                const bool on = voicingEvoToggle_.getToggleState();
                evolutionEngineRef_.setAttackEnabled(on);
                if (on) evolutionEngineRef_.resyncAttack(voiceRef_.getAttack());
            } else if (button == &grooveEvoToggle_) {
                const bool on = grooveEvoToggle_.getToggleState();
                evolutionEngineRef_.setGrooveEnabled(on);
                if (on) evolutionEngineRef_.resyncGroove(voiceRef_.getGroove());
            } else if (button == &busyEvoToggle_) {
                const bool on = busyEvoToggle_.getToggleState();
                evolutionEngineRef_.setBusyEnabled(on);
                if (on) evolutionEngineRef_.resyncBusy(voiceRef_.getBusy());
            } else if (button == &sustainEvoToggle_) {
                const bool on = sustainEvoToggle_.getToggleState();
                evolutionEngineRef_.setSustainEnabled(on);
                if (on) evolutionEngineRef_.resyncSustain(voiceRef_.getSustain());
            } else if (button == &dissonanceEvoToggle_) {
                const bool on = dissonanceEvoToggle_.getToggleState();
                evolutionEngineRef_.setDissonanceEnabled(on);
                if (on) evolutionEngineRef_.resyncDissonance(voiceRef_.getDissonance());
            }
        }

        void sliderValueChanged(juce::Slider* slider) override {
            if (handleBaseSliderChanged(slider)) {
                return;
            }
            if (slider == &volumeSlider_) {
                const float value = static_cast<float>(volumeSlider_.getValue());
                voiceRef_.setVolume(value);
                evolutionEngineRef_.resyncVolume(value);
            } else if (slider == &modeSlider_) {
                const float value = static_cast<float>(modeSlider_.getValue());
                voiceRef_.setTimbre(value);
                evolutionEngineRef_.resyncTimbre(value);
            } else if (slider == &dirtSlider_) {
                const float displayed = static_cast<float>(dirtSlider_.getValue());
                const float stored = 1.0f - displayed;
                voiceRef_.setCleanliness(stored);
                evolutionEngineRef_.resyncCleanliness(stored);
            } else if (slider == &thicknessSlider_) {
                const float value = static_cast<float>(thicknessSlider_.getValue());
                voiceRef_.setWander(value);
                evolutionEngineRef_.resyncWander(value);
            } else if (slider == &voicingSlider_) {
                const float value = static_cast<float>(voicingSlider_.getValue());
                voiceRef_.setAttack(value);
                evolutionEngineRef_.resyncAttack(value);
            } else if (slider == &grooveSlider_) {
                const float value = static_cast<float>(grooveSlider_.getValue());
                voiceRef_.setGroove(value);
                evolutionEngineRef_.resyncGroove(value);
            } else if (slider == &busySlider_) {
                const float value = static_cast<float>(busySlider_.getValue());
                voiceRef_.setBusy(value);
                evolutionEngineRef_.resyncBusy(value);
            } else if (slider == &sustainSlider_) {
                const float value = static_cast<float>(sustainSlider_.getValue());
                voiceRef_.setSustain(value);
                evolutionEngineRef_.resyncSustain(value);
            } else if (slider == &dissonanceSlider_) {
                const float value = static_cast<float>(dissonanceSlider_.getValue());
                voiceRef_.setDissonance(value);
                evolutionEngineRef_.resyncDissonance(value);
            }

            if (owner_ != nullptr) {
                owner_->updateStatusSummary();
            }
        }

        void refreshFromModel() override {
            VoiceRowBase::refreshFromModel();
            if (!volumeSlider_.isMouseButtonDown()) {
                volumeSlider_.setValue(voiceRef_.getVolume(), juce::dontSendNotification);
            }
            if (!modeSlider_.isMouseButtonDown()) {
                modeSlider_.setValue(voiceRef_.getTimbre(), juce::dontSendNotification);
            }
            if (!dirtSlider_.isMouseButtonDown()) {
                dirtSlider_.setValue(1.0f - voiceRef_.getCleanliness(), juce::dontSendNotification);
            }
            if (!thicknessSlider_.isMouseButtonDown()) {
                thicknessSlider_.setValue(voiceRef_.getWander(), juce::dontSendNotification);
            }
            if (!voicingSlider_.isMouseButtonDown()) {
                voicingSlider_.setValue(voiceRef_.getAttack(), juce::dontSendNotification);
            }
            if (!grooveSlider_.isMouseButtonDown()) {
                grooveSlider_.setValue(voiceRef_.getGroove(), juce::dontSendNotification);
            }
            if (!busySlider_.isMouseButtonDown()) {
                busySlider_.setValue(voiceRef_.getBusy(), juce::dontSendNotification);
            }
            if (!sustainSlider_.isMouseButtonDown()) {
                sustainSlider_.setValue(voiceRef_.getSustain(), juce::dontSendNotification);
            }
            if (!dissonanceSlider_.isMouseButtonDown()) {
                dissonanceSlider_.setValue(voiceRef_.getDissonance(), juce::dontSendNotification);
            }
        }

        void resetEvolutionToggles() override {
            for (auto* toggle : evolutionToggles()) {
                toggle->setToggleState(true, juce::dontSendNotification);
            }
            evolutionEngineRef_.setVolumeEnabled(true);
            evolutionEngineRef_.setPitchRangeEnabled(true);
            evolutionEngineRef_.setTimbreEnabled(true);
            evolutionEngineRef_.setCleanlinessEnabled(true);
            evolutionEngineRef_.setWanderEnabled(true);
            evolutionEngineRef_.setAttackEnabled(true);
            evolutionEngineRef_.setGrooveEnabled(true);
            evolutionEngineRef_.setBusyEnabled(true);
            evolutionEngineRef_.setSustainEnabled(true);
            evolutionEngineRef_.setDissonanceEnabled(true);
        }

        void refreshEvolutionToggles() override {
            volumeEvoToggle_.setToggleState(evolutionEngineRef_.isVolumeEnabled(),
                                            juce::dontSendNotification);
            pitchRangeEvoToggle_.setToggleState(evolutionEngineRef_.isPitchRangeEnabled(),
                                                juce::dontSendNotification);
            modeEvoToggle_.setToggleState(evolutionEngineRef_.isTimbreEnabled(),
                                          juce::dontSendNotification);
            dirtEvoToggle_.setToggleState(evolutionEngineRef_.isCleanlinessEnabled(),
                                          juce::dontSendNotification);
            thicknessEvoToggle_.setToggleState(evolutionEngineRef_.isWanderEnabled(),
                                               juce::dontSendNotification);
            voicingEvoToggle_.setToggleState(evolutionEngineRef_.isAttackEnabled(),
                                             juce::dontSendNotification);
            grooveEvoToggle_.setToggleState(evolutionEngineRef_.isGrooveEnabled(),
                                            juce::dontSendNotification);
            busyEvoToggle_.setToggleState(evolutionEngineRef_.isBusyEnabled(),
                                          juce::dontSendNotification);
            sustainEvoToggle_.setToggleState(evolutionEngineRef_.isSustainEnabled(),
                                             juce::dontSendNotification);
            dissonanceEvoToggle_.setToggleState(evolutionEngineRef_.isDissonanceEnabled(),
                                                juce::dontSendNotification);
        }

    private:
        static constexpr int kEvolutionToggleCount = 10;
        static constexpr const char* kEvolutionCaptions[kEvolutionToggleCount] = {
            "Volume", "Range",  "Mode",  "Dirt",    "Thickness",
            "Voicing", "Groove", "Busy", "Sustain", "Dissonance"};

        std::array<juce::Label*, kEvolutionToggleCount> evolutionCaptionLabels() {
            return {&volumeEvoLabel_, &pitchRangeEvoLabel_, &modeEvoLabel_,   &dirtEvoLabel_,
                    &thicknessEvoLabel_, &voicingEvoLabel_, &grooveEvoLabel_, &busyEvoLabel_,
                    &sustainEvoLabel_, &dissonanceEvoLabel_};
        }

        std::array<juce::ToggleButton*, kEvolutionToggleCount> evolutionToggles() {
            return {&volumeEvoToggle_,  &pitchRangeEvoToggle_, &modeEvoToggle_,    &dirtEvoToggle_,
                    &thicknessEvoToggle_, &voicingEvoToggle_,  &grooveEvoToggle_,  &busyEvoToggle_,
                    &sustainEvoToggle_, &dissonanceEvoToggle_};
        }

        juce::Label volumeLabel_;
        juce::Label modeLabel_;
        juce::Label dirtLabel_;
        juce::Label thicknessLabel_;
        juce::Label voicingLabel_;
        juce::Label grooveLabel_;
        juce::Label busyLabel_;
        juce::Label sustainLabel_;
        juce::Label dissonanceLabel_;
        juce::Slider volumeSlider_;
        juce::Slider modeSlider_;
        juce::Slider dirtSlider_;
        juce::Slider thicknessSlider_;
        juce::Slider voicingSlider_;
        juce::Slider grooveSlider_;
        juce::Slider busySlider_;
        juce::Slider sustainSlider_;
        juce::Slider dissonanceSlider_;
        juce::Label evolutionSectionLabel_;
        juce::Label volumeEvoLabel_;
        juce::Label pitchRangeEvoLabel_;
        juce::Label modeEvoLabel_;
        juce::Label dirtEvoLabel_;
        juce::Label thicknessEvoLabel_;
        juce::Label voicingEvoLabel_;
        juce::Label grooveEvoLabel_;
        juce::Label busyEvoLabel_;
        juce::Label sustainEvoLabel_;
        juce::Label dissonanceEvoLabel_;
        juce::ToggleButton volumeEvoToggle_;
        juce::ToggleButton pitchRangeEvoToggle_;
        juce::ToggleButton modeEvoToggle_;
        juce::ToggleButton dirtEvoToggle_;
        juce::ToggleButton thicknessEvoToggle_;
        juce::ToggleButton voicingEvoToggle_;
        juce::ToggleButton grooveEvoToggle_;
        juce::ToggleButton busyEvoToggle_;
        juce::ToggleButton sustainEvoToggle_;
        juce::ToggleButton dissonanceEvoToggle_;
    };

    // Haze's bespoke card: same Volume-alone-left + shared-column-width
    // grid template every voice card uses — see AmbientVoiceRow::resized()
    // for the reference shape. Texture (relabels Timbre — same
    // `timbre_` field, new DSP: an explicit tonal Glow<->Edge morph via
    // Grain::triggerHaze, replacing the random pickWaveform lottery that
    // made Timbre unpredictable and could accidentally collapse into a
    // white-noise wash near 1). Drift (relabels Motion — same `groove_`
    // field, new DSP: scales grain duration via
    // GrainCloud::maybeSpawnHazeGrain, the dominant audible effect,
    // mirroring Ambient's Speed fix — the old pitch-drift-retarget effect
    // alone was too subtle to read as doing anything). Layers (relabels
    // Complexity — same `wander_` field, same density mechanism, just
    // named for what it actually is).
    class HazeVoiceRow : public VoiceRowBase {
    public:
        HazeVoiceRow(VoiceModel& voice, EvolutionEngine& evolutionEngine,
                    JerricanAudioProcessorEditor* owner)
            : VoiceRowBase(voice, evolutionEngine, owner) {
            setUpKnob(volumeSlider_, volumeLabel_, "Volume");
            volumeSlider_.setValue(voiceRef_.getVolume());

            setUpKnob(textureSlider_, textureLabel_, "Texture");
            textureSlider_.setValue(voiceRef_.getTimbre());

            setUpKnob(fuzzSlider_, fuzzLabel_, "Fuzz");
            fuzzSlider_.setValue(voiceRef_.getAttack());

            setUpKnob(driftSlider_, driftLabel_, "Drift");
            driftSlider_.setValue(voiceRef_.getGroove());

            setUpKnob(layersSlider_, layersLabel_, "Layers");
            layersSlider_.setValue(voiceRef_.getWander());

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

        // Same Volume-alone-left + shared-column-width 2-row grid template
        // as Bass/Ambient — see AmbientVoiceRow::resized() for the
        // reference shape this copies.
        void resized() override {
            constexpr int padding = 14;
            const int contentWidth = getWidth() - padding * 2;
            const int knobAreaTop = layoutHeader(padding, contentWidth);

            constexpr int knobLabelHeight = 14;
            constexpr int knobTextBoxHeight = 16;
            constexpr int rowGap = 8;
            constexpr int toggleCaptionHeight = 12;
            constexpr int toggleSize = 16;
            constexpr int footerHeight = 10 + 16 + 18 + toggleCaptionHeight + 2 + toggleSize;

            const int knobAreaHeight =
                std::max(80, getHeight() - knobAreaTop - footerHeight - padding);

            constexpr int leftColumnWidth = 96;
            const int smallKnobSize = std::max(
                32, (knobAreaHeight - rowGap - 2 * (knobLabelHeight + 2 + knobTextBoxHeight)) / 2);
            const int volumeKnobSize = std::max(
                smallKnobSize,
                std::min(84, knobAreaHeight - knobLabelHeight - 2 - knobTextBoxHeight));

            const int volumeBlockHeight = knobLabelHeight + 2 + volumeKnobSize + knobTextBoxHeight;
            const int volumeTop = knobAreaTop + std::max(0, (knobAreaHeight - volumeBlockHeight) / 2);
            volumeLabel_.setBounds(padding, volumeTop, leftColumnWidth, knobLabelHeight);
            volumeSlider_.setBounds(padding + (leftColumnWidth - volumeKnobSize) / 2,
                                    volumeTop + knobLabelHeight + 2, volumeKnobSize,
                                    volumeKnobSize + knobTextBoxHeight);

            const int rightX = padding + leftColumnWidth + 10;
            const int rightWidth = contentWidth - leftColumnWidth - 10;
            constexpr int gridCols = 3;
            const int colWidth = rightWidth / gridCols;

            juce::Slider* row1Knobs[gridCols] = {&textureSlider_, &fuzzSlider_, &driftSlider_};
            juce::Label* row1Labels[gridCols] = {&textureLabel_, &fuzzLabel_, &driftLabel_};
            for (int i = 0; i < gridCols; ++i) {
                const int columnX = rightX + i * colWidth;
                const int knobX = columnX + (colWidth - smallKnobSize) / 2;
                row1Labels[i]->setBounds(columnX, knobAreaTop, colWidth, knobLabelHeight);
                row1Knobs[i]->setBounds(knobX, knobAreaTop + knobLabelHeight + 2, smallKnobSize,
                                        smallKnobSize + knobTextBoxHeight);
            }

            const int row1Bottom = knobAreaTop + knobLabelHeight + 2 + smallKnobSize + knobTextBoxHeight;
            const int row2Y = row1Bottom + rowGap;

            // Only 2 controls this row — shares row 1's column width so
            // the two rows stay visually aligned, third column left empty.
            constexpr int row2Count = 2;
            juce::Slider* row2Knobs[row2Count] = {&layersSlider_, &dissonanceSlider_};
            juce::Label* row2Labels[row2Count] = {&layersLabel_, &dissonanceLabel_};
            for (int i = 0; i < row2Count; ++i) {
                const int columnX = rightX + i * colWidth;
                const int knobX = columnX + (colWidth - smallKnobSize) / 2;
                row2Labels[i]->setBounds(columnX, row2Y, colWidth, knobLabelHeight);
                row2Knobs[i]->setBounds(knobX, row2Y + knobLabelHeight + 2, smallKnobSize,
                                        smallKnobSize + knobTextBoxHeight);
            }

            const int row2Bottom = row2Y + knobLabelHeight + 2 + smallKnobSize + knobTextBoxHeight;
            dividerY_ = row2Bottom + 10;

            const int evolutionLabelY = row2Bottom + 16;
            evolutionSectionLabel_.setBounds(padding, evolutionLabelY, 100, 14);

            const int toggleRowY = evolutionLabelY + 18;
            const int toggleColumnWidth = contentWidth / kEvolutionToggleCount;

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
            if (handleBaseButtonClicked(button)) {
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
            } else if (button == &textureEvoToggle_) {
                const bool on = textureEvoToggle_.getToggleState();
                evolutionEngineRef_.setTimbreEnabled(on);
                if (on) evolutionEngineRef_.resyncTimbre(voiceRef_.getTimbre());
            } else if (button == &fuzzEvoToggle_) {
                const bool on = fuzzEvoToggle_.getToggleState();
                evolutionEngineRef_.setAttackEnabled(on);
                if (on) evolutionEngineRef_.resyncAttack(voiceRef_.getAttack());
            } else if (button == &driftEvoToggle_) {
                const bool on = driftEvoToggle_.getToggleState();
                evolutionEngineRef_.setGrooveEnabled(on);
                if (on) evolutionEngineRef_.resyncGroove(voiceRef_.getGroove());
            } else if (button == &layersEvoToggle_) {
                const bool on = layersEvoToggle_.getToggleState();
                evolutionEngineRef_.setWanderEnabled(on);
                if (on) evolutionEngineRef_.resyncWander(voiceRef_.getWander());
            } else if (button == &dissonanceEvoToggle_) {
                const bool on = dissonanceEvoToggle_.getToggleState();
                evolutionEngineRef_.setDissonanceEnabled(on);
                if (on) evolutionEngineRef_.resyncDissonance(voiceRef_.getDissonance());
            }
        }

        void sliderValueChanged(juce::Slider* slider) override {
            if (handleBaseSliderChanged(slider)) {
                return;
            }
            if (slider == &volumeSlider_) {
                const float value = static_cast<float>(volumeSlider_.getValue());
                voiceRef_.setVolume(value);
                evolutionEngineRef_.resyncVolume(value);
            } else if (slider == &textureSlider_) {
                const float value = static_cast<float>(textureSlider_.getValue());
                voiceRef_.setTimbre(value);
                evolutionEngineRef_.resyncTimbre(value);
            } else if (slider == &fuzzSlider_) {
                const float value = static_cast<float>(fuzzSlider_.getValue());
                voiceRef_.setAttack(value);
                evolutionEngineRef_.resyncAttack(value);
            } else if (slider == &driftSlider_) {
                const float value = static_cast<float>(driftSlider_.getValue());
                voiceRef_.setGroove(value);
                evolutionEngineRef_.resyncGroove(value);
            } else if (slider == &layersSlider_) {
                const float value = static_cast<float>(layersSlider_.getValue());
                voiceRef_.setWander(value);
                evolutionEngineRef_.resyncWander(value);
            } else if (slider == &dissonanceSlider_) {
                const float value = static_cast<float>(dissonanceSlider_.getValue());
                voiceRef_.setDissonance(value);
                evolutionEngineRef_.resyncDissonance(value);
            }

            if (owner_ != nullptr) {
                owner_->updateStatusSummary();
            }
        }

        void refreshFromModel() override {
            VoiceRowBase::refreshFromModel();
            if (!volumeSlider_.isMouseButtonDown()) {
                volumeSlider_.setValue(voiceRef_.getVolume(), juce::dontSendNotification);
            }
            if (!textureSlider_.isMouseButtonDown()) {
                textureSlider_.setValue(voiceRef_.getTimbre(), juce::dontSendNotification);
            }
            if (!fuzzSlider_.isMouseButtonDown()) {
                fuzzSlider_.setValue(voiceRef_.getAttack(), juce::dontSendNotification);
            }
            if (!driftSlider_.isMouseButtonDown()) {
                driftSlider_.setValue(voiceRef_.getGroove(), juce::dontSendNotification);
            }
            if (!layersSlider_.isMouseButtonDown()) {
                layersSlider_.setValue(voiceRef_.getWander(), juce::dontSendNotification);
            }
            if (!dissonanceSlider_.isMouseButtonDown()) {
                dissonanceSlider_.setValue(voiceRef_.getDissonance(), juce::dontSendNotification);
            }
        }

        void resetEvolutionToggles() override {
            for (auto* toggle : evolutionToggles()) {
                toggle->setToggleState(true, juce::dontSendNotification);
            }
            evolutionEngineRef_.setVolumeEnabled(true);
            evolutionEngineRef_.setPitchRangeEnabled(true);
            evolutionEngineRef_.setTimbreEnabled(true);
            evolutionEngineRef_.setAttackEnabled(true);
            evolutionEngineRef_.setGrooveEnabled(true);
            evolutionEngineRef_.setWanderEnabled(true);
            evolutionEngineRef_.setDissonanceEnabled(true);
        }

        void refreshEvolutionToggles() override {
            volumeEvoToggle_.setToggleState(evolutionEngineRef_.isVolumeEnabled(),
                                            juce::dontSendNotification);
            pitchRangeEvoToggle_.setToggleState(evolutionEngineRef_.isPitchRangeEnabled(),
                                                juce::dontSendNotification);
            textureEvoToggle_.setToggleState(evolutionEngineRef_.isTimbreEnabled(),
                                             juce::dontSendNotification);
            fuzzEvoToggle_.setToggleState(evolutionEngineRef_.isAttackEnabled(),
                                          juce::dontSendNotification);
            driftEvoToggle_.setToggleState(evolutionEngineRef_.isGrooveEnabled(),
                                           juce::dontSendNotification);
            layersEvoToggle_.setToggleState(evolutionEngineRef_.isWanderEnabled(),
                                            juce::dontSendNotification);
            dissonanceEvoToggle_.setToggleState(evolutionEngineRef_.isDissonanceEnabled(),
                                                juce::dontSendNotification);
        }

    private:
        static constexpr int kEvolutionToggleCount = 7;
        static constexpr const char* kEvolutionCaptions[kEvolutionToggleCount] = {
            "Volume", "Range", "Texture", "Fuzz", "Drift", "Layers", "Dissonance"};

        std::array<juce::Label*, kEvolutionToggleCount> evolutionCaptionLabels() {
            return {&volumeEvoLabel_, &pitchRangeEvoLabel_, &textureEvoLabel_, &fuzzEvoLabel_,
                    &driftEvoLabel_,  &layersEvoLabel_,     &dissonanceEvoLabel_};
        }

        std::array<juce::ToggleButton*, kEvolutionToggleCount> evolutionToggles() {
            return {&volumeEvoToggle_, &pitchRangeEvoToggle_, &textureEvoToggle_,
                    &fuzzEvoToggle_,   &driftEvoToggle_,      &layersEvoToggle_,
                    &dissonanceEvoToggle_};
        }

        juce::Label volumeLabel_;
        juce::Label textureLabel_;
        juce::Label fuzzLabel_;
        juce::Label driftLabel_;
        juce::Label layersLabel_;
        juce::Label dissonanceLabel_;
        juce::Slider volumeSlider_;
        juce::Slider textureSlider_;
        juce::Slider fuzzSlider_;
        juce::Slider driftSlider_;
        juce::Slider layersSlider_;
        juce::Slider dissonanceSlider_;
        juce::Label evolutionSectionLabel_;
        juce::Label volumeEvoLabel_;
        juce::Label pitchRangeEvoLabel_;
        juce::Label textureEvoLabel_;
        juce::Label fuzzEvoLabel_;
        juce::Label driftEvoLabel_;
        juce::Label layersEvoLabel_;
        juce::Label dissonanceEvoLabel_;
        juce::ToggleButton volumeEvoToggle_;
        juce::ToggleButton pitchRangeEvoToggle_;
        juce::ToggleButton textureEvoToggle_;
        juce::ToggleButton fuzzEvoToggle_;
        juce::ToggleButton driftEvoToggle_;
        juce::ToggleButton layersEvoToggle_;
        juce::ToggleButton dissonanceEvoToggle_;
    };

    // Bass's bespoke card: two knob rows — Volume/Timbre/Dissonance (the
    // controls that existed before, just retuned) on top, Groove/Busy/
    // Wander/Sustain (the new bespoke controls) below — plus a matching
    // 8-toggle Evolution row (the 6 above, minus nothing, plus Busy and
    // Sustain).
    class BassVoiceRow : public VoiceRowBase {
    public:
        BassVoiceRow(VoiceModel& voice, EvolutionEngine& evolutionEngine,
                    JerricanAudioProcessorEditor* owner)
            : VoiceRowBase(voice, evolutionEngine, owner) {
            setUpKnob(volumeSlider_, volumeLabel_, "Volume");
            volumeSlider_.setValue(voiceRef_.getVolume());
            setUpKnob(timbreSlider_, timbreLabel_, "Dirt");
            timbreSlider_.setValue(voiceRef_.getTimbre());
            setUpKnob(attackSlider_, attackLabel_, "Attack");
            attackSlider_.setValue(voiceRef_.getAttack());
            setUpKnob(dissonanceSlider_, dissonanceLabel_, "Dissonance");
            dissonanceSlider_.setValue(voiceRef_.getDissonance());

            setUpKnob(grooveSlider_, grooveLabel_, "Groove");
            grooveSlider_.setValue(voiceRef_.getGroove());
            setUpKnob(busySlider_, busyLabel_, "Busy");
            busySlider_.setValue(voiceRef_.getBusy());
            setUpKnob(wanderSlider_, wanderLabel_, "Wander");
            wanderSlider_.setValue(voiceRef_.getWander());
            setUpKnob(sustainSlider_, sustainLabel_, "Sustain");
            sustainSlider_.setValue(voiceRef_.getSustain());

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

        // Volume sits alone in a fixed-width left column, vertically
        // centered across the whole knob area — the other seven controls
        // are a smaller grid to its right (row 1: 3 tone-shaping controls,
        // row 2: 4 rhythmic/note-shape controls). Sized adaptively from
        // whatever height the shared 2x2 voice grid gives this card
        // (computed, not hardcoded), so every card fits the same shared
        // grid cell height regardless of its own row count.
        void resized() override {
            constexpr int padding = 14;
            const int contentWidth = getWidth() - padding * 2;
            const int knobAreaTop = layoutHeader(padding, contentWidth);

            constexpr int knobLabelHeight = 14;
            constexpr int knobTextBoxHeight = 16;
            constexpr int rowGap = 8;
            constexpr int toggleCaptionHeight = 12;
            constexpr int toggleSize = 16;
            // Same divider(10) + evo-label(16) + toggle-row(18 + caption +
            // gap + toggle) footer shape every card uses.
            constexpr int footerHeight = 10 + 16 + 18 + toggleCaptionHeight + 2 + toggleSize;

            const int knobAreaHeight =
                std::max(80, getHeight() - knobAreaTop - footerHeight - padding);

            constexpr int leftColumnWidth = 96;
            const int smallKnobSize = std::max(
                32, (knobAreaHeight - rowGap - 2 * (knobLabelHeight + 2 + knobTextBoxHeight)) / 2);
            const int volumeKnobSize = std::max(
                smallKnobSize,
                std::min(84, knobAreaHeight - knobLabelHeight - 2 - knobTextBoxHeight));

            const int volumeBlockHeight = knobLabelHeight + 2 + volumeKnobSize + knobTextBoxHeight;
            const int volumeTop = knobAreaTop + std::max(0, (knobAreaHeight - volumeBlockHeight) / 2);
            volumeLabel_.setBounds(padding, volumeTop, leftColumnWidth, knobLabelHeight);
            volumeSlider_.setBounds(padding + (leftColumnWidth - volumeKnobSize) / 2,
                                    volumeTop + knobLabelHeight + 2, volumeKnobSize,
                                    volumeKnobSize + knobTextBoxHeight);

            const int rightX = padding + leftColumnWidth + 10;
            const int rightWidth = contentWidth - leftColumnWidth - 10;
            constexpr int gridCols = 4;
            const int colWidth = rightWidth / gridCols;

            // Sustain sits alone in column 3 — no row 2 counterpart shares
            // that column, so it's grouped into row 1 (top-aligned) rather
            // than row 2, matching "an orphaned single control aligns to
            // the top of its column, not the bottom." Row 1 is 4 wide
            // here (Dirt/Attack/Dissonance + Sustain); row 2 stays 3
            // (Groove/Busy/Wander), leaving row 2's column 3 empty instead.
            constexpr int row1Count = 4;
            constexpr int row2Count = 3;

            juce::Slider* row1Knobs[row1Count] = {&timbreSlider_, &attackSlider_, &dissonanceSlider_,
                                                  &sustainSlider_};
            juce::Label* row1Labels[row1Count] = {&timbreLabel_, &attackLabel_, &dissonanceLabel_,
                                                  &sustainLabel_};
            for (int i = 0; i < row1Count; ++i) {
                const int columnX = rightX + i * colWidth;
                const int knobX = columnX + (colWidth - smallKnobSize) / 2;
                row1Labels[i]->setBounds(columnX, knobAreaTop, colWidth, knobLabelHeight);
                row1Knobs[i]->setBounds(knobX, knobAreaTop + knobLabelHeight + 2, smallKnobSize,
                                        smallKnobSize + knobTextBoxHeight);
            }

            const int row1Bottom = knobAreaTop + knobLabelHeight + 2 + smallKnobSize + knobTextBoxHeight;
            const int row2Y = row1Bottom + rowGap;

            juce::Slider* row2Knobs[row2Count] = {&grooveSlider_, &busySlider_, &wanderSlider_};
            juce::Label* row2Labels[row2Count] = {&grooveLabel_, &busyLabel_, &wanderLabel_};
            for (int i = 0; i < row2Count; ++i) {
                const int columnX = rightX + i * colWidth;
                const int knobX = columnX + (colWidth - smallKnobSize) / 2;
                row2Labels[i]->setBounds(columnX, row2Y, colWidth, knobLabelHeight);
                row2Knobs[i]->setBounds(knobX, row2Y + knobLabelHeight + 2, smallKnobSize,
                                        smallKnobSize + knobTextBoxHeight);
            }

            const int row2Bottom = row2Y + knobLabelHeight + 2 + smallKnobSize + knobTextBoxHeight;
            dividerY_ = row2Bottom + 10;

            const int evolutionLabelY = row2Bottom + 16;
            evolutionSectionLabel_.setBounds(padding, evolutionLabelY, 100, 14);

            const int toggleRowY = evolutionLabelY + 18;
            const int toggleColumnWidth = contentWidth / kEvolutionToggleCount;

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
            if (handleBaseButtonClicked(button)) {
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
            } else if (button == &attackEvoToggle_) {
                const bool on = attackEvoToggle_.getToggleState();
                evolutionEngineRef_.setAttackEnabled(on);
                if (on) evolutionEngineRef_.resyncAttack(voiceRef_.getAttack());
            } else if (button == &dissonanceEvoToggle_) {
                const bool on = dissonanceEvoToggle_.getToggleState();
                evolutionEngineRef_.setDissonanceEnabled(on);
                if (on) evolutionEngineRef_.resyncDissonance(voiceRef_.getDissonance());
            } else if (button == &grooveEvoToggle_) {
                const bool on = grooveEvoToggle_.getToggleState();
                evolutionEngineRef_.setGrooveEnabled(on);
                if (on) evolutionEngineRef_.resyncGroove(voiceRef_.getGroove());
            } else if (button == &busyEvoToggle_) {
                const bool on = busyEvoToggle_.getToggleState();
                evolutionEngineRef_.setBusyEnabled(on);
                if (on) evolutionEngineRef_.resyncBusy(voiceRef_.getBusy());
            } else if (button == &wanderEvoToggle_) {
                const bool on = wanderEvoToggle_.getToggleState();
                evolutionEngineRef_.setWanderEnabled(on);
                if (on) evolutionEngineRef_.resyncWander(voiceRef_.getWander());
            } else if (button == &sustainEvoToggle_) {
                const bool on = sustainEvoToggle_.getToggleState();
                evolutionEngineRef_.setSustainEnabled(on);
                if (on) evolutionEngineRef_.resyncSustain(voiceRef_.getSustain());
            }
        }

        void sliderValueChanged(juce::Slider* slider) override {
            if (handleBaseSliderChanged(slider)) {
                return;
            }
            if (slider == &volumeSlider_) {
                const float value = static_cast<float>(volumeSlider_.getValue());
                voiceRef_.setVolume(value);
                evolutionEngineRef_.resyncVolume(value);
            } else if (slider == &timbreSlider_) {
                const float value = static_cast<float>(timbreSlider_.getValue());
                voiceRef_.setTimbre(value);
                evolutionEngineRef_.resyncTimbre(value);
            } else if (slider == &attackSlider_) {
                const float value = static_cast<float>(attackSlider_.getValue());
                voiceRef_.setAttack(value);
                evolutionEngineRef_.resyncAttack(value);
            } else if (slider == &dissonanceSlider_) {
                const float value = static_cast<float>(dissonanceSlider_.getValue());
                voiceRef_.setDissonance(value);
                evolutionEngineRef_.resyncDissonance(value);
            } else if (slider == &grooveSlider_) {
                const float value = static_cast<float>(grooveSlider_.getValue());
                voiceRef_.setGroove(value);
                evolutionEngineRef_.resyncGroove(value);
            } else if (slider == &busySlider_) {
                const float value = static_cast<float>(busySlider_.getValue());
                voiceRef_.setBusy(value);
                evolutionEngineRef_.resyncBusy(value);
            } else if (slider == &wanderSlider_) {
                const float value = static_cast<float>(wanderSlider_.getValue());
                voiceRef_.setWander(value);
                evolutionEngineRef_.resyncWander(value);
            } else if (slider == &sustainSlider_) {
                const float value = static_cast<float>(sustainSlider_.getValue());
                voiceRef_.setSustain(value);
                evolutionEngineRef_.resyncSustain(value);
            }

            if (owner_ != nullptr) {
                owner_->updateStatusSummary();
            }
        }

        void refreshFromModel() override {
            VoiceRowBase::refreshFromModel();
            if (!volumeSlider_.isMouseButtonDown()) {
                volumeSlider_.setValue(voiceRef_.getVolume(), juce::dontSendNotification);
            }
            if (!timbreSlider_.isMouseButtonDown()) {
                timbreSlider_.setValue(voiceRef_.getTimbre(), juce::dontSendNotification);
            }
            if (!attackSlider_.isMouseButtonDown()) {
                attackSlider_.setValue(voiceRef_.getAttack(), juce::dontSendNotification);
            }
            if (!dissonanceSlider_.isMouseButtonDown()) {
                dissonanceSlider_.setValue(voiceRef_.getDissonance(), juce::dontSendNotification);
            }
            if (!grooveSlider_.isMouseButtonDown()) {
                grooveSlider_.setValue(voiceRef_.getGroove(), juce::dontSendNotification);
            }
            if (!busySlider_.isMouseButtonDown()) {
                busySlider_.setValue(voiceRef_.getBusy(), juce::dontSendNotification);
            }
            if (!wanderSlider_.isMouseButtonDown()) {
                wanderSlider_.setValue(voiceRef_.getWander(), juce::dontSendNotification);
            }
            if (!sustainSlider_.isMouseButtonDown()) {
                sustainSlider_.setValue(voiceRef_.getSustain(), juce::dontSendNotification);
            }
        }

        void resetEvolutionToggles() override {
            for (auto* toggle : evolutionToggles()) {
                toggle->setToggleState(true, juce::dontSendNotification);
            }
            evolutionEngineRef_.setVolumeEnabled(true);
            evolutionEngineRef_.setPitchRangeEnabled(true);
            evolutionEngineRef_.setTimbreEnabled(true);
            evolutionEngineRef_.setAttackEnabled(true);
            evolutionEngineRef_.setDissonanceEnabled(true);
            evolutionEngineRef_.setGrooveEnabled(true);
            evolutionEngineRef_.setBusyEnabled(true);
            evolutionEngineRef_.setWanderEnabled(true);
            evolutionEngineRef_.setSustainEnabled(true);
        }

        void refreshEvolutionToggles() override {
            volumeEvoToggle_.setToggleState(evolutionEngineRef_.isVolumeEnabled(),
                                            juce::dontSendNotification);
            pitchRangeEvoToggle_.setToggleState(evolutionEngineRef_.isPitchRangeEnabled(),
                                                juce::dontSendNotification);
            timbreEvoToggle_.setToggleState(evolutionEngineRef_.isTimbreEnabled(),
                                            juce::dontSendNotification);
            attackEvoToggle_.setToggleState(evolutionEngineRef_.isAttackEnabled(),
                                            juce::dontSendNotification);
            dissonanceEvoToggle_.setToggleState(evolutionEngineRef_.isDissonanceEnabled(),
                                                juce::dontSendNotification);
            grooveEvoToggle_.setToggleState(evolutionEngineRef_.isGrooveEnabled(),
                                            juce::dontSendNotification);
            busyEvoToggle_.setToggleState(evolutionEngineRef_.isBusyEnabled(),
                                          juce::dontSendNotification);
            wanderEvoToggle_.setToggleState(evolutionEngineRef_.isWanderEnabled(),
                                            juce::dontSendNotification);
            sustainEvoToggle_.setToggleState(evolutionEngineRef_.isSustainEnabled(),
                                             juce::dontSendNotification);
        }

    private:
        static constexpr int kEvolutionToggleCount = 9;
        static constexpr const char* kEvolutionCaptions[kEvolutionToggleCount] = {
            "Volume", "Range", "Dirt", "Attack", "Dissonance", "Groove", "Busy", "Wander", "Sustain"};

        std::array<juce::Label*, kEvolutionToggleCount> evolutionCaptionLabels() {
            return {&volumeEvoLabel_,  &pitchRangeEvoLabel_, &timbreEvoLabel_, &attackEvoLabel_,
                    &dissonanceEvoLabel_, &grooveEvoLabel_,  &busyEvoLabel_,   &wanderEvoLabel_,
                    &sustainEvoLabel_};
        }

        std::array<juce::ToggleButton*, kEvolutionToggleCount> evolutionToggles() {
            return {&volumeEvoToggle_,  &pitchRangeEvoToggle_, &timbreEvoToggle_, &attackEvoToggle_,
                    &dissonanceEvoToggle_, &grooveEvoToggle_,  &busyEvoToggle_,   &wanderEvoToggle_,
                    &sustainEvoToggle_};
        }

        juce::Label volumeLabel_, timbreLabel_, attackLabel_, dissonanceLabel_;
        juce::Label grooveLabel_, busyLabel_, wanderLabel_, sustainLabel_;
        juce::Slider volumeSlider_, timbreSlider_, attackSlider_, dissonanceSlider_;
        juce::Slider grooveSlider_, busySlider_, wanderSlider_, sustainSlider_;
        juce::Label evolutionSectionLabel_;
        juce::Label volumeEvoLabel_, pitchRangeEvoLabel_, timbreEvoLabel_, attackEvoLabel_;
        juce::Label dissonanceEvoLabel_, grooveEvoLabel_, busyEvoLabel_, wanderEvoLabel_, sustainEvoLabel_;
        juce::ToggleButton volumeEvoToggle_, pitchRangeEvoToggle_, timbreEvoToggle_, attackEvoToggle_;
        juce::ToggleButton dissonanceEvoToggle_, grooveEvoToggle_, busyEvoToggle_, wanderEvoToggle_,
            sustainEvoToggle_;
    };

    // Ambient's bespoke card: same Volume-alone-left + adaptive-height-grid
    // shape as BassVoiceRow — row 1 (3) Material/Dissonance/Speed, row 2
    // (2, sharing the same column width so both rows stay aligned)
    // Complexity/Cleanliness — plus a matching 7-toggle Evolution row.
    // Material/Speed/Complexity are the same underlying VoiceModel fields
    // (timbre_/groove_/wander_) every voice reuses for its own bespoke
    // meaning — only the label and (for Material) the grain-trigger DSP
    // differ; see Grain::triggerAmbient/GrainCloud::renderAmbientSample.
    class AmbientVoiceRow : public VoiceRowBase {
    public:
        AmbientVoiceRow(VoiceModel& voice, EvolutionEngine& evolutionEngine,
                        JerricanAudioProcessorEditor* owner)
            : VoiceRowBase(voice, evolutionEngine, owner) {
            setUpKnob(volumeSlider_, volumeLabel_, "Volume");
            volumeSlider_.setValue(voiceRef_.getVolume());
            setUpKnob(materialSlider_, materialLabel_, "Material");
            materialSlider_.setValue(voiceRef_.getTimbre());
            setUpKnob(dissonanceSlider_, dissonanceLabel_, "Dissonance");
            dissonanceSlider_.setValue(voiceRef_.getDissonance());

            setUpKnob(speedSlider_, speedLabel_, "Speed");
            speedSlider_.setValue(voiceRef_.getGroove());
            setUpKnob(complexitySlider_, complexityLabel_, "Layers");
            complexitySlider_.setValue(voiceRef_.getWander());
            // Displayed/dragged as "Dirt" — inverted from the underlying
            // cleanliness_ value (0=dirty..1=clean) so turning the knob up
            // adds more dirt, the more intuitive direction for a knob
            // named after the effect it's adding rather than removing.
            setUpKnob(cleanlinessSlider_, cleanlinessLabel_, "Dirt");
            cleanlinessSlider_.setValue(1.0f - voiceRef_.getCleanliness());

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

        void resized() override {
            constexpr int padding = 14;
            const int contentWidth = getWidth() - padding * 2;
            const int knobAreaTop = layoutHeader(padding, contentWidth);

            constexpr int knobLabelHeight = 14;
            constexpr int knobTextBoxHeight = 16;
            constexpr int rowGap = 8;
            constexpr int toggleCaptionHeight = 12;
            constexpr int toggleSize = 16;
            constexpr int footerHeight = 10 + 16 + 18 + toggleCaptionHeight + 2 + toggleSize;

            const int knobAreaHeight =
                std::max(80, getHeight() - knobAreaTop - footerHeight - padding);

            constexpr int leftColumnWidth = 96;
            const int smallKnobSize = std::max(
                32, (knobAreaHeight - rowGap - 2 * (knobLabelHeight + 2 + knobTextBoxHeight)) / 2);
            const int volumeKnobSize = std::max(
                smallKnobSize,
                std::min(84, knobAreaHeight - knobLabelHeight - 2 - knobTextBoxHeight));

            const int volumeBlockHeight = knobLabelHeight + 2 + volumeKnobSize + knobTextBoxHeight;
            const int volumeTop = knobAreaTop + std::max(0, (knobAreaHeight - volumeBlockHeight) / 2);
            volumeLabel_.setBounds(padding, volumeTop, leftColumnWidth, knobLabelHeight);
            volumeSlider_.setBounds(padding + (leftColumnWidth - volumeKnobSize) / 2,
                                    volumeTop + knobLabelHeight + 2, volumeKnobSize,
                                    volumeKnobSize + knobTextBoxHeight);

            const int rightX = padding + leftColumnWidth + 10;
            const int rightWidth = contentWidth - leftColumnWidth - 10;
            constexpr int gridCols = 3;
            const int colWidth = rightWidth / gridCols;

            juce::Slider* row1Knobs[gridCols] = {&materialSlider_, &dissonanceSlider_, &speedSlider_};
            juce::Label* row1Labels[gridCols] = {&materialLabel_, &dissonanceLabel_, &speedLabel_};
            for (int i = 0; i < gridCols; ++i) {
                const int columnX = rightX + i * colWidth;
                const int knobX = columnX + (colWidth - smallKnobSize) / 2;
                row1Labels[i]->setBounds(columnX, knobAreaTop, colWidth, knobLabelHeight);
                row1Knobs[i]->setBounds(knobX, knobAreaTop + knobLabelHeight + 2, smallKnobSize,
                                        smallKnobSize + knobTextBoxHeight);
            }

            const int row1Bottom = knobAreaTop + knobLabelHeight + 2 + smallKnobSize + knobTextBoxHeight;
            const int row2Y = row1Bottom + rowGap;

            // Only 2 controls this row — shares row 1's column width so
            // the two rows stay visually aligned, third column left empty.
            constexpr int row2Count = 2;
            juce::Slider* row2Knobs[row2Count] = {&complexitySlider_, &cleanlinessSlider_};
            juce::Label* row2Labels[row2Count] = {&complexityLabel_, &cleanlinessLabel_};
            for (int i = 0; i < row2Count; ++i) {
                const int columnX = rightX + i * colWidth;
                const int knobX = columnX + (colWidth - smallKnobSize) / 2;
                row2Labels[i]->setBounds(columnX, row2Y, colWidth, knobLabelHeight);
                row2Knobs[i]->setBounds(knobX, row2Y + knobLabelHeight + 2, smallKnobSize,
                                        smallKnobSize + knobTextBoxHeight);
            }

            const int row2Bottom = row2Y + knobLabelHeight + 2 + smallKnobSize + knobTextBoxHeight;
            dividerY_ = row2Bottom + 10;

            const int evolutionLabelY = row2Bottom + 16;
            evolutionSectionLabel_.setBounds(padding, evolutionLabelY, 100, 14);

            const int toggleRowY = evolutionLabelY + 18;
            const int toggleColumnWidth = contentWidth / kEvolutionToggleCount;

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
            if (handleBaseButtonClicked(button)) {
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
            } else if (button == &materialEvoToggle_) {
                const bool on = materialEvoToggle_.getToggleState();
                evolutionEngineRef_.setTimbreEnabled(on);
                if (on) evolutionEngineRef_.resyncTimbre(voiceRef_.getTimbre());
            } else if (button == &dissonanceEvoToggle_) {
                const bool on = dissonanceEvoToggle_.getToggleState();
                evolutionEngineRef_.setDissonanceEnabled(on);
                if (on) evolutionEngineRef_.resyncDissonance(voiceRef_.getDissonance());
            } else if (button == &speedEvoToggle_) {
                const bool on = speedEvoToggle_.getToggleState();
                evolutionEngineRef_.setGrooveEnabled(on);
                if (on) evolutionEngineRef_.resyncGroove(voiceRef_.getGroove());
            } else if (button == &complexityEvoToggle_) {
                const bool on = complexityEvoToggle_.getToggleState();
                evolutionEngineRef_.setWanderEnabled(on);
                if (on) evolutionEngineRef_.resyncWander(voiceRef_.getWander());
            } else if (button == &cleanlinessEvoToggle_) {
                const bool on = cleanlinessEvoToggle_.getToggleState();
                evolutionEngineRef_.setCleanlinessEnabled(on);
                if (on) evolutionEngineRef_.resyncCleanliness(voiceRef_.getCleanliness());
            }
        }

        void sliderValueChanged(juce::Slider* slider) override {
            if (handleBaseSliderChanged(slider)) {
                return;
            }
            if (slider == &volumeSlider_) {
                const float value = static_cast<float>(volumeSlider_.getValue());
                voiceRef_.setVolume(value);
                evolutionEngineRef_.resyncVolume(value);
            } else if (slider == &materialSlider_) {
                const float value = static_cast<float>(materialSlider_.getValue());
                voiceRef_.setTimbre(value);
                evolutionEngineRef_.resyncTimbre(value);
            } else if (slider == &dissonanceSlider_) {
                const float value = static_cast<float>(dissonanceSlider_.getValue());
                voiceRef_.setDissonance(value);
                evolutionEngineRef_.resyncDissonance(value);
            } else if (slider == &speedSlider_) {
                const float value = static_cast<float>(speedSlider_.getValue());
                voiceRef_.setGroove(value);
                evolutionEngineRef_.resyncGroove(value);
            } else if (slider == &complexitySlider_) {
                const float value = static_cast<float>(complexitySlider_.getValue());
                voiceRef_.setWander(value);
                evolutionEngineRef_.resyncWander(value);
            } else if (slider == &cleanlinessSlider_) {
                // Slider shows/drags "Dirt" (inverted) — convert back to
                // the underlying cleanliness_ value before storing.
                const float dirt = static_cast<float>(cleanlinessSlider_.getValue());
                const float value = 1.0f - dirt;
                voiceRef_.setCleanliness(value);
                evolutionEngineRef_.resyncCleanliness(value);
            }

            if (owner_ != nullptr) {
                owner_->updateStatusSummary();
            }
        }

        void refreshFromModel() override {
            VoiceRowBase::refreshFromModel();
            if (!volumeSlider_.isMouseButtonDown()) {
                volumeSlider_.setValue(voiceRef_.getVolume(), juce::dontSendNotification);
            }
            if (!materialSlider_.isMouseButtonDown()) {
                materialSlider_.setValue(voiceRef_.getTimbre(), juce::dontSendNotification);
            }
            if (!dissonanceSlider_.isMouseButtonDown()) {
                dissonanceSlider_.setValue(voiceRef_.getDissonance(), juce::dontSendNotification);
            }
            if (!speedSlider_.isMouseButtonDown()) {
                speedSlider_.setValue(voiceRef_.getGroove(), juce::dontSendNotification);
            }
            if (!complexitySlider_.isMouseButtonDown()) {
                complexitySlider_.setValue(voiceRef_.getWander(), juce::dontSendNotification);
            }
            if (!cleanlinessSlider_.isMouseButtonDown()) {
                cleanlinessSlider_.setValue(1.0f - voiceRef_.getCleanliness(), juce::dontSendNotification);
            }
        }

        void resetEvolutionToggles() override {
            for (auto* toggle : evolutionToggles()) {
                toggle->setToggleState(true, juce::dontSendNotification);
            }
            evolutionEngineRef_.setVolumeEnabled(true);
            evolutionEngineRef_.setPitchRangeEnabled(true);
            evolutionEngineRef_.setTimbreEnabled(true);
            evolutionEngineRef_.setDissonanceEnabled(true);
            evolutionEngineRef_.setGrooveEnabled(true);
            evolutionEngineRef_.setWanderEnabled(true);
            evolutionEngineRef_.setCleanlinessEnabled(true);
        }

        void refreshEvolutionToggles() override {
            volumeEvoToggle_.setToggleState(evolutionEngineRef_.isVolumeEnabled(),
                                            juce::dontSendNotification);
            pitchRangeEvoToggle_.setToggleState(evolutionEngineRef_.isPitchRangeEnabled(),
                                                juce::dontSendNotification);
            materialEvoToggle_.setToggleState(evolutionEngineRef_.isTimbreEnabled(),
                                              juce::dontSendNotification);
            dissonanceEvoToggle_.setToggleState(evolutionEngineRef_.isDissonanceEnabled(),
                                                juce::dontSendNotification);
            speedEvoToggle_.setToggleState(evolutionEngineRef_.isGrooveEnabled(),
                                           juce::dontSendNotification);
            complexityEvoToggle_.setToggleState(evolutionEngineRef_.isWanderEnabled(),
                                                juce::dontSendNotification);
            cleanlinessEvoToggle_.setToggleState(evolutionEngineRef_.isCleanlinessEnabled(),
                                                 juce::dontSendNotification);
        }

    private:
        static constexpr int kEvolutionToggleCount = 7;
        static constexpr const char* kEvolutionCaptions[kEvolutionToggleCount] = {
            "Volume", "Range", "Material", "Dissonance", "Speed", "Layers", "Dirt"};

        std::array<juce::Label*, kEvolutionToggleCount> evolutionCaptionLabels() {
            return {&volumeEvoLabel_,  &pitchRangeEvoLabel_, &materialEvoLabel_,   &dissonanceEvoLabel_,
                    &speedEvoLabel_,   &complexityEvoLabel_, &cleanlinessEvoLabel_};
        }

        std::array<juce::ToggleButton*, kEvolutionToggleCount> evolutionToggles() {
            return {&volumeEvoToggle_,  &pitchRangeEvoToggle_, &materialEvoToggle_,   &dissonanceEvoToggle_,
                    &speedEvoToggle_,   &complexityEvoToggle_, &cleanlinessEvoToggle_};
        }

        juce::Label volumeLabel_, materialLabel_, dissonanceLabel_;
        juce::Label speedLabel_, complexityLabel_, cleanlinessLabel_;
        juce::Slider volumeSlider_, materialSlider_, dissonanceSlider_;
        juce::Slider speedSlider_, complexitySlider_, cleanlinessSlider_;
        juce::Label evolutionSectionLabel_;
        juce::Label volumeEvoLabel_, pitchRangeEvoLabel_, materialEvoLabel_, dissonanceEvoLabel_;
        juce::Label speedEvoLabel_, complexityEvoLabel_, cleanlinessEvoLabel_;
        juce::ToggleButton volumeEvoToggle_, pitchRangeEvoToggle_, materialEvoToggle_, dissonanceEvoToggle_;
        juce::ToggleButton speedEvoToggle_, complexityEvoToggle_, cleanlinessEvoToggle_;
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
            y = layoutSection(perVoiceSectionLabel_, 0, 21, padding, innerContentWidth, y);
            y = layoutSection(voiceSelectSectionLabel_, 21, 25, padding, innerContentWidth, y);
            y = layoutSection(transportSectionLabel_, 25, 29, padding, innerContentWidth, y);
            y = layoutSection(globalSectionLabel_, 29, 36, padding, innerContentWidth, y);
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
                case MidiTarget::VoiceTimbre: return "Timbre / Dirt / Material / Texture / Mode";
                case MidiTarget::VoiceMotion: return "Motion / Groove / Speed / Drift";
                case MidiTarget::VoiceComplexity: return "Complexity / Wander / Layers / Thickness";
                case MidiTarget::VoiceDissonance: return "Dissonance";
                case MidiTarget::VoiceBusy: return "Busy (Bass / Spark)";
                case MidiTarget::VoiceSustain: return "Sustain (Bass / Spark)";
                case MidiTarget::VoiceAttack: return "Attack (Bass) / Fuzz (Haze) / Voicing (Spark)";
                // Raw CC value maps directly to cleanliness_ (0=dirty..
                // 1=clean) here, same as the Scene field — the inverted
                // "Dirt" display/drag direction is a per-voice-card UI
                // convenience only, not part of the MIDI/Scene contract.
                case MidiTarget::VoiceCleanliness: return "Dirt (Ambient / Spark, inverted)";
                case MidiTarget::VoiceEnabledToggle: return "Enabled toggle";
                case MidiTarget::VoicePitchRangeEvoToggle: return "Evolve: Pitch Range";
                case MidiTarget::VoiceVolumeEvoToggle: return "Evolve: Volume";
                case MidiTarget::VoiceTimbreEvoToggle: return "Evolve: Timbre / Dirt / Material / Texture / Mode";
                case MidiTarget::VoiceMotionEvoToggle: return "Evolve: Motion / Groove / Speed / Drift";
                case MidiTarget::VoiceComplexityEvoToggle: return "Evolve: Complexity / Wander / Layers / Thickness";
                case MidiTarget::VoiceDissonanceEvoToggle: return "Evolve: Dissonance";
                case MidiTarget::VoiceBusyEvoToggle: return "Evolve: Busy (Bass / Spark)";
                case MidiTarget::VoiceSustainEvoToggle: return "Evolve: Sustain (Bass / Spark)";
                case MidiTarget::VoiceAttackEvoToggle: return "Evolve: Attack (Bass) / Fuzz (Haze) / Voicing (Spark)";
                case MidiTarget::VoiceCleanlinessEvoToggle: return "Evolve: Dirt (Ambient / Spark)";
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
                case MidiTarget::Tempo: return "Tempo";
                case MidiTarget::Meter: return "Meter";
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
        if (slider == &tempoSlider) {
            processor_.setTempo(static_cast<float>(tempoSlider.getValue()));
        } else if (slider == &evolutionAmountSlider) {
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
        if (!tempoSlider.isMouseButtonDown()) {
            tempoSlider.setValue(processor_.tempo().load(std::memory_order_relaxed),
                                 juce::dontSendNotification);
        }
        refreshMeterBoxSelection(processor_.meterNumeratorDisplay(), processor_.meterDenominatorDisplay());
        beatPulseIndicator_.refresh(processor_.meterNumeratorDisplay(),
                                    processor_.meterDenominatorDisplay(),
                                    processor_.currentSlot16Display());
        refreshTempoSliderEnablement();
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
    juce::Label meterTitleLabel;
    juce::Label meterLabel;
    juce::ComboBox meterBox;
    BeatPulseIndicator beatPulseIndicator_;
    juce::Label tempoLabel;
    juce::Slider tempoSlider;
    std::array<std::unique_ptr<VoiceRowBase>, 4> voiceRows_;
};
