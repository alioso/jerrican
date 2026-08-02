#include <JuceHeader.h>

#include <algorithm>
#include <array>
#include <cstdlib>

#include "VoiceModel.h"
#include "VoiceOscillator.h"

class JerricanEditor : public juce::AudioAppComponent, private juce::Button::Listener {
public:
    JerricanEditor() : voices_{
            VoiceModel("Pulse", "Sine", true, 0.82f, 0.58f),
            VoiceModel("Drone", "Saw", true, 0.74f, 0.35f),
            VoiceModel("Spark", "Noise", true, 0.66f, 0.78f),
            VoiceModel("Echo", "FM", false, 0.54f, 0.48f)},
        oscillators_{VoiceOscillator(voices_[0].getInstrument()),
                     VoiceOscillator(voices_[1].getInstrument()),
                     VoiceOscillator(voices_[2].getInstrument()),
                     VoiceOscillator(voices_[3].getInstrument())} {
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Jerrican", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(juce::FontOptions(28.0f)).withStyle(juce::Font::bold));
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        addAndMakeVisible(subtitleLabel);
        subtitleLabel.setText("A generative instrument built around layered voices, motion, and control surfaces.", juce::dontSendNotification);
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

        setSize(1080, 760);
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

        const int rowHeight = 92;
        const int rowGap = 12;
        const int top = 160;
        const int width = getWidth() - 80;

        for (size_t i = 0; i < voiceRows_.size(); ++i) {
            voiceRows_[i]->setBounds(40, top + static_cast<int>(i) * (rowHeight + rowGap), width, rowHeight);
        }
    }

    void prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) override {
        for (auto& oscillator : oscillators_) {
            oscillator.setSampleRate(sampleRate);
        }
    }

    void releaseResources() override {}

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override {
        auto* left = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
        auto* right = bufferToFill.buffer->getNumChannels() > 1
                          ? bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample)
                          : nullptr;

        for (int sample = 0; sample < bufferToFill.numSamples; ++sample) {
            float mixed = 0.0f;
            for (size_t i = 0; i < voices_.size(); ++i) {
                if (voices_[i].isEnabled()) {
                    mixed += oscillators_[i].renderSample(voices_[i].getPitch()) * voices_[i].getVolume();
                }
            }

            left[sample] = mixed;
            if (right != nullptr) {
                right[sample] = mixed;
            }
        }
    }

private:
    class VoiceRow : public juce::Component, private juce::Button::Listener, private juce::Slider::Listener {
    public:
        VoiceRow(VoiceModel& voice, JerricanEditor* owner) : voiceRef_(voice), owner_(owner) {
            addAndMakeVisible(nameLabel_);
            nameLabel_.setFont(juce::Font(juce::FontOptions(16.0f)).withStyle(juce::Font::bold));
            nameLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
            nameLabel_.setText(voiceRef_.getName(), juce::dontSendNotification);

            addAndMakeVisible(instrumentLabel_);
            instrumentLabel_.setFont(juce::Font(juce::FontOptions(13.0f)));
            instrumentLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            instrumentLabel_.setText(voiceRef_.getInstrument(), juce::dontSendNotification);

            addAndMakeVisible(enabledButton_);
            enabledButton_.setButtonText("On");
            enabledButton_.setToggleState(voiceRef_.isEnabled(), juce::dontSendNotification);
            enabledButton_.addListener(this);

            addAndMakeVisible(volumeSlider_);
            volumeSlider_.setRange(0.0, 1.0);
            volumeSlider_.setValue(voiceRef_.getVolume());
            volumeSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
            volumeSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
            volumeSlider_.addListener(this);

            addAndMakeVisible(pitchSlider_);
            pitchSlider_.setRange(0.0, 1.0);
            pitchSlider_.setValue(voiceRef_.getPitch());
            pitchSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
            pitchSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
            pitchSlider_.addListener(this);

            addAndMakeVisible(volumeLabel_);
            volumeLabel_.setText("Volume", juce::dontSendNotification);
            volumeLabel_.setFont(juce::Font(juce::FontOptions(12.0f)));
            volumeLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

            addAndMakeVisible(pitchLabel_);
            pitchLabel_.setText("Pitch", juce::dontSendNotification);
            pitchLabel_.setFont(juce::Font(juce::FontOptions(12.0f)));
            pitchLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        }

        void resized() override {
            constexpr int left = 0;
            constexpr int top = 0;
            const int controlWidth = std::max(220, getWidth() - 460);
            nameLabel_.setBounds(left, top, 120, 22);
            instrumentLabel_.setBounds(left, top + 24, 120, 18);
            enabledButton_.setBounds(left + 140, top + 8, 60, 24);
            volumeLabel_.setBounds(left + 220, top, 70, 20);
            volumeSlider_.setBounds(left + 220, top + 22, std::min(220, controlWidth), 24);
            pitchLabel_.setBounds(left + 430, top, 70, 20);
            pitchSlider_.setBounds(left + 430, top + 22, std::min(220, controlWidth), 24);
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
            } else if (slider == &pitchSlider_) {
                voiceRef_.setPitch(static_cast<float>(pitchSlider_.getValue()));
            }

            if (owner_ != nullptr) {
                owner_->updateStatus();
            }
        }

    private:
        VoiceModel& voiceRef_;
        JerricanEditor* owner_;
        juce::Label nameLabel_;
        juce::Label instrumentLabel_;
        juce::Label volumeLabel_;
        juce::Label pitchLabel_;
        juce::ToggleButton enabledButton_;
        juce::Slider volumeSlider_;
        juce::Slider pitchSlider_;
    };

    void buttonClicked(juce::Button* button) override {
        if (button == &playButton) {
            statusLabel.setText("Transport running", juce::dontSendNotification);
        } else if (button == &stopButton) {
            for (auto& voice : voices_) {
                voice.setEnabled(false);
            }
            statusLabel.setText("Transport reset", juce::dontSendNotification);
        } else if (button == &randomizeButton) {
            for (auto& voice : voices_) {
                voice.setVolume(0.35f + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 0.55f);
                voice.setPitch(0.2f + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 0.8f);
            }
        }

        updateStatusSummary();
    }

    void updateStatus() {
        updateStatusSummary();
    }

    void updateStatusSummary() {
        int activeVoices = 0;
        float totalVolume = 0.0f;
        for (const auto& voice : voices_) {
            if (voice.isEnabled()) {
                ++activeVoices;
                totalVolume += voice.getVolume();
            }
        }

        const float averageVolume = activeVoices > 0 ? totalVolume / static_cast<float>(activeVoices) : 0.0f;
        statusLabel.setText("Transport ready — " + juce::String(activeVoices) + " active voices / avg volume " + juce::String(averageVolume, 2), juce::dontSendNotification);
    }

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label statusLabel;
    juce::Label voiceHeaderLabel;
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::TextButton randomizeButton;
    std::array<VoiceModel, 4> voices_;
    std::array<VoiceOscillator, 4> oscillators_;
    std::array<std::unique_ptr<VoiceRow>, 4> voiceRows_;
};

class JerricanMainWindow : public juce::DocumentWindow {
public:
    JerricanMainWindow() : juce::DocumentWindow("Jerrican", juce::Colours::black, juce::DocumentWindow::allButtons) {
        setContentOwned(new JerricanEditor(), true);
        setResizable(true, true);
        centreWithSize(900, 620);
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
