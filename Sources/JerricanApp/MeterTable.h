#pragma once

#include <array>
#include <cstddef>

// Meter structure and Bass accent-weight generation for BassGroovePattern's
// metered grid. Ported from Marmite's GrooveProfiles.h (renamed to avoid
// colliding with Jerrican's own "Groove" macro) — the meter table itself
// (kMeters) is identical, only generateProfile's 8-voice drum-kit switch is
// replaced with a single generateBassAccentProfile, since Jerrican has just
// one voice consuming the clock so far.
//
// A weight near 1.0 means "Bass naturally wants to land here"; near 0
// means "rarely, unless Groove pushes the profile's influence down." At
// low Groove the profile fully governs (a legible, steady walking-bass
// rhythm); at high Groove its influence fades toward uniform/unconstrained
// placement. See BassGroovePattern.h.
//
// Rather than hand-authoring one weight table per meter, each meter
// supplies its "pulse grouping" — how its bar divides into natural pulses,
// in 16th-note slots (e.g. 7/8's 2+2+3 eighths = 4+4+6 sixteenths) — and
// generateBassAccentProfile derives the shape from that grouping: strong on
// every pulse group's downbeat (the walking bass "anchors" each pulse),
// with a light syncopated lift just before the next group.
namespace MeterTable {

inline constexpr std::size_t kMaxSlotsPerBar = 24;  // 12/8, the longest supported bar
inline constexpr std::size_t kMaxPulseGroups = 5;   // 5/4's 5 quarter-note pulses
using AccentProfile = std::array<float, kMaxSlotsPerBar>;

struct MeterDef {
    const char* label;
    int numerator;
    int denominator;
    int totalSlots;
    std::array<int, kMaxPulseGroups> groupLengths;  // in 16th-note slots; only first groupCount entries valid
    int groupCount;
};

// slots = numerator * 16 / denominator. Groupings match Marmite's: 4/4 and
// 3/4 split evenly into quarter-note pulses; 5/4 groups as 3+2 quarters
// ("long-short", e.g. Mission Impossible); 6/8 and 12/8 split into
// dotted-quarter pulses; 7/8 groups as 2+2+3 eighths; 9/8 as an even 3+3+3
// compound triple.
inline constexpr std::array<MeterDef, 7> kMeters{{
    {"4/4", 4, 4, 16, {4, 4, 4, 4, 0}, 4},
    {"3/4", 3, 4, 12, {4, 4, 4, 0, 0}, 3},
    {"5/4", 5, 4, 20, {12, 8, 0, 0, 0}, 2},
    {"6/8", 6, 8, 12, {6, 6, 0, 0, 0}, 2},
    {"7/8", 7, 8, 14, {4, 4, 6, 0, 0}, 3},
    {"9/8", 9, 8, 18, {6, 6, 6, 0, 0}, 3},
    {"12/8", 12, 8, 24, {6, 6, 6, 6, 0}, 4},
}};

inline constexpr int kDefaultMeterIndex = 0;  // 4/4

inline int findMeterIndex(int numerator, int denominator) {
    for (std::size_t i = 0; i < kMeters.size(); ++i) {
        if (kMeters[i].numerator == numerator && kMeters[i].denominator == denominator) {
            return static_cast<int>(i);
        }
    }
    return kDefaultMeterIndex;
}

namespace detail {

inline std::array<int, kMaxPulseGroups> groupStarts(const MeterDef& meter) {
    std::array<int, kMaxPulseGroups> starts{};
    int running = 0;
    for (int g = 0; g < meter.groupCount; ++g) {
        starts[static_cast<std::size_t>(g)] = running;
        running += meter.groupLengths[static_cast<std::size_t>(g)];
    }
    return starts;
}

}  // namespace detail

// Only entries [0, meter.totalSlots) of the returned profile are
// meaningful; trailing slots are left at 0 and never read by
// BassGroovePattern.
inline AccentProfile generateBassAccentProfile(const MeterDef& meter) {
    AccentProfile profile{};
    const auto starts = detail::groupStarts(meter);
    const int n = meter.totalSlots;

    for (int s = 0; s < n; ++s) {
        profile[static_cast<std::size_t>(s)] = 0.05f;
    }
    for (int g = 0; g < meter.groupCount; ++g) {
        const int start = starts[static_cast<std::size_t>(g)];
        const int len = meter.groupLengths[static_cast<std::size_t>(g)];
        profile[static_cast<std::size_t>(start)] = 1.0f;
        const int pushSlot = len >= 4 ? start + len - 2 : start + len - 1;
        if (pushSlot < n) {
            profile[static_cast<std::size_t>(pushSlot)] = len >= 4 ? 0.15f : 0.10f;
        }
    }
    return profile;
}

}  // namespace MeterTable
