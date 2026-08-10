<img src="Resources/AppIcon.png" width="120" alt="Jerrican icon">

# Jerrican

Jerrican is a macOS-native generative music instrument, built on C++ and
JUCE, shipping as a Standalone app and as an AU/VST3 plugin. It's not a
sequencer or a sample player — it's a granular synthesis engine that
composes itself continuously from a field of possibilities set by the
user, in the spirit of Umberto Eco's *opera aperta* (the "open work"):
the controls shape tendencies and ranges, not exact values, so the piece
never plays the same way twice.

## Approach

- **Grains, not held notes**: each voice is a `GrainCloud` — a pool of
  short-lived, enveloped, randomized micro-grains (synthesized, not sampled)
  rather than one continuous oscillator. Overlapping grains are what give
  the texture its granular character.
- **Macros, not direct values**: each voice exposes five generative
  controls — **Pitch Range**, **Timbre**, **Motion**, **Complexity**,
  **Dissonance** — plus direct **Enabled**/**Volume** and a **Key** (root
  note, not affected by Evolution/Randomize). These set the field a
  voice's grains are drawn from; the actual pitch/waveform/timing of any
  given grain is decided by the engine at the moment it's spawned.
  Dissonance blends between quantizing to that voice's rooted scale
  (harmonizes with other voices at low Dissonance, especially when their
  Keys are set a deliberate interval apart) and fully free/chromatic.
- **Autonomous drift**: a global **Evolution Amount/Speed** pair drives
  every voice's macros to independently wander — occasionally picking a
  new random target and gliding toward it — while playing; each macro has
  its own on/off switch so it can be pinned under manual control while
  the rest keep drifting. This is what keeps the piece moving and
  non-repeating with zero interaction.
- **Transport**: Play/Stop gate whether new grains spawn; already-sounding
  grains ring out on their own envelope on Stop rather than cutting
  abruptly. Reset snaps every voice back to its starting values;
  Randomize rerolls every voice's levers regardless of transport state.
- **Host Sync** (AU/VST3 only): an opt-in toggle that makes Play/Stop
  follow the host DAW's own transport, so a count-in before recording
  starts grain spawning on the same downbeat. Off by default; Standalone
  has no host transport to follow, so the toggle only appears when hosted.

## Current implementation

- A native JUCE app shell for macOS with a working CMake build pipeline,
  shipping as Standalone, AU, and VST3
- A granular synthesis engine: `VoiceOscillator` (Sine/Saw/FM/Noise
  generators) → `Grain` (one enveloped, panned micro-burst) → `GrainCloud`
  (per-voice grain pool, spawn scheduling, and autonomous drift/breathing),
  with `HarmonicScale` quantizing pitch to each voice's rooted scale as
  Dissonance approaches 0
- A lock-free `VoiceModel` per voice (atomic enabled/volume/pitch-range/
  timbre/motion/complexity/dissonance/root, safe to read from the
  real-time audio thread) and an `EvolutionEngine` per voice for
  autonomous per-macro drift
- Effects chain: Reverb (Room/Decay), Master Volume
- A voice-bank UI exposing all of the above per voice, plus a transport
  row (Play / Stop / Reset / Randomize) and an in-app Help popup
- Full MIDI Learn (`MidiBindingManager`/`MidiPresetStore`) and Scenes
  (`ScenePresetStore`/`SceneState`, full-instrument-state snapshots) —
  per-voice targets apply to whichever voice is currently focused
  (Voice Select pads); Transport is bindable too, as a global action
- Audio export: a Record button (Standalone only — a hosted AU/VST3
  instance leaves recording/bouncing to the DAW's own workflow) captures
  the exact final mix to a timestamped WAV under `~/Music/Jerrican
  Recordings`, via a background-threaded writer (`AudioRecorder.h`) so
  the realtime audio callback never blocks on file I/O; "Open Folder"
  reveals the last recording in Finder
- Headless regression tests for every JUCE-free engine class (8 test
  binaries — the engine has zero JUCE dependency, so these link and run
  with no app bundle/audio device needed)

## Project structure

- [CMakeLists.txt](CMakeLists.txt) — CMake entrypoint for the app and all test targets
- [JUCE/](JUCE/) — vendored JUCE framework (git submodule)
- [Sources/JerricanApp/Main.cpp](Sources/JerricanApp/Main.cpp) — plugin factory entrypoint
- [Sources/JerricanApp/JerricanProcessor.h](Sources/JerricanApp/JerricanProcessor.h) — audio/MIDI processing, transport, Host Sync, Scene capture/apply
- [Sources/JerricanApp/JerricanEditor.h](Sources/JerricanApp/JerricanEditor.h) — the voice-bank UI, Help/Bindings/Scenes popups
- [Sources/JerricanApp/VoiceModel.h](Sources/JerricanApp/VoiceModel.h) — per-voice generative macro state
- [Sources/JerricanApp/VoiceOscillator.h](Sources/JerricanApp/VoiceOscillator.h) — Sine/Saw/FM/Noise waveform generator
- [Sources/JerricanApp/Grain.h](Sources/JerricanApp/Grain.h) — a single enveloped, panned grain
- [Sources/JerricanApp/GrainCloud.h](Sources/JerricanApp/GrainCloud.h) — per-voice grain pool, scheduler, and drift
- [Sources/JerricanApp/HarmonicScale.h](Sources/JerricanApp/HarmonicScale.h) — rooted-scale pitch quantization for Dissonance
- [Sources/JerricanApp/EvolutionEngine.h](Sources/JerricanApp/EvolutionEngine.h) — per-voice autonomous macro drift
- [Sources/JerricanApp/MidiBindingManager.h](Sources/JerricanApp/MidiBindingManager.h) / [MidiPresetStore.h](Sources/JerricanApp/MidiPresetStore.h) — MIDI Learn and its named presets
- [Sources/JerricanApp/SceneState.h](Sources/JerricanApp/SceneState.h) / [ScenePresetStore.h](Sources/JerricanApp/ScenePresetStore.h) — full-state Scene snapshots
- [Sources/JerricanApp/AudioRecorder.h](Sources/JerricanApp/AudioRecorder.h) — background-threaded WAV export of the final mix
- [Sources/JerricanApp/FastRandom.h](Sources/JerricanApp/FastRandom.h) — shared lightweight RNG
- [Tests/](Tests/) — headless regression tests, one per JUCE-free engine class
- [build/](build/) — generated build output (gitignored)

## Build locally

From the project root:

```bash
cmake -S . -B build
cmake --build build -j4
```

The built plugins are produced at:

```bash
build/JerricanApp_artefacts/Standalone/Jerrican.app
build/JerricanApp_artefacts/AU/Jerrican.component
build/JerricanApp_artefacts/VST3/Jerrican.vst3
```

Run the regression tests directly as built binaries, e.g.:

```bash
./build/VoiceModelTests
./build/VoiceOscillatorTests
./build/GrainCloudTests
./build/EvolutionEngineTests
```

## Relationship to Marmite

Jerrican's proven, domain-generic infrastructure — the CMake/JUCE setup,
native window chrome, the atomic UI-thread/audio-thread pattern, the
Evolution drift mechanic, and the entire MIDI Learn + Scenes preset
system — was ported to build Marmite, its generative drum-machine
sibling, applying the same self-composing philosophy to rhythm instead
of pitched granular texture.
