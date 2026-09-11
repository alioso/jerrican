#pragma once

#include <array>
#include <cstddef>

// The ~20 scales (plus the original default) the global Mode control picks
// between. Each entry is a 12-TET semitone-offset pattern from the root —
// same convention HarmonicScale/ChordScale already use for the pentatonic
// they were hardcoded to before Mode existed. Scale lengths genuinely vary
// (5, 6, or 7 notes); callers must read `length`, not assume a fixed size.
//
// These are 12-TET approximations, not a claim of ethnomusicological
// precision — several of the traditions named here (Indian raga, most
// Middle Eastern maqam, Ethiopian qenet) use microtonal tunings in their
// authentic form that 12-TET can only approximate. Some traditions also
// converge on literally the same 12-TET pattern under different names
// (e.g. Bhairav raga and the Byzantine/Double Harmonic scale); this table
// picks one representative name per distinct pattern so every entry
// actually sounds different from every other one.
namespace ModeTable {

inline constexpr std::size_t kMaxNotes = 7;

struct ModeDef {
    const char* label;
    std::array<int, kMaxNotes> semitones;  // only the first `length` entries are valid
    int length;
};

// Index 0 (Pentatonic) is the original hardcoded scale every voice used
// before Mode existed — PresetState::mode defaults to 0 specifically so
// every preset saved before this feature existed, and any preset that
// simply omits the field, reproduces today's exact sound with no
// migration needed.
inline constexpr std::array<ModeDef, 21> kModes{{
    {"Pentatonic", {0, 2, 4, 7, 9, 0, 0}, 5},
    {"Ionian (Major)", {0, 2, 4, 5, 7, 9, 11}, 7},
    {"Dorian", {0, 2, 3, 5, 7, 9, 10}, 7},
    {"Phrygian", {0, 1, 3, 5, 7, 8, 10}, 7},
    {"Lydian", {0, 2, 4, 6, 7, 9, 11}, 7},
    {"Mixolydian", {0, 2, 4, 5, 7, 9, 10}, 7},
    {"Aeolian (Natural Minor)", {0, 2, 3, 5, 7, 8, 10}, 7},
    {"Locrian", {0, 1, 3, 5, 6, 8, 10}, 7},
    {"Harmonic Minor", {0, 2, 3, 5, 7, 8, 11}, 7},
    {"Melodic Minor", {0, 2, 3, 5, 7, 9, 11}, 7},
    {"Hungarian Minor", {0, 2, 3, 6, 7, 8, 11}, 7},
    {"Hungarian Major", {0, 3, 4, 6, 7, 9, 10}, 7},
    {"Phrygian Dominant (Middle Eastern)", {0, 1, 4, 5, 7, 8, 10}, 7},
    {"Double Harmonic (Byzantine/Arabic)", {0, 1, 4, 5, 7, 8, 11}, 7},
    {"Persian", {0, 1, 4, 5, 6, 8, 11}, 7},
    {"Neapolitan Minor", {0, 1, 3, 5, 7, 8, 11}, 7},
    {"Hirajoshi (Japanese)", {0, 2, 3, 7, 8, 0, 0}, 5},
    {"In Sen (Japanese)", {0, 1, 5, 7, 8, 0, 0}, 5},
    {"Egyptian (Suspended Pentatonic)", {0, 2, 5, 7, 10, 0, 0}, 5},
    {"Ethiopian (Anchihoye)", {0, 2, 3, 7, 9, 0, 0}, 5},
    {"Whole Tone", {0, 2, 4, 6, 8, 10, 0}, 6},
}};

}  // namespace ModeTable
