cmake_minimum_required(VERSION 3.22)
project(PitchForge VERSION 4.3.1)

include(FetchContent)

# Keep downloaded archive extraction timestamps deterministic for CMake 3.31+.
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.8
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)

# High-quality open-source pitch/time processor. SoundTouch is LGPL-2.1.
set(SOUNDTOUCH_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SOUNDTOUCH_DLL OFF CACHE BOOL "" FORCE)
set(SOUNDSTRETCH OFF CACHE BOOL "" FORCE)
set(OPENMP OFF CACHE BOOL "" FORCE)
set(INTEGER_SAMPLES OFF CACHE BOOL "" FORCE)
# Use a GitHub-hosted, immutable SoundTouch 2.3.3 commit. The previous
# release tarball host intermittently timed out on GitHub Actions runners.
# Pin the exact 2.3.3 commit to avoid floating dependency contents.
FetchContent_Declare(soundtouch
  GIT_REPOSITORY https://github.com/stenzek/soundtouch.git
  GIT_TAG e83424d5928ab8513d2d082779c275765dee31b9
  GIT_SHALLOW FALSE)
FetchContent_MakeAvailable(soundtouch)

juce_add_plugin(PitchForge
  COMPANY_NAME "Independent Audio Tools"
  PLUGIN_MANUFACTURER_CODE IAT0
  PLUGIN_CODE Pf04
  FORMATS VST3 Standalone
  PRODUCT_NAME "PitchForge"
  PLUGIN_NAME "PitchForge"
  BUNDLE_ID "com.independentaudiotools.pitchforge"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CAN_REPLACE_VST2 FALSE
  VST3_AUTO_MANIFEST TRUE
  VST3_CATEGORIES "Fx" "Pitch Shift"
)

target_compile_definitions(PitchForge_VST3 PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0 SOUNDTOUCH_FLOAT_SAMPLES=1)
juce_generate_juce_header(PitchForge)
target_sources(PitchForge PRIVATE src/PluginProcessor.cpp src/PluginEditor.cpp)
target_compile_features(PitchForge PRIVATE cxx_std_17)
target_link_libraries(PitchForge PRIVATE juce::juce_audio_utils juce::juce_dsp SoundTouch)
