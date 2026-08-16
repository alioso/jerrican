#pragma once

#include <algorithm>
#include <atomic>
#include <string>

// The generative macros (pitch range/timbre/groove/wander) describe a
// field of possibilities, not exact instantaneous values: GrainCloud reads
// them continuously to decide what each new grain does. All of them are
// read on the real-time audio thread and written from the UI thread, so
// they're atomic (relaxed: each field is independent, no ordering needed
// between them).
//
// busy_/sustain_ are Bass-only (see JerricanProcessor.h's kInitialVoices) —
// idle/unused for Drone/Spark/Haze, same tier as how Grain::Character and
// GrainCloud's duration-range constants are already per-voice-archetype
// fields that only some voices make active use of.
class VoiceModel {
public:
    // rootSemitoneOffset (0=A .. 11=G#) transposes this voice's Dissonance
    // quantization target (see HarmonicScale) — unlike every other field
    // here, it's a compositional/identity choice, not a generative macro:
    // never touched by EvolutionEngine or Randomize, only set here and via
    // the per-voice Key control.
    VoiceModel(std::string name, bool enabled, float volume, float pitchRangeLow,
               float pitchRangeHigh, float timbre, float groove, float wander,
               float dissonance, int rootSemitoneOffset = 0, float busy = 0.5f,
               float sustain = 0.5f)
        : name_(std::move(name)),
          enabled_(enabled),
          volume_(clamp01(volume)),
          pitchRangeLow_(clamp01(pitchRangeLow)),
          pitchRangeHigh_(clamp01(std::max(pitchRangeLow, pitchRangeHigh))),
          timbre_(clamp01(timbre)),
          groove_(clamp01(groove)),
          wander_(clamp01(wander)),
          dissonance_(clamp01(dissonance)),
          rootSemitoneOffset_(rootSemitoneOffset),
          busy_(clamp01(busy)),
          sustain_(clamp01(sustain)) {}

    const std::string& getName() const { return name_; }
    bool isEnabled() const { return enabled_.load(std::memory_order_relaxed); }
    float getVolume() const { return volume_.load(std::memory_order_relaxed); }
    float getPitchRangeLow() const { return pitchRangeLow_.load(std::memory_order_relaxed); }
    float getPitchRangeHigh() const { return pitchRangeHigh_.load(std::memory_order_relaxed); }
    float getTimbre() const { return timbre_.load(std::memory_order_relaxed); }
    float getGroove() const { return groove_.load(std::memory_order_relaxed); }
    float getWander() const { return wander_.load(std::memory_order_relaxed); }
    float getDissonance() const { return dissonance_.load(std::memory_order_relaxed); }
    int getRootSemitoneOffset() const { return rootSemitoneOffset_.load(std::memory_order_relaxed); }
    float getBusy() const { return busy_.load(std::memory_order_relaxed); }
    float getSustain() const { return sustain_.load(std::memory_order_relaxed); }

    void setEnabled(bool enabled) { enabled_.store(enabled, std::memory_order_relaxed); }
    void setVolume(float volume) { volume_.store(clamp01(volume), std::memory_order_relaxed); }

    void setPitchRange(float low, float high) {
        const float clampedLow = clamp01(low);
        const float clampedHigh = clamp01(std::max(low, high));
        pitchRangeLow_.store(clampedLow, std::memory_order_relaxed);
        pitchRangeHigh_.store(clampedHigh, std::memory_order_relaxed);
    }

    void setTimbre(float timbre) { timbre_.store(clamp01(timbre), std::memory_order_relaxed); }
    void setGroove(float groove) { groove_.store(clamp01(groove), std::memory_order_relaxed); }
    void setWander(float wander) { wander_.store(clamp01(wander), std::memory_order_relaxed); }
    void setDissonance(float dissonance) {
        dissonance_.store(clamp01(dissonance), std::memory_order_relaxed);
    }
    void setBusy(float busy) { busy_.store(clamp01(busy), std::memory_order_relaxed); }
    void setSustain(float sustain) { sustain_.store(clamp01(sustain), std::memory_order_relaxed); }

    void setRootSemitoneOffset(int rootSemitoneOffset) {
        rootSemitoneOffset_.store(rootSemitoneOffset, std::memory_order_relaxed);
    }

    std::string getSummary() const {
        return name_ + " | enabled=" + (isEnabled() ? "true" : "false") +
               " | volume=" + std::to_string(getVolume()) + " | pitchRange=[" +
               std::to_string(getPitchRangeLow()) + ", " + std::to_string(getPitchRangeHigh()) +
               "] | timbre=" + std::to_string(getTimbre()) + " | groove=" +
               std::to_string(getGroove()) + " | wander=" + std::to_string(getWander()) +
               " | dissonance=" + std::to_string(getDissonance()) +
               " | busy=" + std::to_string(getBusy()) +
               " | sustain=" + std::to_string(getSustain());
    }

private:
    static float clamp01(float value) { return std::max(0.0f, std::min(1.0f, value)); }

    std::string name_;
    std::atomic<bool> enabled_;
    std::atomic<float> volume_;
    std::atomic<float> pitchRangeLow_;
    std::atomic<float> pitchRangeHigh_;
    std::atomic<float> timbre_;
    std::atomic<float> groove_;
    std::atomic<float> wander_;
    std::atomic<float> dissonance_;
    std::atomic<int> rootSemitoneOffset_;
    std::atomic<float> busy_;
    std::atomic<float> sustain_;
};
