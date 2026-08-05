#pragma once

#include <algorithm>
#include <atomic>
#include <string>

// The generative macros (pitch range/timbre/motion/complexity) describe a
// field of possibilities, not exact instantaneous values: GrainCloud reads
// them continuously to decide what each new grain does. All of them are
// read on the real-time audio thread and written from the UI thread, so
// they're atomic (relaxed: each field is independent, no ordering needed
// between them).
class VoiceModel {
public:
    // rootSemitoneOffset (0=A .. 11=G#) transposes this voice's Dissonance
    // quantization target (see HarmonicScale) — unlike every other field
    // here, it's a compositional/identity choice, not a generative macro:
    // never touched by EvolutionEngine or Randomize, only set here and via
    // the per-voice Key control.
    VoiceModel(std::string name, bool enabled, float volume, float pitchRangeLow,
               float pitchRangeHigh, float timbre, float motion, float complexity,
               float dissonance, int rootSemitoneOffset = 0)
        : name_(std::move(name)),
          enabled_(enabled),
          volume_(clamp01(volume)),
          pitchRangeLow_(clamp01(pitchRangeLow)),
          pitchRangeHigh_(clamp01(std::max(pitchRangeLow, pitchRangeHigh))),
          timbre_(clamp01(timbre)),
          motion_(clamp01(motion)),
          complexity_(clamp01(complexity)),
          dissonance_(clamp01(dissonance)),
          rootSemitoneOffset_(rootSemitoneOffset) {}

    const std::string& getName() const { return name_; }
    bool isEnabled() const { return enabled_.load(std::memory_order_relaxed); }
    float getVolume() const { return volume_.load(std::memory_order_relaxed); }
    float getPitchRangeLow() const { return pitchRangeLow_.load(std::memory_order_relaxed); }
    float getPitchRangeHigh() const { return pitchRangeHigh_.load(std::memory_order_relaxed); }
    float getTimbre() const { return timbre_.load(std::memory_order_relaxed); }
    float getMotion() const { return motion_.load(std::memory_order_relaxed); }
    float getComplexity() const { return complexity_.load(std::memory_order_relaxed); }
    float getDissonance() const { return dissonance_.load(std::memory_order_relaxed); }
    int getRootSemitoneOffset() const { return rootSemitoneOffset_.load(std::memory_order_relaxed); }

    void setEnabled(bool enabled) { enabled_.store(enabled, std::memory_order_relaxed); }
    void setVolume(float volume) { volume_.store(clamp01(volume), std::memory_order_relaxed); }

    void setPitchRange(float low, float high) {
        const float clampedLow = clamp01(low);
        const float clampedHigh = clamp01(std::max(low, high));
        pitchRangeLow_.store(clampedLow, std::memory_order_relaxed);
        pitchRangeHigh_.store(clampedHigh, std::memory_order_relaxed);
    }

    void setTimbre(float timbre) { timbre_.store(clamp01(timbre), std::memory_order_relaxed); }
    void setMotion(float motion) { motion_.store(clamp01(motion), std::memory_order_relaxed); }
    void setComplexity(float complexity) {
        complexity_.store(clamp01(complexity), std::memory_order_relaxed);
    }
    void setDissonance(float dissonance) {
        dissonance_.store(clamp01(dissonance), std::memory_order_relaxed);
    }

    void setRootSemitoneOffset(int rootSemitoneOffset) {
        rootSemitoneOffset_.store(rootSemitoneOffset, std::memory_order_relaxed);
    }

    std::string getSummary() const {
        return name_ + " | enabled=" + (isEnabled() ? "true" : "false") +
               " | volume=" + std::to_string(getVolume()) + " | pitchRange=[" +
               std::to_string(getPitchRangeLow()) + ", " + std::to_string(getPitchRangeHigh()) +
               "] | timbre=" + std::to_string(getTimbre()) + " | motion=" +
               std::to_string(getMotion()) + " | complexity=" + std::to_string(getComplexity()) +
               " | dissonance=" + std::to_string(getDissonance());
    }

private:
    static float clamp01(float value) { return std::max(0.0f, std::min(1.0f, value)); }

    std::string name_;
    std::atomic<bool> enabled_;
    std::atomic<float> volume_;
    std::atomic<float> pitchRangeLow_;
    std::atomic<float> pitchRangeHigh_;
    std::atomic<float> timbre_;
    std::atomic<float> motion_;
    std::atomic<float> complexity_;
    std::atomic<float> dissonance_;
    std::atomic<int> rootSemitoneOffset_;
};
