#pragma once

#include <JuceHeader.h>

// Single source of truth for the app's palette — matches the amber/orange
// jerrican app icon. Used by JerricanLookAndFeel and directly by Main.cpp
// for anything the LookAndFeel doesn't cover (labels, panel fills).
namespace JerricanTheme {

inline const juce::Colour background{0xff0a0a0cu};
inline const juce::Colour panel{0xff17171bu};
inline const juce::Colour panelBorder{0xff2a2a30u};
inline const juce::Colour accent{0xffff9e42u};
inline const juce::Colour accentDeep{0xffe85d29u};
inline const juce::Colour textPrimary{0xffffffffu};
inline const juce::Colour textSecondary{0xffb7b3adu};
inline const juce::Colour trackOff{0xff35353bu};

// Distinct cool accent for anything Evolution-related (the transport's
// Amount/Speed knobs, each voice's per-parameter Evolution toggles) so
// that mechanic reads as a clearly separate, related group from the warm
// amber used for direct/manual controls everywhere else.
inline const juce::Colour evolutionAccent{0xff5fd8c9u};

}  // namespace JerricanTheme
