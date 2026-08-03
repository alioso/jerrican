#pragma once

#include <JuceHeader.h>

#include "JerricanTheme.h"

// Hand-painted controls for Jerrican's instrument-panel look: rotary knobs
// with a value arc instead of default JUCE sliders, pill-handled range/
// linear sliders, LED-style toggles, rounded transport buttons, and a
// matching combo box for output device selection.
class JerricanLookAndFeel : public juce::LookAndFeel_V4 {
public:
    JerricanLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, JerricanTheme::background);
        setColour(juce::Slider::textBoxTextColourId, JerricanTheme::textPrimary);
        setColour(juce::Slider::textBoxOutlineColourId, JerricanTheme::panelBorder);
        setColour(juce::Label::textColourId, JerricanTheme::textSecondary);
        setColour(juce::ComboBox::backgroundColourId, JerricanTheme::panel);
        setColour(juce::ComboBox::outlineColourId, JerricanTheme::panelBorder);
        setColour(juce::ComboBox::textColourId, JerricanTheme::textPrimary);
        setColour(juce::PopupMenu::backgroundColourId, JerricanTheme::panel);
        setColour(juce::PopupMenu::textColourId, JerricanTheme::textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, JerricanTheme::accentDeep);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override {
        juce::ignoreUnused(slider);

        const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                     static_cast<float>(width),
                                                     static_cast<float>(height))
                                 .reduced(4.0f);
        const float diameter = std::min(bounds.getWidth(), bounds.getHeight());
        const auto centre = bounds.getCentre();
        const float radius = diameter * 0.5f;
        const float trackRadius = radius - trackThickness * 0.5f;
        const float angle =
            rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Background track (full travel range).
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                                     rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(JerricanTheme::trackOff);
        g.strokePath(backgroundArc,
                     juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));

        // Value arc.
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                                rotaryStartAngle, angle, true);
        g.setColour(JerricanTheme::accent);
        g.strokePath(valueArc,
                     juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));

        // Knob cap.
        const float capRadius = radius - trackThickness - 3.0f;
        juce::ColourGradient capGradient(JerricanTheme::panel.brighter(0.15f), centre.x,
                                          centre.y - capRadius, JerricanTheme::panel.darker(0.3f),
                                          centre.x, centre.y + capRadius, false);
        g.setGradientFill(capGradient);
        g.fillEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f,
                       capRadius * 2.0f);
        g.setColour(JerricanTheme::panelBorder);
        g.drawEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f,
                       capRadius * 2.0f, 1.0f);

        // Pointer.
        juce::Path pointer;
        const float pointerLength = capRadius * 0.78f;
        const float pointerThickness = 3.0f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness,
                                     pointerLength * 0.55f, pointerThickness * 0.5f);
        g.setColour(JerricanTheme::accentDeep);
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override {
        juce::ignoreUnused(slider);

        const float trackY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const auto trackBounds =
            juce::Rectangle<float>(static_cast<float>(x), trackY - trackThickness * 0.5f,
                                    static_cast<float>(width), trackThickness);
        g.setColour(JerricanTheme::trackOff);
        g.fillRoundedRectangle(trackBounds, trackThickness * 0.5f);

        if (style == juce::Slider::SliderStyle::TwoValueHorizontal) {
            const auto filledBounds = juce::Rectangle<float>(
                minSliderPos, trackY - trackThickness * 0.5f, maxSliderPos - minSliderPos,
                trackThickness);
            g.setColour(JerricanTheme::accent);
            g.fillRoundedRectangle(filledBounds, trackThickness * 0.5f);

            drawHandle(g, minSliderPos, trackY);
            drawHandle(g, maxSliderPos, trackY);
        } else {
            const auto filledBounds = juce::Rectangle<float>(
                static_cast<float>(x), trackY - trackThickness * 0.5f,
                sliderPos - static_cast<float>(x), trackThickness);
            g.setColour(JerricanTheme::accent);
            g.fillRoundedRectangle(filledBounds, trackThickness * 0.5f);

            drawHandle(g, sliderPos, trackY);
        }
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override {
        juce::ignoreUnused(shouldDrawButtonAsDown);

        const auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
        const float diameter = std::min(bounds.getWidth(), bounds.getHeight());
        const auto ledBounds = juce::Rectangle<float>(diameter, diameter).withCentre(bounds.getCentre());

        const bool isOn = button.getToggleState();
        g.setColour(isOn ? JerricanTheme::accent
                          : (shouldDrawButtonAsHighlighted ? JerricanTheme::trackOff.brighter(0.2f)
                                                            : JerricanTheme::trackOff));
        g.fillEllipse(ledBounds);
        g.setColour(JerricanTheme::panelBorder);
        g.drawEllipse(ledBounds, 1.0f);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override {
        juce::ignoreUnused(backgroundColour);

        const bool isPrimary = button.getButtonText() == "Play";
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);

        juce::Colour fill = isPrimary ? JerricanTheme::accentDeep : JerricanTheme::panel;
        if (shouldDrawButtonAsDown) {
            fill = fill.darker(0.2f);
        } else if (shouldDrawButtonAsHighlighted) {
            fill = fill.brighter(0.1f);
        }

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(isPrimary ? JerricanTheme::accentDeep : JerricanTheme::panelBorder);
        g.drawRoundedRectangle(bounds, 8.0f, 1.5f);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override {
        return juce::Font(juce::FontOptions(juce::jmin(16.0f, static_cast<float>(buttonHeight) * 0.5f)));
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown, int buttonX,
                      int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override {
        juce::ignoreUnused(isButtonDown, buttonX, buttonY, buttonW, buttonH);

        const auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width),
                                                     static_cast<float>(height))
                                 .reduced(1.0f);
        g.setColour(JerricanTheme::panel);
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(JerricanTheme::panelBorder);
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        const float arrowSize = 5.0f;
        const auto arrowCentre =
            juce::Point<float>(bounds.getRight() - 14.0f, bounds.getCentreY());
        juce::Path arrow;
        arrow.addTriangle(arrowCentre.x - arrowSize, arrowCentre.y - arrowSize * 0.5f,
                          arrowCentre.x + arrowSize, arrowCentre.y - arrowSize * 0.5f, arrowCentre.x,
                          arrowCentre.y + arrowSize * 0.6f);
        g.setColour(JerricanTheme::accent);
        g.fillPath(arrow);
        juce::ignoreUnused(box);
    }

    void drawCallOutBoxBackground(juce::CallOutBox& box, juce::Graphics& g, const juce::Path& path,
                                  juce::Image&) override {
        juce::ignoreUnused(box);
        g.setColour(JerricanTheme::panel);
        g.fillPath(path);
        g.setColour(JerricanTheme::panelBorder);
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }

private:
    static constexpr float trackThickness = 4.0f;

    static void drawHandle(juce::Graphics& g, float centreX, float centreY) {
        constexpr float handleRadius = 7.0f;
        g.setColour(JerricanTheme::accent);
        g.fillEllipse(centreX - handleRadius, centreY - handleRadius, handleRadius * 2.0f,
                      handleRadius * 2.0f);
        g.setColour(JerricanTheme::background);
        g.drawEllipse(centreX - handleRadius, centreY - handleRadius, handleRadius * 2.0f,
                      handleRadius * 2.0f, 1.5f);
    }
};
