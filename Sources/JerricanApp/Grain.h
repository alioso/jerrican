#pragma once

#include <algorithm>
#include <cmath>

#include "VoiceOscillator.h"

// A single grain: a short-lived enveloped burst from one or more
// VoiceOscillators. Lives entirely on the audio thread, re-triggered in
// place by GrainCloud (no allocation). Idle grains (isActive() == false)
// are silent and safe to re-trigger at any time.
class Grain {
public:
    // Ambient: today's symmetric Hann window, unfiltered — smooth, static
    // timbre, right for sustained pads (Drone/Haze). Plucked: fast attack +
    // curved decay plus a bright-to-dark filter sweep — a produced pluck
    // rather than a static tone, for the short-grain "instrument" voices
    // (Bass/Spark).
    enum class Character { Ambient, Plucked };

    struct StereoSample {
        float left = 0.0f;
        float right = 0.0f;
    };

    // pan is 0 (full left) .. 1 (full right), 0.5 = center. Single discrete
    // waveform, picked by the caller — used by every voice except Bass
    // (Drone/Spark/Haze; see GrainCloud::pickWaveform). Filter sweep (if
    // Character::Plucked) uses the fixed default cutoff range.
    void trigger(VoiceOscillator::Waveform waveform, double sampleRate, float pitch,
                 float durationMs, float pan, Character character) {
        oscillator_.setWaveform(waveform);
        oscillator_.setSampleRate(sampleRate);
        oscillator_.reset();
        useBlend_ = false;

        beginLife(sampleRate, pitch, durationMs, pan, character);
        filterStartMultiple_ = kDefaultFilterStartMultiple;
        filterEndMultiple_ = kDefaultFilterEndMultiple;
    }

    // Bass-only: a fixed electric-bass-style core tone — a fundamental
    // Sine, a strong second harmonic (one octave up) for the "boom"/body
    // a bare fundamental doesn't have, only a whisper of Saw (this is a
    // rounded/warm core, not a bright one), and a Noise burst shaped as
    // its own fast-decaying attack transient (a pick/finger pluck click,
    // not sustained hiss) — the same core every time, regardless of
    // Timbre. Timbre no longer crossfades between different oscillators;
    // it's the drive amount into a tanh waveshaper applied to that core,
    // the same idea as a bass driven through a saturation/fuzz pedal —
    // but it's purely a tone-quality control, not a loudness one:
    // loudnessCompensation_ pulls the makeup gain back down as drive
    // rises, since soft-clipping otherwise raises the average level as a
    // side effect and Timbre turning the sound "fuller" wasn't the goal
    // (only its character should change). kMinDrive is low enough that 0
    // is genuinely clean, near-linear pass-through.
    void triggerBlended(double sampleRate, float pitch, float durationMs, float pan, float timbre,
                        Character character) {
        sineOscillator_.setWaveform(VoiceOscillator::Waveform::Sine);
        sineOscillator_.setSampleRate(sampleRate);
        sineOscillator_.reset();
        sawOscillator_.setWaveform(VoiceOscillator::Waveform::Saw);
        sawOscillator_.setSampleRate(sampleRate);
        sawOscillator_.reset();
        noiseOscillator_.setWaveform(VoiceOscillator::Waveform::Noise);
        noiseOscillator_.setSampleRate(sampleRate);
        noiseOscillator_.reset();
        harmonicPhase_ = 0.0;
        useBlend_ = true;

        const float clampedTimbre = std::max(0.0f, std::min(1.0f, timbre));
        driveAmount_ = lerp(kMinDrive, kMaxDrive, clampedTimbre);
        loudnessCompensation_ = lerp(1.0f, kMaxDriveLoudnessCompensation, clampedTimbre);

        beginLife(sampleRate, pitch, durationMs, pan, character);
        filterStartMultiple_ = kBassFilterStartMultiple;
        filterEndMultiple_ = kBassFilterEndMultiple;
    }

    bool isActive() const { return remainingLifeSamples_ > 0; }

