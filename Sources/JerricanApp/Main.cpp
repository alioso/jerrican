#include <JuceHeader.h>

#include <algorithm>
#include <array>

#include "EvolutionEngine.h"
#include "GrainCloud.h"
#include "VoiceModel.h"

class JerricanEditor : public juce::AudioAppComponent,
                        private juce::Button::Listener,
                        private juce::Slider::Listener,
                        private juce::Timer {
public:
    JerricanEditor()
        : voices_{VoiceModel(kInitialVoices[0].name, kInitialVoices[0].enabled,
                              kInitialVoices[0].volume, kInitialVoices[0].pitchLow,
                              kInitialVoices[0].pitchHigh, kInitialVoices[0].timbre,
                              kInitialVoices[0].motion, kInitialVoices[0].complexity),
                  VoiceModel(kInitialVoices[1].name, kInitialVoices[1].enabled,
                              kInitialVoices[1].volume, kInitialVoices[1].pitchLow,
                              kInitialVoices[1].pitchHigh, kInitialVoices[1].timbre,
                              kInitialVoices[1].motion, kInitialVoices[1].complexity),
                  VoiceModel(kInitialVoices[2].name, kInitialVoices[2].enabled,
                              kInitialVoices[2].volume, kInitialVoices[2].pitchLow,
                              kInitialVoices[2].pitchHigh, kInitialVoices[2].timbre,
                              kInitialVoices[2].motion, kInitialVoices[2].complexity),
                  VoiceModel(kInitialVoices[3].name, kInitialVoices[3].enabled,
                              kInitialVoices[3].volume, kInitialVoices[3].pitchLow,
                              kInitialVoices[3].pitchHigh, kInitialVoices[3].timbre,
                              kInitialVoices[3].motion, kInitialVoices[3].complexity)},
          grainClouds_{GrainCloud(0x1a2b3c4du, kInitialVoices[0].minGrainDurationMs,
                                   kInitialVoices[0].maxGrainDurationMs),
                       GrainCloud(0x5e6f7081u, kInitialVoices[1].minGrainDurationMs,
                                   kInitialVoices[1].maxGrainDurationMs),
                       GrainCloud(0x92a3b4c5u, kInitialVoices[2].minGrainDurationMs,
                                   kInitialVoices[2].maxGrainDurationMs),
                       GrainCloud(0xd6e7f809u, kInitialVoices[3].minGrainDurationMs,
                                   kInitialVoices[3].maxGrainDurationMs)},
          evolutionEngines_{EvolutionEngine(0x37a1f2c9u), EvolutionEngine(0x6b4d8e12u),
                             EvolutionEngine(0xa9c3f501u), EvolutionEngine(0xe1d47b6au)} {
        for (size_t i = 0; i < voices_.size(); ++i) {
            const auto& initial = kInitialVoices[i];
            const float center = (initial.pitchLow + initial.pitchHigh) * 0.5f;
            const float width = initial.pitchHigh - initial.pitchLow;
            evolutionEngines_[i].resetTo(center, width, initial.timbre, initial.motion,
                                          initial.complexity);
        }

        addAndMakeVisible(titleLabel);
        titleLabel.setText("Jerrican", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(juce::FontOptions(28.0f)).withStyle(juce::Font::bold));
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        addAndMakeVisible(subtitleLabel);
        subtitleLabel.setText(
            "A generative instrument: each voice composes itself from a field of "
            "possibilities and never plays the same way twice.",
            juce::dontSendNotification);
        subtitleLabel.setFont(juce::Font(juce::FontOptions(16.0f)));
        subtitleLabel.setJustificationType(juce::Justification::centred);
        subtitleLabel.setColour(juce::Label::textColourId, juce::Colours::grey);

        addAndMakeVisible(playButton);
        playButton.setButtonText("Play");
        playButton.addListener(this);

        addAndMakeVisible(stopButton);
        stopButton.setButtonText("Stop / Reset");
        stopButton.addListener(this);

        addAndMakeVisible(randomizeButton);
        randomizeButton.setButtonText("Randomize");
        randomizeButton.addListener(this);

        addAndMakeVisible(evolutionLabel);
        evolutionLabel.setText("Evolution", juce::dontSendNotification);
        evolutionLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        evolutionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

        addAndMakeVisible(evolutionSlider);
        evolutionSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        evolutionSlider.setRange(0.0, 1.0);
        evolutionSlider.setValue(0.0);
        evolutionSlider.setNumDecimalPlacesToDisplay(2);
        evolutionSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
        evolutionSlider.addListener(this);

        addAndMakeVisible(statusLabel);
        statusLabel.setText("Transport idle", juce::dontSendNotification);
        statusLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

        addAndMakeVisible(voiceHeaderLabel);
        voiceHeaderLabel.setText("Voice bank", juce::dontSendNotification);
        voiceHeaderLabel.setFont(juce::Font(juce::FontOptions(18.0f)).withStyle(juce::Font::bold));
        voiceHeaderLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        for (size_t i = 0; i < voices_.size(); ++i) {
            voiceRows_[i] = std::make_unique<VoiceRow>(voices_[i], this);
            addAndMakeVisible(*voiceRows_[i]);
        }

        updateStatus();

        setSize(1180, 920);
        setAudioChannels(0, 2);
        startTimerHz(30);
    }

    ~JerricanEditor() override { shutdownAudio(); }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::darkgrey);
        g.drawRect(40, 120, getWidth() - 80, getHeight() - 180, 1);
    }

    void resized() override {
        titleLabel.setBounds(40, 30, getWidth() - 80, 36);
        subtitleLabel.setBounds(40, 70, getWidth() - 80, 24);

        playButton.setBounds(40, getHeight() - 80, 120, 36);
        stopButton.setBounds(180, getHeight() - 80, 140, 36);
        randomizeButton.setBounds(340, getHeight() - 80, 140, 36);

        evolutionLabel.setBounds(500, getHeight() - 96, 160, 14);
        evolutionSlider.setBounds(500, getHeight() - 80, 220, 24);

        statusLabel.setBounds(740, getHeight() - 80, getWidth() - 780, 24);

        voiceHeaderLabel.setBounds(40, 120, 200, 28);

        const int rowHeight = 130;
        const int rowGap = 14;
        const int top = 160;
        const int width = getWidth() - 80;

        for (size_t i = 0; i < voiceRows_.size(); ++i) {
            voiceRows_[i]->setBounds(40, top + static_cast<int>(i) * (rowHeight + rowGap), width,
                                      rowHeight);
        }
    }

    void prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) override {
        for (auto& cloud : grainClouds_) {
            cloud.setSampleRate(sampleRate);
        }
        for (auto& engine : evolutionEngines_) {
            engine.setSampleRate(sampleRate);
        }
    }

    void releaseResources() override {}

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override {
        auto* left = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
        auto* right = bufferToFill.buffer->getNumChannels() > 1
                          ? bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample)
                          : nullptr;

        const bool playing = isPlaying_.load(std::memory_order_relaxed);
        const float evolution = evolution_.load(std::memory_order_relaxed);

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
                evolutionEngines_[i].update(voice, evolution);

                // Already-active grains ring out on their own envelope even
                // after Stop; only new spawning is gated by the transport,
                // so stopping fades gracefully instead of clicking.
                const float complexity = (playing && voice.isEnabled()) ? voice.getComplexity() : 0.0f;

                const auto voiceSample =
                    cloud.renderSample(voice.getPitchRangeLow(), voice.getPitchRangeHigh(),
                                        voice.getTimbre(), voice.getMotion(), complexity,
                                        voice.getVolume());
                mixedLeft += voiceSample.left;
                mixedRight += voiceSample.right;
            }

            constexpr float headroom = 0.5f;
            left[sample] = mixedLeft * headroom;
            if (right != nullptr) {
                right[sample] = mixedRight * headroom;
            }
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
        float minGrainDurationMs;
        float maxGrainDurationMs;
    };

    // Grain duration range is the main lever for a voice's fundamental
    // character (see GrainCloud) — short & sparse reads as pointillistic,
    // long & overlapping reads as a sustained drone. Complexity is tuned
    // per archetype to suit that duration range: expected concurrent grains
    // is roughly complexity * 40/sec * average-duration-seconds, so a
    // Drone's long grains need a much lower Complexity number than a
    // Pulse's short ones to reach a comparable density.
    static constexpr std::array<InitialVoice, 4> kInitialVoices{
        {{"Pulse", true, 0.70f, 0.45f, 0.60f, 0.15f, 0.35f, 0.12f, 20.0f, 70.0f},
         {"Drone", true, 0.60f, 0.05f, 0.20f, 0.15f, 0.10f, 0.12f, 1500.0f, 4000.0f},
         {"Spark", true, 0.55f, 0.65f, 0.95f, 0.85f, 0.60f, 0.85f, 30.0f, 120.0f},
         {"Echo", false, 0.50f, 0.30f, 0.80f, 0.60f, 0.65f, 0.45f, 80.0f, 200.0f}}};

    class VoiceRow : public juce::Component, private juce::Button::Listener, private juce::Slider::Listener {
    public:
        VoiceRow(VoiceModel& voice, JerricanEditor* owner) : voiceRef_(voice), owner_(owner) {
            addAndMakeVisible(nameLabel_);
            nameLabel_.setFont(juce::Font(juce::FontOptions(16.0f)).withStyle(juce::Font::bold));
            nameLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
            nameLabel_.setText(voiceRef_.getName(), juce::dontSendNotification);

            addAndMakeVisible(enabledButton_);
            enabledButton_.setButtonText("On");
            enabledButton_.setToggleState(voiceRef_.isEnabled(), juce::dontSendNotification);
            enabledButton_.addListener(this);

            setUpSlider(volumeSlider_, volumeLabel_, "Volume", juce::Slider::LinearHorizontal);
            volumeSlider_.setRange(0.0, 1.0);
            volumeSlider_.setValue(voiceRef_.getVolume());

            setUpSlider(pitchRangeSlider_, pitchRangeLabel_, "Pitch range",
                        juce::Slider::TwoValueHorizontal);
            pitchRangeSlider_.setRange(0.0, 1.0);
            pitchRangeSlider_.setMinAndMaxValues(voiceRef_.getPitchRangeLow(),
                                                  voiceRef_.getPitchRangeHigh(),
                                                  juce::dontSendNotification);

            setUpSlider(timbreSlider_, timbreLabel_, "Timbre", juce::Slider::LinearHorizontal);
            timbreSlider_.setRange(0.0, 1.0);
            timbreSlider_.setValue(voiceRef_.getTimbre());

            setUpSlider(motionSlider_, motionLabel_, "Motion", juce::Slider::LinearHorizontal);
            motionSlider_.setRange(0.0, 1.0);
            motionSlider_.setValue(voiceRef_.getMotion());

            setUpSlider(complexitySlider_, complexityLabel_, "Complexity",
                        juce::Slider::LinearHorizontal);
            complexitySlider_.setRange(0.0, 1.0);
            complexitySlider_.setValue(voiceRef_.getComplexity());
        }

        void resized() override {
            constexpr int left = 0;
            constexpr int row1 = 0;
            constexpr int row2 = 50;
            const int controlWidth = std::max(140, (getWidth() - 40) / 4 - 20);

            nameLabel_.setBounds(left, row1, 130, 22);
            enabledButton_.setBounds(left + 140, row1 + 2, 60, 24);
            volumeLabel_.setBounds(left + 220, row1, 90, 18);
            volumeSlider_.setBounds(left + 220, row1 + 20, std::min(220, controlWidth * 2), 24);

            pitchRangeLabel_.setBounds(left, row2, 100, 18);
            pitchRangeSlider_.setBounds(left, row2 + 20, controlWidth, 24);

            const int col2 = left + controlWidth + 20;
            timbreLabel_.setBounds(col2, row2, 100, 18);
            timbreSlider_.setBounds(col2, row2 + 20, controlWidth, 24);

            const int col3 = col2 + controlWidth + 20;
            motionLabel_.setBounds(col3, row2, 100, 18);
            motionSlider_.setBounds(col3, row2 + 20, controlWidth, 24);

            const int col4 = col3 + controlWidth + 20;
            complexityLabel_.setBounds(col4, row2, 100, 18);
            complexitySlider_.setBounds(col4, row2 + 20, controlWidth, 24);
        }

        void buttonClicked(juce::Button* button) override {
            if (button == &enabledButton_) {
                voiceRef_.setEnabled(enabledButton_.getToggleState());
                if (owner_ != nullptr) {
                    owner_->updateStatus();
                }
            }
        }

        void sliderValueChanged(juce::Slider* slider) override {
            if (slider == &volumeSlider_) {
                voiceRef_.setVolume(static_cast<float>(volumeSlider_.getValue()));
            } else if (slider == &pitchRangeSlider_) {
                voiceRef_.setPitchRange(static_cast<float>(pitchRangeSlider_.getMinValue()),
                                         static_cast<float>(pitchRangeSlider_.getMaxValue()));
            } else if (slider == &timbreSlider_) {
                voiceRef_.setTimbre(static_cast<float>(timbreSlider_.getValue()));
            } else if (slider == &motionSlider_) {
                voiceRef_.setMotion(static_cast<float>(motionSlider_.getValue()));
            } else if (slider == &complexitySlider_) {
                voiceRef_.setComplexity(static_cast<float>(complexitySlider_.getValue()));
            }

            if (owner_ != nullptr) {
                owner_->updateStatus();
            }
        }

        // Reflects the current model state into the controls, without
        // triggering listener callbacks (used after Stop/Reset and by the
        // Evolution auto-refresh timer). Skips any control the user is
        // currently dragging, so autonomous evolution doesn't fight a live
        // gesture.
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
        }

    private:
        void setUpSlider(juce::Slider& slider, juce::Label& label, const char* labelText,
                          juce::Slider::SliderStyle style) {
            addAndMakeVisible(label);
            label.setText(labelText, juce::dontSendNotification);
            label.setFont(juce::Font(juce::FontOptions(12.0f)));
            label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

            addAndMakeVisible(slider);
            slider.setSliderStyle(style);
            slider.setNumDecimalPlacesToDisplay(2);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
            slider.addListener(this);
        }

        VoiceModel& voiceRef_;
        JerricanEditor* owner_;
        juce::Label nameLabel_;
        juce::Label volumeLabel_;
        juce::Label pitchRangeLabel_;
        juce::Label timbreLabel_;
        juce::Label motionLabel_;
        juce::Label complexityLabel_;
        juce::ToggleButton enabledButton_;
        juce::Slider volumeSlider_;
        juce::Slider pitchRangeSlider_;
        juce::Slider timbreSlider_;
        juce::Slider motionSlider_;
        juce::Slider complexitySlider_;
    };

    void buttonClicked(juce::Button* button) override {
        if (button == &playButton) {
            isPlaying_.store(true, std::memory_order_relaxed);
            statusLabel.setText("Transport running", juce::dontSendNotification);
        } else if (button == &stopButton) {
            isPlaying_.store(false, std::memory_order_relaxed);
            resetVoicesToInitialState();
            statusLabel.setText("Transport stopped — voices reset", juce::dontSendNotification);
        } else if (button == &randomizeButton) {
            for (size_t i = 0; i < voices_.size(); ++i) {
                grainClouds_[i].rerollDrift(voices_[i].getPitchRangeLow(),
                                             voices_[i].getPitchRangeHigh());
            }
        }

        updateStatusSummary();
    }

    void sliderValueChanged(juce::Slider* slider) override {
        if (slider == &evolutionSlider) {
            evolution_.store(static_cast<float>(evolutionSlider.getValue()),
                              std::memory_order_relaxed);
        }
    }

    void timerCallback() override {
        if (evolution_.load(std::memory_order_relaxed) <= 0.0f) {
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
            voiceRows_[i]->refreshFromModel();

            const float center = (initial.pitchLow + initial.pitchHigh) * 0.5f;
            const float width = initial.pitchHigh - initial.pitchLow;
            evolutionEngines_[i].resetTo(center, width, initial.timbre, initial.motion,
                                          initial.complexity);
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

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label statusLabel;
    juce::Label voiceHeaderLabel;
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::TextButton randomizeButton;
    juce::Label evolutionLabel;
    juce::Slider evolutionSlider;
    std::atomic<bool> isPlaying_{false};
    std::atomic<float> evolution_{0.0f};
    std::array<VoiceModel, 4> voices_;
    std::array<GrainCloud, 4> grainClouds_;
    std::array<EvolutionEngine, 4> evolutionEngines_;
    std::array<std::unique_ptr<VoiceRow>, 4> voiceRows_;
};

class JerricanMainWindow : public juce::DocumentWindow {
public:
    JerricanMainWindow() : juce::DocumentWindow("Jerrican", juce::Colours::black, juce::DocumentWindow::allButtons) {
        setContentOwned(new JerricanEditor(), true);
        setResizable(true, true);
        centreWithSize(1180, 920);
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
