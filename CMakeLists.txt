cmake_minimum_required(VERSION 3.22)

# Sensible macOS defaults for native builds; override with -D if needed.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
endif()

if(NOT DEFINED LUDENSCOPE_VERSION)
  set(LUDENSCOPE_VERSION "2.1.2")
endif()
project(LudensScopeSurround VERSION ${LUDENSCOPE_VERSION} LANGUAGES C CXX)
add_compile_definitions (LUDENSCOPE_VERSION_STRING="${LUDENSCOPE_VERSION}")

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)

FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.15
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)

FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(LudensScopeSurroundBinaryData SOURCES
  Source/assets/ludens_logo.png)

set(LUDENSCOPE_SURROUND_FORMATS VST3 AU Standalone)
if(APPLE OR WIN32)
  list(APPEND LUDENSCOPE_SURROUND_FORMATS AAX)
endif()

set(LUDENSCOPE_COPY_PLUGIN TRUE)
if(DEFINED ENV{CI})
  set(LUDENSCOPE_COPY_PLUGIN FALSE)
endif()

juce_add_plugin(LudensScopeSurround
  COMPANY_NAME "LudensScope Surround"
  BUNDLE_ID "local.ludensscopesurround"
  PLUGIN_MANUFACTURER_CODE LDSR
  PLUGIN_CODE LDSR
  FORMATS ${LUDENSCOPE_SURROUND_FORMATS}
  PRODUCT_NAME "LudensScope Surround"
  DESCRIPTION "Stereo and decoded 5.1 spectrum, loudness and scope analyzer"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD ${LUDENSCOPE_COPY_PLUGIN}
  NEEDS_CURL TRUE
  VST3_CATEGORIES "Fx Analyzer")

target_sources(LudensScopeSurround PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/dsp/LoudnessMeter.cpp
  Source/dsp/SpectrumAnalyzer.cpp)

target_include_directories(LudensScopeSurround PRIVATE Source)

target_compile_definitions(LudensScopeSurround PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_MODAL_LOOPS_PERMITTED=1
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0)

target_link_libraries(LudensScopeSurround PRIVATE
  juce::juce_audio_utils
  juce::juce_dsp
  juce::juce_cryptography
  LudensScopeSurroundBinaryData
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

# CLAP via the MIT-licensed clap-juce-extensions wrapper.
FetchContent_Declare(clap_juce_extensions
  GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
  GIT_TAG 6bde108329ff9fc24404b79b8b397261a75cf6a8
  GIT_SUBMODULES_RECURSE TRUE
  GIT_PROGRESS TRUE)

FetchContent_MakeAvailable(clap_juce_extensions)

clap_juce_extensions_plugin(TARGET LudensScopeSurround
  CLAP_ID "com.ltillegal.ludensscopesurround"
  CLAP_FEATURES audio-effect analyzer stereo surround utility)

option(LUDENSCOPE_BUILD_TESTS "Build the offline calibration test" OFF)

if(LUDENSCOPE_BUILD_TESTS)
  juce_add_console_app(LudensScopeSurroundCalibration PRODUCT_NAME "LudensScopeSurroundCalibration")
  target_sources(LudensScopeSurroundCalibration PRIVATE
    Tests/Calibration.cpp
    Source/dsp/SpectrumAnalyzer.cpp)
  target_include_directories(LudensScopeSurroundCalibration PRIVATE Source)
  target_link_libraries(LudensScopeSurroundCalibration PRIVATE
    juce::juce_dsp
    juce::juce_recommended_config_flags)

  add_executable(LudensScopeSurroundLoudnessCalibration
    Tests/LoudnessCalibration.cpp
    Source/dsp/LoudnessMeter.cpp)
  target_include_directories(LudensScopeSurroundLoudnessCalibration PRIVATE Source)
  target_compile_features(LudensScopeSurroundLoudnessCalibration PRIVATE cxx_std_17)

  add_executable(LudensScopeSurroundSurround
    Tests/Surround.cpp
    Source/dsp/LoudnessMeter.cpp)
  target_include_directories(LudensScopeSurroundSurround PRIVATE Source)
  target_compile_features(LudensScopeSurroundSurround PRIVATE cxx_std_17)

  juce_add_console_app(LudensScopeSurroundUiSnapshot PRODUCT_NAME "LudensScopeSurroundUiSnapshot" NEEDS_CURL TRUE)
  target_sources(LudensScopeSurroundUiSnapshot PRIVATE
    Tests/UiSnapshot.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/LoudnessMeter.cpp
    Source/dsp/SpectrumAnalyzer.cpp)
  target_include_directories(LudensScopeSurroundUiSnapshot PRIVATE Source)
  target_compile_definitions(LudensScopeSurroundUiSnapshot PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    LUDENSCOPE_TEST_HOOKS=1)
  target_link_libraries(LudensScopeSurroundUiSnapshot PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_cryptography
    LudensScopeSurroundBinaryData
    juce::juce_recommended_config_flags)

  add_test(NAME loudness COMMAND LudensScopeSurroundLoudnessCalibration)
  add_test(NAME surround COMMAND LudensScopeSurroundSurround)
  add_test(NAME spectrum COMMAND LudensScopeSurroundCalibration)

  # The UI snapshot needs a display (Xvfb on Linux); hosted macOS/Windows
  # runners do not provide a GUI session for this.
  if(UNIX AND NOT APPLE)
    add_test(NAME ui_snapshot COMMAND LudensScopeSurroundUiSnapshot)
  endif()
endif()