    StereoSample renderSample() {
        if (remainingLifeSamples_ <= 0) {
            return {};
        }

        const float t = 1.0f - static_cast<float>(remainingLifeSamples_) /
                                    static_cast<float>(totalLifeSamples_);

        float sample;
        if (useBlend_) {
            // Driven directly from fundamentalHz_ rather than through
            // VoiceOscillator's clamped-normalizedPitch interface, so the
            // octave-up harmonic can't get silently clamped back down to
            // the fundamental for notes near the top of the pitch range.
            const double harmonicIncrement = 2.0 * fundamentalHz_ / sampleRate_;
            const float secondHarmonic = static_cast<float>(std::sin(harmonicPhase_ * twoPi));
            harmonicPhase_ += harmonicIncrement;
            harmonicPhase_ -= std::floor(harmonicPhase_);

            const float elapsedMs = t * noteDurationMs_;
            const float noiseEnvelope = std::exp(-elapsedMs / kNoiseDecayMs);

            const float core = kCoreFundamentalWeight * sineOscillator_.renderSample(pitch_) +
                               kCoreSecondHarmonicWeight * secondHarmonic +
                               kCoreSawWeight * sawOscillator_.renderSample(pitch_) +
                               kCoreNoiseWeight * noiseEnvelope * noiseOscillator_.renderSample(pitch_);
            sample = (std::tanh(core * driveAmount_) / std::tanh(driveAmount_)) * loudnessCompensation_;
        } else {
            sample = oscillator_.renderSample(pitch_);
        }
        if (character_ == Character::Plucked) {
            sample = applyFilterSweep(sample, t);
        }

        float envelope;
        if (useBlend_) {
            envelope = bassEnvelope(t, noteDurationMs_);
        } else if (character_ == Character::Plucked) {
            envelope = percussiveEnvelope(t);
        } else {
            envelope = hannEnvelope(t);
        }
        const float windowed = sample * envelope;
        --remainingLifeSamples_;

        return {windowed * leftGain_, windowed * rightGain_};
    }

private:
    static constexpr float halfPi = 1.5707963267948966f;
    static constexpr float twoPi = 6.283185307179586f;
    static constexpr float attackFraction = 0.15f;
    static constexpr float decayShape = 1.4f;
    static constexpr float filterSweepCurve = 0.5f;
    static constexpr float kDefaultFilterStartMultiple = 6.0f;
    static constexpr float kDefaultFilterEndMultiple = 3.0f;
    // Bass's fixed core-tone blend (triggerBlended) — constant regardless
    // of Timbre, which only drives the waveshaper below. Saw is kept
    // deliberately small: this is meant to read as a rounded, warm core
    // even before any saturation is added, not an already-bright one.
    static constexpr float kCoreFundamentalWeight = 0.60f;
    static constexpr float kCoreSecondHarmonicWeight = 0.32f;
    static constexpr float kCoreSawWeight = 0.06f;
    static constexpr float kCoreNoiseWeight = 0.10f;  // peak, before its own fast decay below
    static constexpr float kNoiseDecayMs = 12.0f;      // pluck-click transient, not sustained hiss
    // kMinDrive is small enough that tanh(x*kMinDrive)/tanh(kMinDrive) is
    // within a fraction of a percent of linear across the whole [-1,1]
    // input range — Timbre=0 is genuinely clean, not "saturated but
    // quiet". kMaxDriveLoudnessCompensation counteracts the RMS increase
    // that soft-clipping otherwise produces at high drive (a side effect
    // of compressing the signal toward its ceiling), so sweeping Timbre
    // changes character without also changing perceived volume.
    static constexpr float kMinDrive = 0.02f;
    static constexpr float kMaxDrive = 9.0f;
    static constexpr float kMaxDriveLoudnessCompensation = 0.6f;
    static constexpr float kBassFilterStartMultiple = 8.0f;
    static constexpr float kBassFilterEndMultiple = 4.0f;
    // Bass envelope (triggerBlended only — trigger()'s percussiveEnvelope
    // below is untouched, still used by Drone/Spark/Haze): fast pluck
    // attack, then one continuous exponential decay across the rest of
    // the note — like a real plucked string simply ringing out, rather
    // than a separate decay/hold/release stack (which read as an odd,
    // artificially chopped-off shape, especially on short/punchy notes
    // where the flat "hold" ate most of the note's length). Since the
    // decay is expressed as a fraction of the note's own duration (not a
    // fixed ms value), Sustain doing its job — lengthening the grain's
    // total duration — automatically stretches this same curve out in
    // real time: short Sustain decays away in a blip, long Sustain rings
    // out slowly, and neither ever gets cut off mid-decay.
    static constexpr float kBassAttackMs = 6.0f;
    static constexpr float kBassDecayRate = 3.2f;  // exp(-3.2) ~ -28dB by the note's end

