cmake_minimum_required(VERSION 3.24)
project(Jerrican LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Universal binary: Apple Silicon + Intel in one build.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
# Oldest macOS an Intel Mac running this is likely to have; also caps what
# API/SDK surface we can rely on.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")

add_subdirectory(JUCE)

juce_add_gui_app(JerricanApp
    PRODUCT_NAME "Jerrican"
    VERSION "0.1.0"
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

target_compile_definitions(JerricanApp
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:JerricanApp,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:JerricanApp,JUCE_VERSION>")

target_link_libraries(JerricanApp
    PRIVATE
        JerricanAssets
        juce::juce_gui_extra
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

set_target_properties(JerricanApp PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE FALSE
)
