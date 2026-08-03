#pragma once

#include <algorithm>
#include <cstdint>

#include "FastRandom.h"
#include "VoiceModel.h"

// One per voice. Drives the voice's own macros (Pitch Range center, Timbre,
// Motion, Complexity, Dissonance) to wander on their own over time, scaled
// by two independent global controls: Amount (0 = the macros stay exactly
// where the user set them — update() is a no-op regardless of Speed; 1 =
// new targets are picked often) and Speed (how quickly the current value
// glides toward whichever target was last picked — independent of how
// often a new one is chosen). Uses the same "occasionally pick a new
// random target, smooth toward it" technique as GrainCloud's internal
// drift, just applied to the macro values themselves instead of
// grain-cloud-internal state.
//
// Pitch range *width* is held fixed at whatever resetTo() was given — only
// where the range sits wanders, not how wide it is, so it can't collapse
// or invert.
class EvolutionEngine {
public:
    explicit EvolutionEngine(std::uint32_t seed) : random_(seed) {}

    void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }

    void resetTo(float pitchCenter, float pitchWidth, float timbre, float motion,
                 float complexity, float dissonance) {
        pitchWidth_ = pitchWidth;
        pitchCenterCurrent_ = pitchCenterTarget_ = pitchCenter;
        timbreCurrent_ = timbreTarget_ = timbre;
        motionCurrent_ = motionTarget_ = motion;
        complexityCurrent_ = complexityTarget_ = complexity;
        dissonanceCurrent_ = dissonanceTarget_ = dissonance;
        sampleCounter_ = 0;
    }

    // Called once per sample. Provably inert at amount <= 0 — returns
    // immediately without touching the voice, regardless of speed.
    void update(VoiceModel& voice, float amount, float speed) {
        if (amount <= 0.0f) {
            return;
        }

        const float retargetProbabilityPerSample =
            (amount * retargetRateSpanHz) / static_cast<float>(sampleRate_);
        if (random_.nextFloat01() < retargetProbabilityPerSample) {
            const float halfWidth = pitchWidth_ * 0.5f;
            pitchCenterTarget_ = random_.nextFloatRange(halfWidth, 1.0f - halfWidth);
            timbreTarget_ = random_.nextFloat01();
            motionTarget_ = random_.nextFloat01();
            complexityTarget_ = random_.nextFloat01();
            dissonanceTarget_ = random_.nextFloat01();
        }

        const float smoothing = speed * smoothingCoefficientSpan;
        pitchCenterCurrent_ += (pitchCenterTarget_ - pitchCenterCurrent_) * smoothing;
        timbreCurrent_ += (timbreTarget_ - timbreCurrent_) * smoothing;
        motionCurrent_ += (motionTarget_ - motionCurrent_) * smoothing;
        complexityCurrent_ += (complexityTarget_ - complexityCurrent_) * smoothing;
        dissonanceCurrent_ += (dissonanceTarget_ - dissonanceCurrent_) * smoothing;

        // The smoothed values move every sample, but writing them into the
        // (atomic) VoiceModel every sample is unnecessary churn for
        // something that only needs to move on a musical timescale.
        if (++sampleCounter_ >= writeIntervalSamples) {
            sampleCounter_ = 0;
            const float halfWidth = pitchWidth_ * 0.5f;
            voice.setPitchRange(pitchCenterCurrent_ - halfWidth, pitchCenterCurrent_ + halfWidth);
            voice.setTimbre(timbreCurrent_);
            voice.setMotion(motionCurrent_);
            voice.setComplexity(complexityCurrent_);
            voice.setDissonance(dissonanceCurrent_);
        }
    }

private:
    static constexpr int writeIntervalSamples = 64;
    static constexpr float retargetRateSpanHz = 0.3f;
    static constexpr float smoothingCoefficientSpan = 0.00002f;

    FastRandom random_;
    double sampleRate_ = 44100.0;
    int sampleCounter_ = 0;
    float pitchWidth_ = 0.2f;
    float pitchCenterCurrent_ = 0.5f;
    float pitchCenterTarget_ = 0.5f;
    float timbreCurrent_ = 0.5f;
    float timbreTarget_ = 0.5f;
    float motionCurrent_ = 0.5f;
    float motionTarget_ = 0.5f;
    float complexityCurrent_ = 0.5f;
    float complexityTarget_ = 0.5f;
    float dissonanceCurrent_ = 0.5f;
    float dissonanceTarget_ = 0.5f;
};
