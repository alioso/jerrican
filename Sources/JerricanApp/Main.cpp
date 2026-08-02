#include <JuceHeader.h>

#include <algorithm>
#include <array>

#include "GrainCloud.h"
#include "VoiceModel.h"

class JerricanEditor : public juce::AudioAppComponent, private juce::Button::Listener {
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
          grainClouds_{GrainCloud(0x1a2b3c4du), GrainCloud(0x5e6f7081u), GrainCloud(0x92a3b4c5u),
                       GrainCloud(0xd6e7f809u)} {
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
        statusLabel.setBounds(520, getHeight() - 80, getWidth() - 560, 24);

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
    }

    void releaseResources() override {}

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override {
        auto* left = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
        auto* right = bufferToFill.buffer->getNumChannels() > 1
                          ? bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample)
                          : nullptr;

        const bool playing = isPlaying_.load(std::memory_order_relaxed);

        for (int sample = 0; sample < bufferToFill.numSamples; ++sample) {
            float mixedLeft = 0.0f;
            float mixedRight = 0.0f;

            for (size_t i = 0; i < voices_.size(); ++i) {
                auto& voice = voices_[i];
                auto& cloud = grainClouds_[i];

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
    };

    static constexpr std::array<InitialVoice, 4> kInitialVoices{
        {{"Pulse", true, 0.70f, 0.45f, 0.65f, 0.00f, 0.30f, 0.40f},
         {"Drone", true, 0.60f, 0.05f, 0.25f, 0.33f, 0.15f, 0.20f},
         {"Spark", true, 0.55f, 0.60f, 0.95f, 1.00f, 0.70f, 0.75f},
         {"Echo", false, 0.50f, 0.30f, 0.80f, 0.66f, 0.60f, 0.50f}}};

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
        // triggering listener callbacks (used after Stop/Reset).
        void refreshFromModel() {
            enabledButton_.setToggleState(voiceRef_.isEnabled(), juce::dontSendNotification);
            volumeSlider_.setValue(voiceRef_.getVolume(), juce::dontSendNotification);
            pitchRangeSlider_.setMinAndMaxValues(voiceRef_.getPitchRangeLow(),
                                                  voiceRef_.getPitchRangeHigh(),
                                                  juce::dontSendNotification);
            timbreSlider_.setValue(voiceRef_.getTimbre(), juce::dontSendNotification);
            motionSlider_.setValue(voiceRef_.getMotion(), juce::dontSendNotification);
            complexitySlider_.setValue(voiceRef_.getComplexity(), juce::dontSendNotification);
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
            for (auto& cloud : grainClouds_) {
                cloud.rerollDrift();
            }
        }

        updateStatusSummary();
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
    std::atomic<bool> isPlaying_{false};
    std::array<VoiceModel, 4> voices_;
    std::array<GrainCloud, 4> grainClouds_;
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
