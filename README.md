# Jerrican

    . Centered on a clean C++ and JUCE foundation with a clear instrument architecture: a transport layer, a bank of voices, and a set of per-voice controls that can evolve into a full realtime sound engine.

## Approach

The instrument is being shaped around a simple but expressive model:

- Instruments: each voice is assigned a character such as sine, saw, noise, or FM-style motion so the app can grow into a palette of distinct timbral families.
- Voices: the interface exposes a compact voice bank with per-voice enablement, volume, and pitch. This gives the instrument a modular structure from the start.
- Transport: play, stop/reset, and randomize actions provide a lightweight performance layer while preserving a clear separation between UI state and future audio synthesis logic.
- State model: the app currently uses a small voice-state object that can later be connected to sequencers, modulation, and real-time audio generation without rewriting the core architecture.

## Current implementation

- A proper native JUCE app shell for macOS
- A working CMake build pipeline
- A descriptive instrument-style UI with a transport row and a voice bank
- A lightweight voice model that stores name, instrument family, enabled state, volume, and pitch
- A small verification test for the voice model

## Project structure

- [CMakeLists.txt](CMakeLists.txt) — CMake entrypoint for the JUCE app and the voice-model test target
- [JUCE/](JUCE/) — vendored JUCE framework source
- [Sources/JerricanApp/Main.cpp](Sources/JerricanApp/Main.cpp) — app entrypoint and instrument UI shell
- [Sources/JerricanApp/VoiceModel.h](Sources/JerricanApp/VoiceModel.h) — the data model behind the voice bank
- [Tests/VoiceModelTest.cpp](Tests/VoiceModelTest.cpp) — a simple regression-style check for the voice state model
- [build/](build/) — generated build output

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

## Next direction

The next milestone is to connect the voice model to a real audio engine so the instrument can begin producing sound rather than just exposing an expressive control surface. The architecture is already arranged to support that transition cleanly.
