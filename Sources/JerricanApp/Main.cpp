#include <JuceHeader.h>

#include <BinaryData.h>

#include <algorithm>
#include <array>

#include "EvolutionEngine.h"
#include "FastRandom.h"
#include "Grain.h"
#include "GrainCloud.h"
#include "JerricanLookAndFeel.h"
#include "JerricanTheme.h"
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
            "own; 0 leaves them alone.\n"
            "Evolution Speed - how fast a change glides in once Amount picks "
            "one - near-instant to almost imperceptibly slow.\n\n"
            "PER-VOICE CONTROLS\n"
            "Enabled - mutes/unmutes the voice.  Volume - overall level.\n"
            "Pitch Range - band grains draw their pitch from.\n"
            "Timbre - blends grain character, smooth to metallic/textured.\n"
            "Motion - how far the sampling point wanders within Pitch Range.\n"
            "Complexity - how dense the grain cloud is.\n"
            "Dissonance - 0 quantizes to a shared consonant scale so voices "
            "harmonize; 1 is fully free/chromatic.\n\n"
            "Each control has its own small Evolution switch (on by default, "
            "teal) - turn one off to keep it under manual control while the "
            "rest keep drifting.\n\n"
            "REVERB\n"
            "Room - a global send amount/space size for the whole mix. "
            "Decay - how long the tail rings out. Both are 0 by default "
            "(no reverb, output unchanged) and never evolve on their own.\n\n"
            "OUTPUT\n"
            "Chooses which audio device the instrument plays through.\n\n" +
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
                        private juce::Timer {
public:
    JerricanEditor()
        : voices_{VoiceModel(kInitialVoices[0].name, kInitialVoices[0].enabled,
                              kInitialVoices[0].volume, kInitialVoices[0].pitchLow,
                              kInitialVoices[0].pitchHigh, kInitialVoices[0].timbre,
                              kInitialVoices[0].motion, kInitialVoices[0].complexity,
                              kInitialVoices[0].dissonance),
                  VoiceModel(kInitialVoices[1].name, kInitialVoices[1].enabled,
                              kInitialVoices[1].volume, kInitialVoices[1].pitchLow,
                              kInitialVoices[1].pitchHigh, kInitialVoices[1].timbre,
                              kInitialVoices[1].motion, kInitialVoices[1].complexity,
                              kInitialVoices[1].dissonance),
                  VoiceModel(kInitialVoices[2].name, kInitialVoices[2].enabled,
                              kInitialVoices[2].volume, kInitialVoices[2].pitchLow,
                              kInitialVoices[2].pitchHigh, kInitialVoices[2].timbre,
                              kInitialVoices[2].motion, kInitialVoices[2].complexity,
                              kInitialVoices[2].dissonance),
                  VoiceModel(kInitialVoices[3].name, kInitialVoices[3].enabled,
                              kInitialVoices[3].volume, kInitialVoices[3].pitchLow,
                              kInitialVoices[3].pitchHigh, kInitialVoices[3].timbre,
                              kInitialVoices[3].motion, kInitialVoices[3].complexity,
                              kInitialVoices[3].dissonance)},
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
        playButton.addListener(this);

        addAndMakeVisible(stopButton);
        stopButton.setButtonText("Stop");
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

        addAndMakeVisible(statusLabel);
        statusLabel.setText("Transport idle", juce::dontSendNotification);
        statusLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
        statusLabel.setColour(juce::Label::textColourId, JerricanTheme::textSecondary);

        for (size_t i = 0; i < voices_.size(); ++i) {
            voiceRows_[i] = std::make_unique<VoiceRow>(voices_[i], evolutionEngines_[i], this);
            addAndMakeVisible(*voiceRows_[i]);
        }

        updateStatus();

        setSize(1180, 900);
        setAudioChannels(0, 2);
        populateOutputDeviceBox();
        outputDeviceBox.onChange = [this] { outputDeviceSelected(); };
        startTimerHz(30);
    }

    ~JerricanEditor() override {
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
        constexpr int evolutionBlockX = 500;
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

        const int statusX = reverbBlockX + reverbBlockWidth + 30;
        statusLabel.setBounds(statusX, bottomY - 24, getWidth() - statusX - 40, 24);

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

    void prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) override {
        for (auto& cloud : grainClouds_) {
            cloud.setSampleRate(sampleRate);
        }
        for (auto& engine : evolutionEngines_) {
            engine.setSampleRate(sampleRate);
        }
        reverb_.setSampleRate(sampleRate);
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

        for (int sample = 0; sample < bufferToFill.numSamples; ++sample) {
            float mixedLeft = 0.0f;
            float mixedRight = 0.0f;

            for (size_t i = 0; i < voices_.size(); ++i) {
                auto& voice = voices_[i];
                auto& cloud = grainClouds_[i];

                // Evolution runs regardless of transport state — the
                // macros keep drifting "on their own" even while stopped,
                // it just isn't audible until Play (matching the transport
                // gate below, not a special case here).
                evolutionEngines_[i].update(voice, evolutionAmount, evolutionSpeed);

                // Already-active grains ring out on their own envelope even
                // after Stop; only new spawning is gated by the transport,
                // so stopping fades gracefully instead of clicking.
                const float complexity = (playing && voice.isEnabled()) ? voice.getComplexity() : 0.0f;

                const auto voiceSample =
                    cloud.renderSample(voice.getPitchRangeLow(), voice.getPitchRangeHigh(),
                                        voice.getTimbre(), voice.getMotion(), complexity,
                                        voice.getVolume(), voice.getDissonance());
                mixedLeft += voiceSample.left;
                mixedRight += voiceSample.right;
            }

            constexpr float headroom = 0.5f;
            left[sample] = mixedLeft * headroom;
            if (right != nullptr) {
                right[sample] = mixedRight * headroom;
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
    // the shared consonant scale out of the box (see HarmonicScale), so
    // nothing is constantly dissonant/spooky unless deliberately opened up.
    static constexpr std::array<InitialVoice, 4> kInitialVoices{
        {{"Pulse", true, 0.65f, 0.40f, 0.65f, 0.40f, 0.45f, 0.35f, 0.15f, 200.0f, 500.0f,
          Grain::Character::Plucked},
         {"Drone", true, 0.60f, 0.05f, 0.20f, 0.15f, 0.10f, 0.12f, 0.15f, 1500.0f, 4000.0f,
          Grain::Character::Ambient},
         {"Spark", true, 0.55f, 0.60f, 0.85f, 0.70f, 0.50f, 0.45f, 0.15f, 150.0f, 400.0f,
          Grain::Character::Plucked},
         {"Haze", true, 0.50f, 0.15f, 0.35f, 0.75f, 0.20f, 0.15f, 0.15f, 2000.0f, 5000.0f,
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
            g.setColour(JerricanTheme::panelBorder);
            g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

            g.setColour(JerricanTheme::panelBorder);
            g.drawHorizontalLine(dividerY_, 14.0f, static_cast<float>(getWidth() - 14));
        }

        void resized() override {
            constexpr int padding = 14;
            const int contentWidth = getWidth() - padding * 2;

            nameLabel_.setBounds(padding, padding, contentWidth - 30, 26);
            enabledButton_.setBounds(getWidth() - padding - 22, padding + 2, 22, 22);

            const int pitchY = padding + 26 + 8;
            pitchRangeLabel_.setBounds(padding, pitchY, 160, 14);
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

    private:
        static constexpr int kEvolutionToggleCount = 6;
        static constexpr const char* kEvolutionCaptions[kEvolutionToggleCount] = {
            "Volume", "Range", "Timbre", "Motion", "Complexity", "Dissonance"};

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
        juce::Label nameLabel_;
        juce::Label volumeLabel_;
        juce::Label pitchRangeLabel_;
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

    void buttonClicked(juce::Button* button) override {
        if (button == &playButton) {
            isPlaying_.store(true, std::memory_order_relaxed);
            statusLabel.setText("Transport running", juce::dontSendNotification);
        } else if (button == &stopButton) {
            // Just halts new grain spawning — existing grains ring out on
            // their own, and every knob stays exactly where it was, so
            // pressing Play again picks up right where you left off.
            isPlaying_.store(false, std::memory_order_relaxed);
            statusLabel.setText("Transport stopped", juce::dontSendNotification);
        } else if (button == &resetButton) {
            // Explicitly snaps every voice back to its starting values —
            // separate from Stop, since that used to happen together and
            // made Stop feel like it was yanking the controls out from
            // under you.
            resetVoicesToInitialState();
            statusLabel.setText("Voices reset to defaults", juce::dontSendNotification);
        } else if (button == &randomizeButton) {
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
        }
    }

    void timerCallback() override {
        if (evolutionAmount_.load(std::memory_order_relaxed) <= 0.0f) {
            return;
        }
        for (auto& row : voiceRows_) {
            row->refreshFromModel();
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
    juce::Label outputLabel;
    juce::ComboBox outputDeviceBox;
    juce::Label statusLabel;
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
    std::atomic<bool> isPlaying_{false};
    std::atomic<float> evolutionAmount_{0.0f};
    std::atomic<float> evolutionSpeed_{0.5f};
    std::atomic<float> reverbRoom_{0.0f};
    std::atomic<float> reverbDecay_{0.0f};
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
        centreWithSize(1180, 900);
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
