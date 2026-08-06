#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "PresetNameValidation.h"
#include "SceneState.h"

// Named full-state-snapshot persistence — same shape and conventions as
// MidiPresetStore (injected directory, .jscene files, one key=value pair
// per line, isValidPresetName guard against path traversal, malformed
// lines skipped rather than crashing), just serializing a SceneState
// instead of a binding table. Kept as a separate class/file rather than
// folded into MidiPresetStore since the two are conceptually different
// preset systems (parameter values vs. controller mappings) that happen
// to share a persistence shape.
class ScenePresetStore {
public:
    explicit ScenePresetStore(std::filesystem::path directory) : directory_(std::move(directory)) {}

    std::vector<std::string> listPresetNames() const {
        std::vector<std::string> names;
        if (!std::filesystem::exists(directory_)) {
            return names;
        }
        for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
            if (entry.is_regular_file() && entry.path().extension() == kExtension) {
                names.push_back(entry.path().stem().string());
            }
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    bool save(const std::string& name, const SceneState& state) const {
        if (!isValidPresetName(name)) {
            return false;
        }

        std::error_code errorCode;
        std::filesystem::create_directories(directory_, errorCode);

        std::ofstream file(pathFor(name));
        if (!file.is_open()) {
            return false;
        }

        file << serialize(state);
        return true;
    }

    bool load(const std::string& name, SceneState& state) const {
        if (!isValidPresetName(name)) {
            return false;
        }

        std::ifstream file(pathFor(name));
        if (!file.is_open()) {
            return false;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        state = deserialize(buffer.str());
        return true;
    }

    // Same key=value text shape as the on-disk .jscene format, exposed so
    // JerricanAudioProcessor::getStateInformation/setStateInformation can
    // reuse it for a plugin instance's own state (a different concern from
    // named preset files, but identical serialization needs).
    static std::string serialize(const SceneState& state) {
        std::ostringstream file;
        for (std::size_t i = 0; i < state.voices.size(); ++i) {
            const auto& voice = state.voices[i];
            const std::string prefix = "voice" + std::to_string(i) + ".";
            file << prefix << "enabled=" << (voice.enabled ? 1 : 0) << "\n";
            file << prefix << "volume=" << voice.volume << "\n";
            file << prefix << "pitchLow=" << voice.pitchLow << "\n";
            file << prefix << "pitchHigh=" << voice.pitchHigh << "\n";
            file << prefix << "timbre=" << voice.timbre << "\n";
            file << prefix << "motion=" << voice.motion << "\n";
            file << prefix << "complexity=" << voice.complexity << "\n";
            file << prefix << "dissonance=" << voice.dissonance << "\n";
            file << prefix << "rootSemitoneOffset=" << voice.rootSemitoneOffset << "\n";
            file << prefix << "volumeEvoEnabled=" << (voice.volumeEvoEnabled ? 1 : 0) << "\n";
            file << prefix << "pitchRangeEvoEnabled=" << (voice.pitchRangeEvoEnabled ? 1 : 0) << "\n";
            file << prefix << "timbreEvoEnabled=" << (voice.timbreEvoEnabled ? 1 : 0) << "\n";
            file << prefix << "motionEvoEnabled=" << (voice.motionEvoEnabled ? 1 : 0) << "\n";
            file << prefix << "complexityEvoEnabled=" << (voice.complexityEvoEnabled ? 1 : 0) << "\n";
            file << prefix << "dissonanceEvoEnabled=" << (voice.dissonanceEvoEnabled ? 1 : 0) << "\n";
        }
        file << "global.evolutionAmount=" << state.evolutionAmount << "\n";
        file << "global.evolutionSpeed=" << state.evolutionSpeed << "\n";
        file << "global.reverbRoom=" << state.reverbRoom << "\n";
        file << "global.reverbDecay=" << state.reverbDecay << "\n";
        file << "global.masterVolume=" << state.masterVolume << "\n";
        return file.str();
    }

    static SceneState deserialize(const std::string& text) {
        SceneState state;
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) {
            parseLine(line, state);
        }
        return state;
    }

    bool remove(const std::string& name) const {
        if (!isValidPresetName(name)) {
            return false;
        }

        std::error_code errorCode;
        return std::filesystem::remove(pathFor(name), errorCode);
    }

private:
    static constexpr const char* kExtension = ".jscene";

    std::filesystem::path pathFor(const std::string& name) const {
        return directory_ / (name + kExtension);
    }

    static void parseLine(const std::string& line, SceneState& state) {
        const auto equalsPos = line.find('=');
        if (equalsPos == std::string::npos) {
            return;
        }
        const std::string key = line.substr(0, equalsPos);
        const std::string valueToken = line.substr(equalsPos + 1);

        float value = 0.0f;
        try {
            value = std::stof(valueToken);
        } catch (const std::exception&) {
            // Malformed line (hand-edited or corrupted scene file) — skip
            // it rather than letting stof's exception crash the app.
            return;
        }

        if (key.rfind("global.", 0) == 0) {
            const std::string field = key.substr(7);
            if (field == "evolutionAmount") state.evolutionAmount = value;
            else if (field == "evolutionSpeed") state.evolutionSpeed = value;
            else if (field == "reverbRoom") state.reverbRoom = value;
            else if (field == "reverbDecay") state.reverbDecay = value;
            else if (field == "masterVolume") state.masterVolume = value;
            return;
        }

        if (key.rfind("voice", 0) != 0) {
            return;
        }
        const auto dotPos = key.find('.');
        if (dotPos == std::string::npos) {
            return;
        }
        int index = -1;
        try {
            index = std::stoi(key.substr(5, dotPos - 5));
        } catch (const std::exception&) {
            return;
        }
        if (index < 0 || static_cast<std::size_t>(index) >= state.voices.size()) {
            return;
        }

        auto& voice = state.voices[static_cast<std::size_t>(index)];
        const std::string field = key.substr(dotPos + 1);
        const bool boolValue = value != 0.0f;
        if (field == "enabled") voice.enabled = boolValue;
        else if (field == "volume") voice.volume = value;
        else if (field == "pitchLow") voice.pitchLow = value;
        else if (field == "pitchHigh") voice.pitchHigh = value;
        else if (field == "timbre") voice.timbre = value;
        else if (field == "motion") voice.motion = value;
        else if (field == "complexity") voice.complexity = value;
        else if (field == "dissonance") voice.dissonance = value;
        else if (field == "rootSemitoneOffset") voice.rootSemitoneOffset = static_cast<int>(value);
        else if (field == "volumeEvoEnabled") voice.volumeEvoEnabled = boolValue;
        else if (field == "pitchRangeEvoEnabled") voice.pitchRangeEvoEnabled = boolValue;
        else if (field == "timbreEvoEnabled") voice.timbreEvoEnabled = boolValue;
        else if (field == "motionEvoEnabled") voice.motionEvoEnabled = boolValue;
        else if (field == "complexityEvoEnabled") voice.complexityEvoEnabled = boolValue;
        else if (field == "dissonanceEvoEnabled") voice.dissonanceEvoEnabled = boolValue;
    }

    std::filesystem::path directory_;
};
