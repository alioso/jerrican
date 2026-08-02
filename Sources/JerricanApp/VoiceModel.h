#pragma once

#include <algorithm>
#include <atomic>
#include <string>

// enabled_/volume_/pitch_ are read on the real-time audio thread and written
// from the UI thread, so they're atomic (relaxed: each parameter is
// independent, no ordering needed between them).
class VoiceModel {
public:
    VoiceModel(std::string name, std::string instrument, bool enabled, float volume, float pitch)
        : name_(std::move(name)),
          instrument_(std::move(instrument)),
          enabled_(enabled),
          volume_(volume),
          pitch_(pitch) {}

    const std::string& getName() const { return name_; }
    const std::string& getInstrument() const { return instrument_; }
    bool isEnabled() const { return enabled_.load(std::memory_order_relaxed); }
    float getVolume() const { return volume_.load(std::memory_order_relaxed); }
    float getPitch() const { return pitch_.load(std::memory_order_relaxed); }

    void setEnabled(bool enabled) { enabled_.store(enabled, std::memory_order_relaxed); }
    void setVolume(float volume) {
        volume_.store(std::max(0.0f, std::min(1.0f, volume)), std::memory_order_relaxed);
    }
    void setPitch(float pitch) {
        pitch_.store(std::max(0.0f, std::min(1.0f, pitch)), std::memory_order_relaxed);
    }

    std::string getSummary() const {
        return name_ + " | " + instrument_ + " | enabled=" + (isEnabled() ? "true" : "false") +
               " | volume=" + std::to_string(getVolume()) + " | pitch=" + std::to_string(getPitch());
    }

private:
    std::string name_;
    std::string instrument_;
    std::atomic<bool> enabled_;
    std::atomic<float> volume_;
    std::atomic<float> pitch_;
};
