cmake_minimum_required(VERSION 3.24)
project(Jerrican LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES arm64)

add_subdirectory(JUCE)

juce_add_gui_app(JerricanApp
    PRODUCT_NAME "Jerrican"
    VERSION "0.1.0"
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE
)

juce_generate_juce_header(JerricanApp)

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

target_compile_definitions(JerricanApp
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:JerricanApp,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:JerricanApp,JUCE_VERSION>")

target_link_libraries(JerricanApp
    PRIVATE
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
