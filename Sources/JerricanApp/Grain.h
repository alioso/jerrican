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
        // already uses. A straight linear lerp between 350ms and 4.5ms
        // spent most of the knob's travel still sounding slow (the
        // midpoint landed around 177ms) — but a full logarithmic mapping
        // overcorrected: since attackFrac's remainder is always spent on
        // the same fixed, fairly fast exponential decay regardless of
        // Attack, pushing every knob position past ~40% down into
        // single-digit-ms attacks left almost the entire note already
        // decaying, reading as quieter and less defined rather than more
        // percussive. A gentle sqrt-shaped curve on the knob position
        // (not on the ms value itself) reaches noticeably snappier times
        // earlier than pure linear without collapsing most of the range
        // into "already decaying". attackFrac is still capped inside
        // bassEnvelope so a very slow attack can't eat an entire short/
        // punchy (low Sustain) note.
        const float clampedAttack = std::max(0.0f, std::min(1.0f, attack));
        const float shapedAttack = std::sqrt(clampedAttack);
        attackMs_ = lerp(kBassAttackMaxMs, kBassAttackMinMs, shapedAttack);

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

    // Haze-only: an explicit morph across "texture" — Glow (mostly Sine,
    // a touch of FM for a bit of synthetic edge even at 0 — Haze's own
    // identity, distinct from Ambient's pure-sine Glass) -> Edge
    // (Saw+FM-heavy, bright/buzzy) — rather than trigger()'s random
    // pickWaveform lottery. That lottery is what made Timbre feel
    // unpredictable and, worse, let Timbre drift into a majority-Noise
    // pick near 1 — an accidental white-noise "ocean" wash nobody asked
    // for. Texture never touches Noise at all: it stays a deliberate,
    // fully tonal/pitched morph across Sine/Saw/FM at every setting, so
    // there's nothing left to accidentally collapse into a wash.
    //
    // `fuzz` is fully decoupled from `texture` — its own dedicated 0..1
    // control, not a threshold gate on the tail of Texture's travel (an
    // earlier design put fuzz only above 80% Texture; that fought the
    // clean Glow<->Edge morph on the same knob and made both harder to
    // dial in independently).
    void triggerHaze(double sampleRate, float pitch, float durationMs, float pan, float texture,
                      float fuzz) {
        sineOscillator_.setWaveform(VoiceOscillator::Waveform::Sine);
        sineOscillator_.setSampleRate(sampleRate);
        sineOscillator_.reset();
        sawOscillator_.setWaveform(VoiceOscillator::Waveform::Saw);
        sawOscillator_.setSampleRate(sampleRate);
        sawOscillator_.reset();
        fmOscillator_.setWaveform(VoiceOscillator::Waveform::Fm);
        fmOscillator_.setSampleRate(sampleRate);
        fmOscillator_.reset();
        hazeUnisonSaw1_.setWaveform(VoiceOscillator::Waveform::Saw);
        hazeUnisonSaw1_.setSampleRate(sampleRate);
        hazeUnisonSaw1_.reset();
        hazeUnisonSaw2_.setWaveform(VoiceOscillator::Waveform::Saw);
        hazeUnisonSaw2_.setSampleRate(sampleRate);
        hazeUnisonSaw2_.reset();
        hazeUnisonSaw3_.setWaveform(VoiceOscillator::Waveform::Saw);
        hazeUnisonSaw3_.setSampleRate(sampleRate);
        hazeUnisonSaw3_.reset();
        hazeUnisonSaw4_.setWaveform(VoiceOscillator::Waveform::Saw);
        hazeUnisonSaw4_.setSampleRate(sampleRate);
        hazeUnisonSaw4_.reset();
        mode_ = RenderMode::HazeBlend;

        const float clampedTexture = std::max(0.0f, std::min(1.0f, texture));
        hazeSineWeight_ = lerp(kHazeGlowSineWeight, 0.0f, clampedTexture);
        hazeFmWeight_ = lerp(kHazeGlowFmWeight, kHazeEdgeFmWeight, clampedTexture);
        hazeSawWeight_ = lerp(0.0f, kHazeEdgeSawWeight, clampedTexture);

        // sqrt-shaped, not linear, so the fuzz reaches most of its
        // intensity well before 100% rather than only arriving right at
        // the very top of the knob's travel.
        const float clampedFuzz = std::max(0.0f, std::min(1.0f, fuzz));
        const float fuzzAmount = std::sqrt(clampedFuzz);
        // Fuzz-only Saw injection, independent of Texture's own
        // hazeSawWeight_/sawOscillator_ entirely (dedicated unison
        // oscillators below — Texture's own tone morph is never touched).
        // At low Texture (the "reed" setting this session confirmed is
        // liked) the pre-clip signal is mostly a near-pure Sine — clipping
        // that just rounds off a smooth wave, since there's barely any
        // harmonic content for the distortion to turn into buzz. This
        // guarantees a genuinely broadband signal reaches the clipper
        // whenever Fuzz is turned up, regardless of Texture.
        hazeFuzzInjectedSawWeight_ = lerp(0.0f, kHazeFuzzInjectedSawWeight, fuzzAmount);
        // Unison: 4 Saw voices detuned around the note's own pitch (in
        // addition to the single-oscillator injection above), all clipped
        // together by the same waveshaper. A single clipped oscillator,
        // however hard-driven, only ever reads as one thin source — "fat"
        // guitar-wall character (the Siamese Dream reference this whole
        // saga has been chasing) comes from *multiple* simultaneously
        // sounding sources hitting a clipper together, generating real
        // intermodulation between them, not from one voice clipped harder.
        // Detune is expressed in normalized-pitch units, which map
        // exponentially to Hz (see VoiceOscillator::frequencyFromNormalizedPitch)
        // — a fixed additive offset here is therefore a fixed proportional
        // (cents) detune everywhere in the pitch range, not a fixed Hz
        // amount that would go sharp/flat inconsistently by register.
        hazeUnisonBlend_ = fuzzAmount;
        hazeDriveAmount_ = lerp(kHazeMinDrive, kHazeMaxDrive, fuzzAmount);
        hazeLoudnessCompensation_ = lerp(1.0f, kHazeMaxDriveLoudnessCompensation, fuzzAmount);
        // Both bounded to the same ±1 ceiling as the tanh stage — these
        // change the *shape* of the clip, not its loudness. hardClipBlend_
        // mixes in a hard-cornered clip: tanh's transition band never
        // truly disappears no matter how hard it's driven (tanh(35) and
        // tanh(60) are both ~1 — past a point, more drive stops changing
        // anything audible), so pushing drive alone plateaus in perceived
        // character. A hard clip has none of that — its corner is a sharp
        // discontinuity regardless of drive, which is what actually reads
        // as buzzy/fuzzy rather than just "warm." asymmetry_ offsets the
        // signal before clipping so the positive and negative halves clip
        // at different points — real transistor/diode fuzz circuits are
        // never perfectly symmetric, and that asymmetry is what generates
        // even-order harmonics (the "buzz" component) on top of tanh's
        // odd-only harmonics.
        hazeHardClipBlend_ = lerp(0.0f, kHazeMaxHardClipBlend, fuzzAmount);
        hazeAsymmetry_ = lerp(0.0f, kHazeMaxAsymmetry, fuzzAmount);
        hazeFuzzFilterState_ = 0.0f;
        hazeCabFilterState1_ = 0.0f;
        hazeCabFilterState2_ = 0.0f;
        // How much the post-clip "cabinet" darkening (see render path)
        // engages — 0 at no fuzz (untouched), full strength at max fuzz.
        hazeCabBlend_ = fuzzAmount;

        beginLife(sampleRate, pitch, durationMs, pan, Character::Ambient);

        // Computed after beginLife so fundamentalHz_ is set. VoiceOscillator's
        // Saw is a naive, non-band-limited oscillator — it already carries
        // aliased (inharmonic, wrapped-around) high-frequency content.
        // Driving that straight into heavy gain and hard tanh clipping
        // massively amplifies the aliasing and folds it into new,
        // dissonant intermodulation garbage — audibly "weird artifacts",
        // not a musical fuzz. Real analog fuzz pedals only ever see a
        // band-limited signal to begin with; this filter — applied
        // *before* the waveshaper, only as the fuzz stage engages —
        // approximates that by taming the ultrasonic-ish content the
        // distortion would otherwise turn into noise, while still
        // leaving real harmonic body for the saturation to work with.
        const float fuzzFilterMultiple =
            lerp(kHazeFuzzFilterOpenMultiple, kHazeFuzzFilterDarkMultiple, fuzzAmount);
        const float fuzzCutoffHz = static_cast<float>(fundamentalHz_) * fuzzFilterMultiple;
        hazeFuzzFilterCoefficient_ =
            1.0f - std::exp(-twoPi * fuzzCutoffHz / static_cast<float>(sampleRate));

        // "Cabinet" lowpass, post-clip — a real distortion pedal is never
        // heard on its own, only through a guitar amp + speaker cabinet,
        // whose frequency response rolls off hard above ~3-4kHz with
        // essentially nothing above ~5kHz. Skipping that (a bare clipper
        // with nothing after it) leaves every harsh upper-harmonic clipping
        // byproduct fully exposed — that's what reads as synthetic/"Tron"
        // rather than a warm, thick, driven-amp character. Unlike the
        // pre-distortion filter above (relative to the fundamental, opens/
        // closes to manage aliasing), this is a *fixed absolute* cutoff —
        // a real speaker's high rolloff doesn't move with the note being
        // played — and cascaded two-pole (two one-pole stages back to
        // back) for a steeper ~12dB/octave slope closer to an actual
        // cabinet than a single 6dB/octave pole.
        const float cabCoefficient =
            1.0f - std::exp(-twoPi * kHazeCabCutoffHz / static_cast<float>(sampleRate));
        hazeCabFilterCoefficient_ = cabCoefficient;
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
            case RenderMode::HazeBlend: {
                const float sineSample = sineOscillator_.renderSample(pitch_);
                const float sawSample = sawOscillator_.renderSample(pitch_);
                const float fmSample = fmOscillator_.renderSample(pitch_);
                // Fuzz-only unison — 4 detuned Saw voices, independent of
                // Texture's own sawOscillator_/hazeSawWeight_. Averaged
                // (not just summed) so the unison's overall contribution
                // stays comparable in level to the single-voice injection
                // it's layered against, letting hazeFuzzInjectedSawWeight_
                // control total amount the same way it did before.
                const float unisonSaw =
                    (hazeUnisonSaw1_.renderSample(pitch_ - kHazeUnisonDetune2) +
                     hazeUnisonSaw2_.renderSample(pitch_ - kHazeUnisonDetune1) +
                     hazeUnisonSaw3_.renderSample(pitch_ + kHazeUnisonDetune1) +
                     hazeUnisonSaw4_.renderSample(pitch_ + kHazeUnisonDetune2)) *
                    0.25f;
                const float injectedSaw =
                    sawSample + (unisonSaw - sawSample) * hazeUnisonBlend_;
                const float raw = hazeSineWeight_ * sineSample + hazeFmWeight_ * fmSample +
                                  hazeSawWeight_ * sawSample +
                                  hazeFuzzInjectedSawWeight_ * injectedSaw;
                // Filtered *before* the waveshaper — see triggerHaze —
                // so the fuzz stage isn't distorting Saw's raw aliased
                // content directly.
                hazeFuzzFilterState_ +=
                    (raw - hazeFuzzFilterState_) * hazeFuzzFilterCoefficient_;
                // asymmetry_ before both clip stages (see triggerHaze) —
                // even-order harmonic buzz. soft = the existing peak-
                // normalized tanh; hard = a fixed-hardness clip whose
                // corner sharpness doesn't soften as drive rises, blended
                // in by hardClipBlend_ for the actual "fuzzy" edge.
                const float biased = hazeFuzzFilterState_ + hazeAsymmetry_;
                const float soft = std::tanh(biased * hazeDriveAmount_) / std::tanh(hazeDriveAmount_);
                const float hard = std::max(-1.0f, std::min(1.0f, biased * kHazeHardClipDrive));
                const float shaped = soft + (hard - soft) * hazeHardClipBlend_;
                // "Cabinet" darkening — see triggerHaze. Cascaded two-pole
                // lowpass at a fixed absolute cutoff, crossfaded in by
                // hazeCabBlend_ so clean (fuzz=0) playing is untouched.
                hazeCabFilterState1_ +=
                    (shaped - hazeCabFilterState1_) * hazeCabFilterCoefficient_;
                hazeCabFilterState2_ +=
                    (hazeCabFilterState1_ - hazeCabFilterState2_) * hazeCabFilterCoefficient_;
                const float cabbed = shaped + (hazeCabFilterState2_ - shaped) * hazeCabBlend_;
                sample = cabbed * hazeLoudnessCompensation_;
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
    // Haze's Texture blend endpoints (triggerHaze) — Glow (0) keeps a
    // little FM for synthetic edge even at its cleanest; Edge (1) is
    // Saw+FM heavy. Sine fades out completely by 1; FM is present at both
    // ends (Haze's constant "a bit synthetic" identity); Saw only exists
    // toward Edge. No Noise anywhere in this blend.
    static constexpr float kHazeGlowSineWeight = 0.75f;
    static constexpr float kHazeGlowFmWeight = 0.25f;
    static constexpr float kHazeEdgeFmWeight = 0.55f;
    static constexpr float kHazeEdgeSawWeight = 0.45f;
    // Haze's fuzz stage — its own dedicated Fuzz control (fully decoupled
    // from Texture's Glow<->Edge morph; an earlier design gated it to
    // Texture's last 20%, which fought the tone morph on the same knob).
    // Ramps from near-clean to a thick, heavily saturated Big-Muff-style
    // drive by 1.0. Drive and loudness compensation are deliberately
    // decoupled: drive controls how aggressively the
    // waveform gets pushed toward tanh's ceiling (the actual distorted
    // *character* — pushed far past Bass's Dirt ceiling of 9.0, this is
    // meant to be properly fuzzed out), while compensation controls how
    // much headroom margin is left for when several such grains overlap.
    // That margin used to matter a lot more here than it did for Bass:
    // heavily saturated, near-square-wave grains at similar pitches sum
    // close to linearly rather than averaging out, so multiple overlapping
    // fuzzed notes could exceed the render stage's hard safety clamp —
    // audible as an unintended "limiter" clipping the mix. That's now
    // handled structurally instead (GrainCloud::renderHazeSample glides
    // toward its normalization target rather than snapping to it every
    // sample, see there), which is what killed the audible "wave drops".
    // Loudness compensation: 0.45 was measured (a standalone probe reading
    // actual rendered samples directly, bypassing the whole plugin) to
    // leave full fuzz's RMS at ~0.24 against clean Texture's ~0.46 — i.e.
    // the fuzzed tone was quieter than clean, despite genuinely squarer
    // waveshape (crest factor measured dropping from ~1.5 to ~1.3, real
    // saturation). That's why cranking drive/curve params never seemed to
    // land: the saturated character was present in the waveform but being
    // turned down afterward, reading as timid rather than nasty. 0.85 is
    // the level that loudness-matches clean rather than either burying it
    // (0.45) or making the whole range louder than clean (0.9, tried and
    // rejected earlier) — same level, more bite.
    // Pushed all the way to the ceiling of this architecture — pure hard
    // clip (hardClipBlend_ at 1.0, zero tanh softening left) at max fuzz,
    // full makeup gain, strong asymmetry. Confirmed (by ear, at this exact
    // setting) as the character that actually landed, once paired with
    // the unison layer below — a single clipped oscillator never got
    // there no matter how hard it was driven; several simultaneously
    // clipped, detuned voices intermodulating is what did it.
    static constexpr float kHazeMinDrive = 0.02f;
    static constexpr float kHazeMaxDrive = 200.0f;
    static constexpr float kHazeMaxDriveLoudnessCompensation = 1.0f;
    static constexpr float kHazeMaxHardClipBlend = 1.0f;
    static constexpr float kHazeMaxAsymmetry = 0.4f;
    static constexpr float kHazeHardClipDrive = 4.0f;
    // Fuzz-only Saw injection (triggerHaze) — guarantees broadband
    // material reaches the clipper even at low Texture.
    static constexpr float kHazeFuzzInjectedSawWeight = 1.0f;
    // Fuzz-only unison detune (triggerHaze/render) — normalized-pitch
    // units, exponential mapping so this is a fixed proportional (cents)
    // spread everywhere. ~0.006/0.015 work out to roughly ±25/±60 cents
    // for the 4 voices — wide enough to genuinely beat/thicken rather than
    // just lightly chorus.
    static constexpr float kHazeUnisonDetune1 = 0.006f;
    static constexpr float kHazeUnisonDetune2 = 0.015f;
    // Pre-distortion filter (triggerHaze) — barely engaged at fuzzAmount=0
    // (20x fundamental is well above anything the fuzz stage's inert
    // range would otherwise touch), closing down to a real lowpass at
    // full fuzz to tame the naive Saw's aliased content before it hits
    // heavy gain and clipping. 3.5x was too dark: it strangled off the
    // buzzy upper-harmonic content that actually reads as an aggressive
    // "Big Muff" character, leaving only a soft, rounded clip even with
    // drive maxed out. 12x still removes the worst ultrasonic aliasing
    // (it closes in well below Nyquist for anything in Haze's playable
    // range) while leaving most of the harmonic buzz intact for the
    // waveshaper to chew on.
    static constexpr float kHazeFuzzFilterOpenMultiple = 20.0f;
    static constexpr float kHazeFuzzFilterDarkMultiple = 12.0f;
    // "Cabinet" post-clip lowpass (triggerHaze/render path) — a real
    // distortion pedal is always heard through a guitar amp + speaker
    // cabinet, whose response rolls off hard above ~3-4kHz. Skipping that
    // leaves harsh clipping byproducts fully exposed, which is what reads
    // as synthetic/digital rather than a warm driven-amp character. (An
    // earlier attempt at post-clip shaping here — a fundamental-relative
    // mid/low scoop — was a real bug: its cutoff sat almost on top of the
    // fundamental itself, so it tracked nearly the whole clipped signal
    // and subtracted most of it back out, gutting the tone rather than
    // shaping it; measured live, crest factor went the *wrong* direction,
    // 1.56 -> 2.02, i.e. less square as fuzz increased. Removed in favor
    // of this fixed-frequency cabinet-style lowpass instead.) 3200Hz is a
    // typical guitar-cab high rolloff point; unlike the pre-distortion
    // filter above this is an absolute frequency, not fundamental-
    // relative — a real speaker's response doesn't move with the note.
    // Left wide open (effectively bypassed) — with the unison layer doing
    // the actual work of making this sound big, darkening it further on
    // top wasn't needed and only softened the result.
    static constexpr float kHazeCabCutoffHz = 18000.0f;

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

    enum class RenderMode { Single, BassBlend, AmbientBlend, HazeBlend };

    VoiceOscillator oscillator_;       // used by trigger()
    VoiceOscillator sineOscillator_;   // used by triggerBlended()/triggerAmbient()
    VoiceOscillator sawOscillator_;    // used by triggerBlended()/triggerAmbient()
    VoiceOscillator noiseOscillator_;  // used by triggerBlended()/triggerAmbient()
    VoiceOscillator fmOscillator_;     // used by triggerAmbient() only
    // Fuzz-only unison Saw layer (triggerHaze) — see kHazeUnisonDetune*.
    // Never touched by Texture's own hazeSawWeight_/sawOscillator_.
    VoiceOscillator hazeUnisonSaw1_;
    VoiceOscillator hazeUnisonSaw2_;
    VoiceOscillator hazeUnisonSaw3_;
    VoiceOscillator hazeUnisonSaw4_;
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
    float hazeSineWeight_ = 0.0f;  // triggerHaze() only
    float hazeFmWeight_ = 0.0f;    // triggerHaze() only
    float hazeSawWeight_ = 0.0f;   // triggerHaze() only
    float hazeFuzzInjectedSawWeight_ = 0.0f;  // triggerHaze() only
    float hazeUnisonBlend_ = 0.0f;            // triggerHaze() only
    float hazeDriveAmount_ = kHazeMinDrive;   // triggerHaze() only
    float hazeLoudnessCompensation_ = 1.0f;   // triggerHaze() only
    float hazeHardClipBlend_ = 0.0f;          // triggerHaze() only
    float hazeAsymmetry_ = 0.0f;              // triggerHaze() only
    float hazeFuzzFilterState_ = 0.0f;        // triggerHaze() only
    float hazeFuzzFilterCoefficient_ = 1.0f;  // triggerHaze() only
    float hazeCabFilterState1_ = 0.0f;        // triggerHaze() only
    float hazeCabFilterState2_ = 0.0f;        // triggerHaze() only
    float hazeCabFilterCoefficient_ = 1.0f;   // triggerHaze() only
    float hazeCabBlend_ = 0.0f;               // triggerHaze() only
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
