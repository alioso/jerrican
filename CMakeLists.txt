cmake_minimum_required(VERSION 3.24)
project(Jerrican LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    # Universal binary: Apple Silicon + Intel in one build.
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    # Oldest macOS an Intel Mac running this is likely to have; also caps
    # what API/SDK surface we can rely on.
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
endif()

add_subdirectory(JUCE)

# Standalone + AU + VST3 on macOS; AU is an Apple-only format, so
# Windows/Linux builds are Standalone + VST3. Deliberately excludes
# AUv3/Unity/LV2/VST2 (not targeted). PLUGIN_MANUFACTURER_CODE/PLUGIN_CODE
# are permanent once shipped (hosts key cached plugin identity off them),
# so chosen deliberately now rather than left at a JUCE default.
set(JerricanApp_FORMATS Standalone VST3)
if(APPLE)
    list(APPEND JerricanApp_FORMATS AU)
endif()

juce_add_plugin(JerricanApp
    PRODUCT_NAME "Jerrican"
    VERSION "0.1.0"
    COMPANY_NAME "Alban Bailly"
    BUNDLE_ID "com.albanbailly.jerrican"
    PLUGIN_MANUFACTURER_CODE Abal
    PLUGIN_CODE Jrcn
    FORMATS ${JerricanApp_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Generator
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/AppIcon.png"
)

juce_generate_juce_header(JerricanApp)

juce_add_binary_data(JerricanAssets
    SOURCES
        Resources/AppIcon.png
)

target_sources(JerricanApp
    PRIVATE
        Sources/JerricanApp/Main.cpp
)

target_include_directories(JerricanApp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/JerricanApp
)

add_executable(VoiceModelTests
    Tests/VoiceModelTest.cpp
)

target_include_directories(VoiceModelTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/JerricanApp
)

add_executable(VoiceOscillatorTests
    Tests/VoiceOscillatorTest.cpp
)

target_include_directories(VoiceOscillatorTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/JerricanApp
)

add_executable(GrainCloudTests
    Tests/GrainCloudTest.cpp
)

target_include_directories(GrainCloudTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/JerricanApp
)

add_executable(EvolutionEngineTests
    Tests/EvolutionEngineTest.cpp
)

target_include_directories(EvolutionEngineTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/JerricanApp
)

add_executable(HarmonicScaleTests
    Tests/HarmonicScaleTest.cpp
)

target_include_directories(HarmonicScaleTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/JerricanApp
)

add_executable(ChordScaleTests
    Tests/ChordScaleTest.cpp
)

target_include_directories(ChordScaleTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/JerricanApp
)

add_executable(MidiBindingManagerTests
    Tests/MidiBindingManagerTest.cpp
)

target_include_directories(MidiBindingManagerTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/JerricanApp
)

add_executable(MidiPresetStoreTests
    Tests/MidiPresetStoreTest.cpp
)

target_include_directories(MidiPresetStoreTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/JerricanApp
)

add_executable(ScenePresetStoreTests
    Tests/ScenePresetStoreTest.cpp
)

target_include_directories(ScenePresetStoreTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Sources/JerricanApp
)

target_compile_definitions(JerricanApp
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        # No VST2 version of this plugin has ever been (or will be)
        # released, so there's no VST2/VST3 parameter-automation
        # migration to preserve — this only silences JUCE's build-time
        # warning about that concern, not a real compatibility issue here.
        JUCE_VST3_CAN_REPLACE_VST2=0
        # See Sources/JerricanApp/StandaloneApp.h — restores a native
        # macOS title bar (rounded corners, traffic lights) that JUCE's
        # default Standalone window doesn't use.
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:JerricanApp,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:JerricanApp,JUCE_VERSION>")

target_link_libraries(JerricanApp
    PRIVATE
        JerricanAssets
        juce::juce_gui_extra
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Gathers every built format into a flat, shareable folder — run
# `cmake --install build --prefix dist --component release` (after
# configuring with -DCMAKE_BUILD_TYPE=Release, which is what puts
# output under a Release/ subfolder in the first place) to collect
# dist/Standalone, dist/VST3, (dist/AU on macOS) with no extra clutter.
# The explicit COMPONENT is required: JUCE's own add_subdirectory(JUCE)
# above registers its own (uncomponented) install rules for the whole
# SDK, which `cmake --install` runs by default unless a specific
# --component is requested. Addresses each format's subfolder directly
# (rather than the whole artefacts tree, which also holds JUCE's
# intermediate JuceLibraryCode/ and a static lib) — this only works
# cleanly because we always build Release for sharing, so the Release/
# subfolder name is a safe assumption here specifically.
foreach(format Standalone VST3 AU)
    if(format STREQUAL "AU" AND NOT APPLE)
        continue()
    endif()
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/JerricanApp_artefacts/Release/${format}/"
            DESTINATION "${format}"
            USE_SOURCE_PERMISSIONS
            COMPONENT release)
endforeach()
