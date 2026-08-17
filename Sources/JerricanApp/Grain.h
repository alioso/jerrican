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
    // Ambient: symmetric Hann window. trigger()'s path (Spark/Haze) leaves
    // it unfiltered; triggerAmbient() (the Ambient voice) applies its own
    // static Cleanliness-driven lowpass (see triggerAmbient) despite
    // sharing this same Character — the applyFilterSweep dispatch below
    // is keyed off RenderMode, not Character, for that reason. Plucked:
    // fast attack + curved decay plus a bright-to-dark filter sweep — a
    // produced pluck rather than a static tone, for the short-grain
    // "instrument" voices (Bass/Spark).
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
        mode_ = RenderMode::Single;

        beginLife(sampleRate, pitch, durationMs, pan, character);
        filterStartMultiple_ = kDefaultFilterStartMultiple;
        filterEndMultiple_ = kDefaultFilterEndMultiple;
    }

    // Bass-only: a fixed electric-bass-style core tone — a fundamental
    // Sine, a strong second harmonic (one octave up) for the "boom"/body
    // a bare fundamental doesn't have, only a whisper of Saw (this is a
    // rounded/warm core, not a bright one), and a Noise burst shaped as
    // its own fast-decaying attack transient (a pick/finger pluck click,
    // not sustained hiss) — the same core every time, regardless of the
    // `timbre` parameter (displayed on the Bass card as "Dirt" — it's the
    // drive amount into a tanh waveshaper applied to that core, the same
    // idea as a bass driven through a saturation/fuzz pedal, so "Dirt"
    // names what it actually does more directly than "Timbre" did). It's
    // purely a tone-quality control, not a loudness one:
    // loudnessCompensation_ pulls the makeup gain back down as drive
    // rises, since soft-clipping otherwise raises the average level as a
    // side effect and turning the sound "fuller" wasn't the goal (only
    // its character should change). kMinDrive is low enough that 0 is
    // genuinely clean, near-linear pass-through — and see edgeEnvelope in
    // renderSample() for how the core itself also settles toward a
    // cleaner, more sine-dominant tone over a note's sustain, independent
    // of Dirt, so the fundamental character reads as bass rather than a
    // static synth blend even before any drive is added.
    void triggerBlended(double sampleRate, float pitch, float durationMs, float pan, float timbre,
                        float attack, Character character) {
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
        mode_ = RenderMode::BassBlend;

        const float clampedTimbre = std::max(0.0f, std::min(1.0f, timbre));
        driveAmount_ = lerp(kMinDrive, kMaxDrive, clampedTimbre);
        loudnessCompensation_ = lerp(1.0f, kMaxDriveLoudnessCompensation, clampedTimbre);
        // VoiceOscillator's Saw is a naive, non-band-limited sawtooth (a
        // hard discontinuity every cycle) — real, audible aliasing, not
        // just "character". A fixed presence regardless of Dirt meant
        // Dirt=0 could never be genuinely clean no matter what happened
        // downstream (filtering/headroom/enveloping all operate on the
        // signal *after* it's already aliased). Scaling its weight by
        // Dirt directly means Dirt=0 has zero raw sawtooth in the core at
        // all — nothing left to alias — and it only fades in as Dirt
        // rises, which is exactly the character axis it should be.
        sawWeight_ = kCoreSawWeight * clampedTimbre;
        // 0 = a slow, soft swell in ("None" of a hard attack); 1 = an
        // almost-instant, hard pluck — turning the knob up adds more
        // attack, the same "more of the named effect" direction Dirt
        // already uses. lerp is capped inside bassEnvelope so a very slow
        // attack can't eat an entire short/punchy (low Sustain) note.
        const float clampedAttack = std::max(0.0f, std::min(1.0f, attack));
        attackMs_ = lerp(kBassAttackMaxMs, kBassAttackMinMs, clampedAttack);

        beginLife(sampleRate, pitch, durationMs, pan, character);
        // A closing filter sweep paired with a decaying amplitude
        // envelope is a classic analog "boing"/pluck synthesis trick —
        // exactly what was audible as a residual "spring" tail even at
        // Timbre=0, since the sweep range used to be fixed regardless of
        // Timbre. Now it's tied to Timbre too: at 0 both ends collapse to
        // the same flat cutoff (no sweep at all, genuinely static/clean);
        // the full sweep only opens up as Timbre rises, pairing the
        // filter's movement with the same knob that already controls
        // saturation character.
        filterStartMultiple_ = lerp(kBassFilterFlatMultiple, kBassFilterStartMultiple, clampedTimbre);
        filterEndMultiple_ = lerp(kBassFilterFlatMultiple, kBassFilterEndMultiple, clampedTimbre);
    }

    // Ambient-only: an explicit morph across "material" — Glass (pure
    // Sine) -> Wood (Sine blended with a soft Saw layer, warmer) -> Bell
    // (FM, richer/metallic) — rather than trigger()'s random pickWaveform
    // lottery, so turning the knob predictably moves through a
    // recognizable set of textures instead of a coin-flip between
    // unrelated waveforms. Cleanliness blends in filtered Noise
    // continuously across the grain's whole life (not gated to an attack
    // transient the way Bass's pluck-click noise is) — this is a
    // sustained pad, so "dusty" here means an audible tape-hiss-like
    // grain throughout the note, fading to a pristine tone as Cleanliness
    // rises — and also darkens a static lowpass (applyAmbientFilter): a
    // raw, unfiltered Saw/FM layer has a full harsh harmonic series with
    // nothing to soften it, which read as "basic synth" rather than a
    // warm pad. The filter isn't swept (unlike Plucked's — a pad doesn't
    // need a moving brightness curve, just a consistent warm color), and
    // it darkens together with Cleanliness dropping, so the dusty end
    // reads as tape warmth rather than a broken patch.
    void triggerAmbient(double sampleRate, float pitch, float durationMs, float pan, float material,
                        float cleanliness) {
        sineOscillator_.setWaveform(VoiceOscillator::Waveform::Sine);
        sineOscillator_.setSampleRate(sampleRate);
        sineOscillator_.reset();
        sawOscillator_.setWaveform(VoiceOscillator::Waveform::Saw);
        sawOscillator_.setSampleRate(sampleRate);
        sawOscillator_.reset();
        fmOscillator_.setWaveform(VoiceOscillator::Waveform::Fm);
        fmOscillator_.setSampleRate(sampleRate);
        fmOscillator_.reset();
        noiseOscillator_.setWaveform(VoiceOscillator::Waveform::Noise);
        noiseOscillator_.setSampleRate(sampleRate);
        noiseOscillator_.reset();
        mode_ = RenderMode::AmbientBlend;

        const float clampedMaterial = std::max(0.0f, std::min(1.0f, material));
        if (clampedMaterial < 0.5f) {
            const float u = clampedMaterial * 2.0f;
            glassWeight_ = 1.0f - u;
            woodWeight_ = u;
            bellWeight_ = 0.0f;
        } else {
            const float u = (clampedMaterial - 0.5f) * 2.0f;
            glassWeight_ = 0.0f;
            woodWeight_ = 1.0f - u;
            bellWeight_ = u;
        }

        const float clampedCleanliness = std::max(0.0f, std::min(1.0f, cleanliness));
        ambientNoiseWeight_ = lerp(kMaxAmbientNoiseWeight, 0.0f, clampedCleanliness);

        beginLife(sampleRate, pitch, durationMs, pan, Character::Ambient);

        // Computed after beginLife so fundamentalHz_ is already set.
        // Material gets only a small nudge here (its own identity should
        // come from the waveform blend above, not brightness) — giving it
        // too much pull over this same cutoff last round made Material
        // sound like it was doing Dirt's job, and made Dirt itself nearly
        // inaudible underneath. Dirt now owns this range almost entirely,
        // and it's a much wider swing: properly dull/muffled at 1, clearly
        // more open/present at 0.
        const float materialBrightness = lerp(0.92f, 1.12f, clampedMaterial);
        const float cutoffMultiple =
            lerp(kAmbientFilterDarkMultiple, kAmbientFilterBrightMultiple, clampedCleanliness) *
            materialBrightness;
        const float cutoffHz = static_cast<float>(fundamentalHz_) * cutoffMultiple;
        ambientFilterCoefficient_ =
            1.0f - std::exp(-twoPi * cutoffHz / static_cast<float>(sampleRate));
        ambientFilterState_ = 0.0f;
    }

    bool isActive() const { return remainingLifeSamples_ > 0; }

    StereoSample renderSample() {
        if (remainingLifeSamples_ <= 0) {
            return {};
        }

        const float t = 1.0f - static_cast<float>(remainingLifeSamples_) /
                                    static_cast<float>(totalLifeSamples_);

        float sample;
        switch (mode_) {
            case RenderMode::Single:
                sample = oscillator_.renderSample(pitch_);
                break;
            case RenderMode::BassBlend: {
                // Driven directly from fundamentalHz_ rather than through
                // VoiceOscillator's clamped-normalizedPitch interface, so
                // the octave-up harmonic can't get silently clamped back
                // down to the fundamental for notes near the top of the
                // pitch range.
                const double harmonicIncrement = 2.0 * fundamentalHz_ / sampleRate_;
                const float secondHarmonic = static_cast<float>(std::sin(harmonicPhase_ * twoPi));
                harmonicPhase_ += harmonicIncrement;
                harmonicPhase_ -= std::floor(harmonicPhase_);

                const float elapsedMs = t * noteDurationMs_;
                const float noiseEnvelope = std::exp(-elapsedMs / kNoiseDecayMs);

                // A real plucked string's harmonic content decays faster
                // than its fundamental — bright at the pluck, mellowing
                // into a warm, nearly-pure sustain. Holding the harmonic/
                // saw layer at a constant ratio to the fundamental for the
                // note's entire length is what read as "a synth patch,
                // not a bass": it never evolves. edgeEnvelope reproduces
                // that natural brightness-then-mellow arc independently of
                // Dirt/drive — the sustain settles toward a genuinely
                // clean, sine-dominant tone regardless of Dirt's setting,
                // rather than holding a synth-like constant blend forever.
                const float edgeEnvelope =
                    kEdgeSustainFloor + (1.0f - kEdgeSustainFloor) * std::exp(-elapsedMs / kEdgeDecayMs);

                // kCoreHeadroom: the four weights below sum to just under
                // 1.08 in the worst case (attack transient, all four
                // components peaking together) — left unscaled, that
                // occasionally exceeds unity even before any waveshaping,
                // which the final render stage's hard safety clamp then
                // clips. At Dirt=0 the waveshaper itself is already
                // near-linear (driveAmount_ is tiny), so that clamp was
                // the only real source of the residual "saturation" heard
                // even at 0 — this headroom margin removes it at the
                // source instead.
                const float core = kCoreHeadroom *
                                   (kCoreFundamentalWeight * sineOscillator_.renderSample(pitch_) +
                                    kCoreSecondHarmonicWeight * edgeEnvelope * secondHarmonic +
                                    sawWeight_ * edgeEnvelope * sawOscillator_.renderSample(pitch_) +
                                    kCoreNoiseWeight * noiseEnvelope * noiseOscillator_.renderSample(pitch_));
                sample =
                    (std::tanh(core * driveAmount_) / std::tanh(driveAmount_)) * loudnessCompensation_;
                break;
            }
            case RenderMode::AmbientBlend: {
                // Each oscillator's renderSample() must be called exactly
                // once per audio sample (it advances internal phase) —
                // Wood reuses the same sine/saw samples Glass/the blend
                // already computed rather than calling them again.
                const float sineSample = sineOscillator_.renderSample(pitch_);
                const float sawSample = sawOscillator_.renderSample(pitch_);
                const float fmSample = fmOscillator_.renderSample(pitch_);
                const float noiseSample = noiseOscillator_.renderSample(pitch_);
                const float wood = 0.55f * sineSample + 0.45f * sawSample;
                const float raw = glassWeight_ * sineSample + woodWeight_ * wood + bellWeight_ * fmSample +
                                  ambientNoiseWeight_ * noiseSample;
                ambientFilterState_ += (raw - ambientFilterState_) * ambientFilterCoefficient_;
                sample = ambientFilterState_;
                break;
            }
        }
        if (character_ == Character::Plucked) {
            sample = applyFilterSweep(sample, t);
        }

        float envelope;
        if (mode_ == RenderMode::BassBlend) {
            envelope = bassEnvelope(t, noteDurationMs_, attackMs_);
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
    // Bass's core-tone blend (triggerBlended). Fundamental/second-harmonic
    // weights are constant regardless of Dirt — a rounded, warm core even
    // before any saturation is added. kCoreSawWeight is only the *ceiling*
    // of Saw's contribution (scaled by Dirt into sawWeight_, see
    // triggerBlended) — Saw is a naive, non-band-limited oscillator, so
    // it must be genuinely absent at Dirt=0, not just quiet, or its
    // aliasing is still there underneath everything else.
    static constexpr float kCoreFundamentalWeight = 0.60f;
    static constexpr float kCoreSecondHarmonicWeight = 0.32f;
    static constexpr float kCoreSawWeight = 0.06f;
    static constexpr float kCoreNoiseWeight = 0.10f;  // peak, before its own fast decay below
    // How far the harmonic/saw layer decays toward the fundamental over
    // the note's life — see edgeEnvelope. Floor (not all the way to 0):
    // the sustain keeps a little warmth/edge, it doesn't collapse to a
    // bare sine.
    static constexpr float kEdgeSustainFloor = 0.32f;
    static constexpr float kEdgeDecayMs = 180.0f;
    // Keeps the worst-case constructive-peak sum of the four weights
    // above (~1.08) comfortably under unity, so the render stage's hard
    // safety clamp never has to clip a single grain on its own.
    static constexpr float kCoreHeadroom = 0.82f;
    static constexpr float kNoiseDecayMs = 12.0f;      // pluck-click transient, not sustained hiss
    // kMinDrive is small enough that tanh(x*kMinDrive)/tanh(kMinDrive) is
    // within a fraction of a percent of linear across the whole [-1,1]
    // input range — Dirt=0 is genuinely clean, not "saturated but quiet".
    // kMaxDriveLoudnessCompensation counteracts the RMS increase that
    // soft-clipping otherwise produces at high drive (a side effect of
    // compressing the signal toward its ceiling, on top of Saw's own
    // contribution growing alongside Dirt — see sawWeight_), so sweeping
    // Dirt changes character without also nearly doubling perceived
    // volume. 0.6 wasn't nearly enough pullback for how much louder a
    // heavily tanh-compressed, near-square wave reads (both in raw level
    // and in added midrange presence, which ears are most sensitive to)
    // compared to the near-linear signal at Dirt=0.
    static constexpr float kMinDrive = 0.02f;
    static constexpr float kMaxDrive = 9.0f;
    static constexpr float kMaxDriveLoudnessCompensation = 0.35f;
    static constexpr float kBassFilterStartMultiple = 8.0f;
    static constexpr float kBassFilterEndMultiple = 4.0f;
    // Both sweep endpoints collapse to this single cutoff at Timbre=0 —
    // no movement at all, so nothing "boings".
    static constexpr float kBassFilterFlatMultiple = 6.0f;
    // Bass envelope (triggerBlended only — trigger()'s percussiveEnvelope
    // below is untouched, still used by Drone/Spark/Haze): a
    // user-controllable pluck attack (see Attack/attackMs_ in
    // triggerBlended — from an almost-instant hard pluck to a slow soft
    // swell), then one continuous exponential decay across the rest of
    // the note — like a real plucked string simply ringing out, rather
    // than a separate decay/hold/release stack (which read as an odd,
    // artificially chopped-off shape, especially on short/punchy notes
    // where the flat "hold" ate most of the note's length). Since the
    // decay is expressed as a fraction of the note's own duration (not a
    // fixed ms value), Sustain doing its job — lengthening the grain's
    // total duration — automatically stretches this same curve out in
    // real time: short Sustain decays away in a blip, long Sustain rings
    // out slowly, and neither ever gets cut off mid-decay.
    // 1.5ms (a near-instant linear ramp) was abrupt enough to be heard as
    // a click/pop in its own right rather than a hard pluck — 4.5ms is
    // still snappy and percussive without that broadband click character.
    static constexpr float kBassAttackMinMs = 4.5f;   // hard, snappy pluck
    static constexpr float kBassAttackMaxMs = 350.0f;  // soft swell in
    static constexpr float kBassDecayRate = 3.2f;  // exp(-3.2) ~ -28dB by the note's end
    // Ambient's noise floor peak weight at max Dirt (Cleanliness=0) — a
    // continuous texture, not an attack transient. Raised from an earlier,
    // barely-audible 0.12: Dirt needs to be unmistakable on its own, not a
    // subtle undertone easily masked by Material's much larger waveform-
    // blend swing.
    static constexpr float kMaxAmbientNoiseWeight = 0.28f;
    // Static (unswept) lowpass darkness range for Ambient, tied to Dirt —
    // wide enough that max Dirt is properly dull/muffled and min Dirt is
    // clearly more open, so this reads as Dirt's own clear signature
    // rather than a faint side effect.
    static constexpr float kAmbientFilterDarkMultiple = 1.3f;
    static constexpr float kAmbientFilterBrightMultiple = 13.0f;

    static float hannEnvelope(float t) { return 0.5f - 0.5f * std::cos(twoPi * t); }

    static float percussiveEnvelope(float t) {
        if (t < attackFraction) {
            return t / attackFraction;
        }
        const float u = (t - attackFraction) / (1.0f - attackFraction);
        return std::pow(1.0f - u, decayShape);
    }

    static float bassEnvelope(float t, float noteDurationMs, float attackMs) {
        // Capped higher than the old fixed-attack cap (0.25) since a slow
        // Attack setting can now genuinely approach or exceed a short
        // Sustain note's own length — 0.6 still guarantees some audible
        // decay/tail even in that combination, rather than the whole note
        // being consumed by the ramp-in.
        const float attackFrac = std::min(0.6f, attackMs / noteDurationMs);
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

    enum class RenderMode { Single, BassBlend, AmbientBlend };

    VoiceOscillator oscillator_;       // used by trigger()
    VoiceOscillator sineOscillator_;   // used by triggerBlended()/triggerAmbient()
    VoiceOscillator sawOscillator_;    // used by triggerBlended()/triggerAmbient()
    VoiceOscillator noiseOscillator_;  // used by triggerBlended()/triggerAmbient()
    VoiceOscillator fmOscillator_;     // used by triggerAmbient() only
    RenderMode mode_ = RenderMode::Single;
    float driveAmount_ = kMinDrive;
    float sawWeight_ = 0.0f;  // triggerBlended() only — kCoreSawWeight scaled by Dirt, see there
    float loudnessCompensation_ = 1.0f;
    double harmonicPhase_ = 0.0;  // second-harmonic oscillator, used by triggerBlended() only
    float glassWeight_ = 0.0f;         // triggerAmbient() only
    float woodWeight_ = 0.0f;          // triggerAmbient() only
    float bellWeight_ = 0.0f;          // triggerAmbient() only
    float ambientNoiseWeight_ = 0.0f;  // triggerAmbient() only
    float ambientFilterState_ = 0.0f;         // triggerAmbient() only
    float ambientFilterCoefficient_ = 1.0f;   // triggerAmbient() only
    Character character_ = Character::Ambient;
    float pitch_ = 0.5f;
    double sampleRate_ = 44100.0;
    double fundamentalHz_ = 220.0;
    float filterState_ = 0.0f;
    float filterStartMultiple_ = kDefaultFilterStartMultiple;
    float filterEndMultiple_ = kDefaultFilterEndMultiple;
    float noteDurationMs_ = 1.0f;
    float attackMs_ = kBassAttackMinMs;  // triggerBlended() only
    int totalLifeSamples_ = 1;
    int remainingLifeSamples_ = 0;
    float leftGain_ = 0.7071f;
    float rightGain_ = 0.7071f;
};