    static float hannEnvelope(float t) { return 0.5f - 0.5f * std::cos(twoPi * t); }

    static float percussiveEnvelope(float t) {
        if (t < attackFraction) {
            return t / attackFraction;
        }
        const float u = (t - attackFraction) / (1.0f - attackFraction);
        return std::pow(1.0f - u, decayShape);
    }

    static float bassEnvelope(float t, float noteDurationMs) {
        const float attackFrac = std::min(0.25f, kBassAttackMs / noteDurationMs);
        if (t < attackFrac) {
            return t / attackFrac;
        }
        const float u = (t - attackFrac) / std::max(0.0001f, 1.0f - attackFrac);
        return std::exp(-kBassDecayRate * u);
    }

    static float lerp(float a, float b, float t) { return a + (b - a) * t; }

    // Shared setup between trigger()/triggerBlended() — everything except
    // waveform selection and filter cutoff range, which each caller sets
    // itself immediately after calling this.
    void beginLife(double sampleRate, float pitch, float durationMs, float pan,
                   Character character) {
        pitch_ = pitch;
        character_ = character;
        sampleRate_ = sampleRate;
        fundamentalHz_ = VoiceOscillator::frequencyFromNormalizedPitch(pitch);
        filterState_ = 0.0f;

        noteDurationMs_ = std::max(1.0f, durationMs);
        totalLifeSamples_ = std::max(1, static_cast<int>(durationMs * 0.001 * sampleRate));
        remainingLifeSamples_ = totalLifeSamples_;

        const float clampedPan = std::max(0.0f, std::min(1.0f, pan));
        const float angle = clampedPan * halfPi;
        leftGain_ = std::cos(angle);
        rightGain_ = std::sin(angle);
    }

    float applyFilterSweep(float rawSample, float t) {
        const float sweepProgress = std::pow(t, filterSweepCurve);
        const float cutoffMultiple =
            filterStartMultiple_ + (filterEndMultiple_ - filterStartMultiple_) * sweepProgress;
        const float cutoffHz = static_cast<float>(fundamentalHz_) * cutoffMultiple;
        const float filterCoefficient =
            1.0f - std::exp(-twoPi * cutoffHz / static_cast<float>(sampleRate_));
        filterState_ += (rawSample - filterState_) * filterCoefficient;
        return filterState_;
    }

    VoiceOscillator oscillator_;       // used by trigger()
    VoiceOscillator sineOscillator_;   // used by triggerBlended()
    VoiceOscillator sawOscillator_;    // used by triggerBlended()
    VoiceOscillator noiseOscillator_;  // used by triggerBlended()
    bool useBlend_ = false;
    float driveAmount_ = kMinDrive;
    float loudnessCompensation_ = 1.0f;
    double harmonicPhase_ = 0.0;  // second-harmonic oscillator, used by triggerBlended() only
    Character character_ = Character::Ambient;
    float pitch_ = 0.5f;
    double sampleRate_ = 44100.0;
    double fundamentalHz_ = 220.0;
    float filterState_ = 0.0f;
    float filterStartMultiple_ = kDefaultFilterStartMultiple;
    float filterEndMultiple_ = kDefaultFilterEndMultiple;
    float noteDurationMs_ = 1.0f;
    int totalLifeSamples_ = 1;
    int remainingLifeSamples_ = 0;
    float leftGain_ = 0.7071f;
    float rightGain_ = 0.7071f;
};
