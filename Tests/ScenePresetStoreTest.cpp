#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

#include "ScenePresetStore.h"
#include "SceneState.h"

namespace {

std::filesystem::path makeTempDir() {
    const auto dir = std::filesystem::temp_directory_path() /
                     ("jerrican_scene_preset_test_" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(dir);
    return dir;
}

SceneState makeTestScene() {
    SceneState scene;
    for (std::size_t i = 0; i < scene.voices.size(); ++i) {
        auto& voice = scene.voices[i];
        const float base = static_cast<float>(i) * 0.1f;
        voice.enabled = (i % 2 == 0);
        voice.volume = base + 0.1f;
        voice.pitchLow = base + 0.2f;
        voice.pitchHigh = base + 0.3f;
        voice.timbre = base + 0.4f;
        voice.motion = base + 0.5f;
        voice.complexity = base + 0.6f;
        voice.dissonance = base + 0.7f;
        voice.volumeEvoEnabled = (i % 2 == 0);
        voice.pitchRangeEvoEnabled = (i % 2 != 0);
        voice.timbreEvoEnabled = true;
        voice.motionEvoEnabled = false;
        voice.complexityEvoEnabled = true;
        voice.dissonanceEvoEnabled = false;
    }
    scene.evolutionAmount = 0.42f;
    scene.evolutionSpeed = 0.77f;
    scene.reverbRoom = 0.33f;
    scene.reverbDecay = 0.66f;
    scene.masterVolume = 0.88f;
    return scene;
}

void assertScenesEqual(const SceneState& a, const SceneState& b) {
    for (std::size_t i = 0; i < a.voices.size(); ++i) {
        const auto& va = a.voices[i];
        const auto& vb = b.voices[i];
        assert(va.enabled == vb.enabled);
        assert(std::abs(va.volume - vb.volume) < 1e-4f);
        assert(std::abs(va.pitchLow - vb.pitchLow) < 1e-4f);
        assert(std::abs(va.pitchHigh - vb.pitchHigh) < 1e-4f);
        assert(std::abs(va.timbre - vb.timbre) < 1e-4f);
        assert(std::abs(va.motion - vb.motion) < 1e-4f);
        assert(std::abs(va.complexity - vb.complexity) < 1e-4f);
        assert(std::abs(va.dissonance - vb.dissonance) < 1e-4f);
        assert(va.volumeEvoEnabled == vb.volumeEvoEnabled);
        assert(va.pitchRangeEvoEnabled == vb.pitchRangeEvoEnabled);
        assert(va.timbreEvoEnabled == vb.timbreEvoEnabled);
        assert(va.motionEvoEnabled == vb.motionEvoEnabled);
        assert(va.complexityEvoEnabled == vb.complexityEvoEnabled);
        assert(va.dissonanceEvoEnabled == vb.dissonanceEvoEnabled);
    }
    assert(std::abs(a.evolutionAmount - b.evolutionAmount) < 1e-4f);
    assert(std::abs(a.evolutionSpeed - b.evolutionSpeed) < 1e-4f);
    assert(std::abs(a.reverbRoom - b.reverbRoom) < 1e-4f);
    assert(std::abs(a.reverbDecay - b.reverbDecay) < 1e-4f);
    assert(std::abs(a.masterVolume - b.masterVolume) < 1e-4f);
}

}  // namespace

int main() {
    const auto tempDir = makeTempDir();
    ScenePresetStore store(tempDir);

    assert(store.listPresetNames().empty());

    // Save -> load round-trip preserves every field.
    {
        const auto original = makeTestScene();
        assert(store.save("Live Set 1", original));

        SceneState loaded;
        assert(store.load("Live Set 1", loaded));
        assertScenesEqual(original, loaded);
    }

    // listPresetNames reflects saved files, sorted.
    {
        SceneState scene;
        store.save("Zebra", scene);
        store.save("Alpha", scene);

        const auto names = store.listPresetNames();
        assert(names.size() == 3);
        assert(names[0] == "Alpha");
        assert(names[1] == "Live Set 1");
        assert(names[2] == "Zebra");
    }

    // remove deletes the preset file.
    {
        assert(store.remove("Zebra"));
        const auto names = store.listPresetNames();
        assert(names.size() == 2);
        for (const auto& name : names) {
            assert(name != "Zebra");
        }
        assert(!store.remove("Zebra"));
    }

    // Path-traversal attempts are rejected, matching MidiPresetStore.
    {
        SceneState scene;
        assert(!store.save("../escape", scene));
        assert(!store.load("../escape", scene));
        assert(!store.remove("../escape"));
        assert(!store.save("nested/name", scene));
    }

    // A malformed value is skipped rather than crashing the loader.
    {
        const auto malformedPath = tempDir / "Malformed.jscene";
        std::ofstream malformedFile(malformedPath);
        malformedFile << "voice0.volume=not-a-number\n";
        malformedFile << "global.masterVolume=0.5\n";
        malformedFile.close();

        SceneState loaded;
        assert(store.load("Malformed", loaded));
        assert(std::abs(loaded.masterVolume - 0.5f) < 1e-4f);
    }

    // operator== — used by PresetControls to detect dirty/matching state.
    {
        const auto a = makeTestScene();
        auto b = makeTestScene();
        assert(a == b);

        b.voices[0].volume += 0.5f;
        assert(!(a == b));
        b.voices[0].volume -= 0.5f;
        assert(a == b);

        b.voices[2].pitchRangeEvoEnabled = !b.voices[2].pitchRangeEvoEnabled;
        assert(!(a == b));
        b.voices[2].pitchRangeEvoEnabled = a.voices[2].pitchRangeEvoEnabled;
        assert(a == b);

        b.masterVolume += 0.5f;
        assert(!(a == b));

        // Tiny float differences (well within round-trip tolerance) still
        // compare equal.
        auto c = makeTestScene();
        c.masterVolume += 1e-6f;
        assert(a == c);
    }

    std::filesystem::remove_all(tempDir);

    std::cout << "ScenePresetStore tests passed" << std::endl;
    return 0;
}
