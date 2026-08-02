# Jerrican

Jerrican is a macOS-native generative music instrument, built on C++ and
JUCE. It's not a sequencer or a sample player — it's a granular synthesis
engine that composes itself continuously from a field of possibilities set
by the user, in the spirit of Umberto Eco's *opera aperta* (the "open
work"): the controls shape tendencies and ranges, not exact values, so the
piece never plays the same way twice.

## Approach

- **Grains, not held notes**: each voice is a `GrainCloud` — a pool of
  short-lived, enveloped, randomized micro-grains (synthesized, not sampled)
  rather than one continuous oscillator. Overlapping grains are what give
  the texture its granular character.
- **Macros, not direct values**: each voice exposes four generative
  controls — **Pitch Range**, **Timbre**, **Motion**, **Complexity** — plus
  direct **Enabled**/**Volume**. These set the field a voice's grains are
  drawn from; the actual pitch/waveform/timing of any given grain is decided
  by the engine at the moment it's spawned.
- **Autonomous drift**: independent of user input, each voice's sampling
  point slowly wanders within its Pitch Range and its overall level
  "breathes" over time, driven by Motion — this is what keeps the piece
  moving and non-repeating with zero interaction.
- **Transport**: Play/Stop gate whether new grains spawn; already-sounding
  grains ring out on their own envelope on Stop rather than cutting
  abruptly. Stop also resets every voice back to its initial state.

## Current implementation

- A native JUCE app shell for macOS with a working CMake build pipeline
- A granular synthesis engine: `VoiceOscillator` (Sine/Saw/FM/Noise
  generators) → `Grain` (one enveloped, panned micro-burst) → `GrainCloud`
  (per-voice grain pool, spawn scheduling, and autonomous drift/breathing)
- A lock-free `VoiceModel` per voice (atomic enabled/volume/pitch-range/
  timbre/motion/complexity, safe to read from the real-time audio thread)
- A voice-bank UI exposing all of the above per voice, plus a transport row
  (Play / Stop-Reset / Randomize)
- Headless regression tests for the voice model, oscillator, and grain cloud

## Project structure

- [CMakeLists.txt](CMakeLists.txt) — CMake entrypoint for the app and all test targets
- [JUCE/](JUCE/) — vendored JUCE framework (git submodule)
- [Sources/JerricanApp/Main.cpp](Sources/JerricanApp/Main.cpp) — app entrypoint, transport, and voice-bank UI
- [Sources/JerricanApp/VoiceModel.h](Sources/JerricanApp/VoiceModel.h) — per-voice generative macro state
- [Sources/JerricanApp/VoiceOscillator.h](Sources/JerricanApp/VoiceOscillator.h) — Sine/Saw/FM/Noise waveform generator
- [Sources/JerricanApp/Grain.h](Sources/JerricanApp/Grain.h) — a single enveloped, panned grain
- [Sources/JerricanApp/GrainCloud.h](Sources/JerricanApp/GrainCloud.h) — per-voice grain pool, scheduler, and drift
- [Sources/JerricanApp/FastRandom.h](Sources/JerricanApp/FastRandom.h) — shared lightweight RNG
- [Tests/](Tests/) — headless regression tests (`VoiceModelTest`, `VoiceOscillatorTest`, `GrainCloudTest`)
- [build/](build/) — generated build output (gitignored)

## Build locally

From the project root:

```bash
cmake -S . -B build
cmake --build build -j4
```

The built app is produced at:

```bash
build/JerricanApp_artefacts/Jerrican.app
```

Run the regression tests directly as built binaries, e.g.:

```bash
./build/VoiceModelTests
./build/VoiceOscillatorTests
./build/GrainCloudTests
```

## Next direction

Voice personalities (Drone/Pulse/Spark/Echo) need more distinct sonic
identity, some grain artifacts need cleanup, and a global "Evolution"
meta-control is planned to let the generative macros themselves drift
autonomously over time, rather than only the grains within a fixed setting.
