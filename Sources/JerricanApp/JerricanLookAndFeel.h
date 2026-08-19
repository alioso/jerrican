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
        const juce::Colour arcColour =
            slider.getName() == "evolutionKnob" ? JerricanTheme::evolutionAccent : JerricanTheme::accent;

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
        g.setColour(arcColour);
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
        g.setColour(arcColour.darker(0.3f));
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
        if (button.getName() == "soloToggle") {
            g.setColour(button.getToggleState() ? JerricanTheme::soloAccent : JerricanTheme::trackOff);
            g.fillRoundedRectangle(bounds, 4.0f);
            g.setColour(JerricanTheme::textPrimary);
            g.setFont(juce::Font(juce::FontOptions(10.0f)).withStyle(juce::Font::bold));
            g.drawText("S", bounds, juce::Justification::centred);
            return;
        }
        const float diameter = std::min(bounds.getWidth(), bounds.getHeight());
        const auto ledBounds = juce::Rectangle<float>(diameter, diameter).withCentre(bounds.getCentre());

        const juce::Colour onColour =
            button.getName() == "evolutionToggle" ? JerricanTheme::evolutionAccent : JerricanTheme::accent;

        const bool isOn = button.getToggleState();
        g.setColour(isOn ? onColour
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

        const bool isPlay = button.getButtonText() == "Play";
        const bool isStop = button.getButtonText() == "Stop";
        const bool isRecord = button.getButtonText() == "Record";
        const bool isActive = button.getToggleState();  // Play, while actually playing
        const bool enabled = button.isEnabled();
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);

        if (isRecord) {
            // Outlined ghost while armed-but-idle (echoes Play's own
            // "outline = not the thing to click right now" language, just
            // in danger red to signal what this control does); solid red
            // once actually recording, same filled-vs-outline shift Stop
            // already uses for "there's something to click here now".
            g.setColour(JerricanTheme::panel);
            g.fillRoundedRectangle(bounds, 8.0f);
            g.setColour(JerricanTheme::dangerDeep);
            g.drawRoundedRectangle(bounds, 8.0f, isActive ? 2.0f : 1.5f);
            if (isActive) {
                g.setColour(JerricanTheme::danger);
                g.fillRoundedRectangle(bounds.reduced(1.5f), 7.0f);
            }
            return;
        }

        if (isPlay && isActive) {
            // Ghost/outline style once running: a solid fill reads as "you
            // can click this", but there's nothing left to do here while
            // playing — an outlined ring keeps the amber identity without
            // implying it's still an actionable button. Stop takes over as
            // the one solid, clickable control.
            g.setColour(JerricanTheme::panel);
            g.fillRoundedRectangle(bounds, 8.0f);
            g.setColour(JerricanTheme::accent);
            g.drawRoundedRectangle(bounds, 8.0f, 2.0f);
            return;
        }

        // Stop grows teeth once there's actually something to stop: red
        // when enabled, flat grey otherwise.
        juce::Colour fill = JerricanTheme::panel;
        juce::Colour outline = JerricanTheme::panelBorder;
        if (isPlay) {
            fill = JerricanTheme::accentDeep;
            outline = JerricanTheme::accentDeep;
        } else if (isStop) {
            fill = enabled ? JerricanTheme::danger : JerricanTheme::panel;
            outline = enabled ? JerricanTheme::dangerDeep : JerricanTheme::panelBorder;
        }

        if (!enabled && !isPlay) {
            fill = JerricanTheme::trackOff;
            outline = JerricanTheme::panelBorder;
        } else if (shouldDrawButtonAsDown) {
            fill = fill.darker(0.2f);
        } else if (shouldDrawButtonAsHighlighted) {
            fill = fill.brighter(0.1f);
        }

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(outline);
        g.drawRoundedRectangle(bounds, 8.0f, 1.5f);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override {
        return juce::Font(juce::FontOptions(juce::jmin(16.0f, static_cast<float>(buttonHeight) * 0.5f)));
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool isMouseOverButton,
                        bool isButtonDown) override {
        const bool isPlay = button.getButtonText() == "Play";
        const bool isStop = button.getButtonText() == "Stop";
        const bool isRecord = button.getButtonText() == "Record";
        if (!isPlay && !isStop && !isRecord) {
            LookAndFeel_V4::drawButtonText(g, button, isMouseOverButton, isButtonDown);
            return;
        }

        const bool isActive = button.getToggleState();
        const bool enabled = button.isEnabled();
        const juce::Colour colour =
            isPlay ? (isActive ? JerricanTheme::accent : JerricanTheme::textPrimary)
            : isRecord ? (isActive ? JerricanTheme::textPrimary : JerricanTheme::danger)
                       : (enabled ? JerricanTheme::textPrimary : JerricanTheme::textSecondary);

        const auto bounds = button.getLocalBounds().toFloat();
        const juce::Font font = getTextButtonFont(button, button.getHeight());
        const juce::String text = button.getButtonText();
        const float textWidth = juce::GlyphArrangement::getStringWidth(font, text);
        constexpr float iconBoxSize = 14.0f;
        constexpr float gap = 8.0f;
        const float groupWidth = iconBoxSize + gap + textWidth;
        const float startX = bounds.getCentreX() - groupWidth * 0.5f;

        const auto iconBounds = juce::Rectangle<float>(startX, bounds.getCentreY() - iconBoxSize * 0.5f,
                                                        iconBoxSize, iconBoxSize);
        if (isPlay) {
            drawPlayGlyph(g, iconBounds, colour, isActive);
        } else if (isRecord) {
            drawRecordGlyph(g, iconBounds, colour, isActive);
        } else {
            drawStopGlyph(g, iconBounds, colour, enabled);
        }

        g.setColour(colour);
        g.setFont(font);
        const auto textBounds = juce::Rectangle<float>(startX + iconBoxSize + gap, bounds.getY(),
                                                        textWidth + 4.0f, bounds.getHeight());
        g.drawText(text, textBounds, juce::Justification::centredLeft, false);
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

    // Filled triangle while stopped (the primary "go" affordance); once
    // playing, swaps to two static bars — reads as "already running" the
    // same way a media player's pause icon does, reinforcing that this is
    // now a status readout rather than something left to click.
    static void drawPlayGlyph(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour,
                              bool active) {
        g.setColour(colour);
        if (!active) {
            juce::Path triangle;
            triangle.addTriangle(bounds.getX(), bounds.getY(), bounds.getX(), bounds.getBottom(),
                                 bounds.getRight(), bounds.getCentreY());
            g.fillPath(triangle);
        } else {
            const float barWidth = bounds.getWidth() * 0.32f;
            g.fillRoundedRectangle(
                juce::Rectangle<float>(bounds.getX(), bounds.getY(), barWidth, bounds.getHeight()),
                1.5f);
            g.fillRoundedRectangle(juce::Rectangle<float>(bounds.getRight() - barWidth, bounds.getY(),
                                                           barWidth, bounds.getHeight()),
                                   1.5f);
        }
    }

    // Filled square while there's something to stop (clickable); an
    // outline-only square once disabled, matching the same filled-vs-
    // outline "clickable vs not" convention used for Play.
    static void drawStopGlyph(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour,
                              bool enabled) {
        const auto square = bounds.reduced(bounds.getWidth() * 0.12f);
        g.setColour(colour);
        if (enabled) {
            g.fillRoundedRectangle(square, 2.0f);
        } else {
            g.drawRoundedRectangle(square, 2.0f, 1.5f);
        }
    }

    // Filled circle while armed-but-idle (the actionable "start" cue,
    // same filled-primary-icon convention Play's triangle uses); once
    // recording, swaps to a filled square — the universal "click to stop
    // this" shape, reusing Stop's own glyph language since that's
    // literally what clicking it now does.
    static void drawRecordGlyph(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour,
                                bool active) {
        g.setColour(colour);
        if (!active) {
            g.fillEllipse(bounds);
        } else {
            const auto square = bounds.reduced(bounds.getWidth() * 0.12f);
            g.fillRoundedRectangle(square, 2.0f);
        }
    }

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
