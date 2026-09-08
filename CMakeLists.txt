cmake_minimum_required(VERSION 3.22)
project(PitchForge VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.8
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)

juce_add_plugin(PitchForge
  COMPANY_NAME "Independent Audio Tools"
  PLUGIN_MANUFACTURER_CODE IAT0
  PLUGIN_CODE Pf01
  FORMATS VST3 Standalone
  PRODUCT_NAME "PitchForge"
  BUNDLE_ID "com.independentaudiotools.pitchforge"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  VST3_CATEGORIES "Fx" "Pitch Shift")

juce_generate_juce_header(PitchForge)

target_sources(PitchForge PRIVATE src/PluginProcessor.cpp src/PluginEditor.cpp)
target_compile_features(PitchForge PRIVATE cxx_std_17)
target_link_libraries(PitchForge PRIVATE juce::juce_audio_utils juce::juce_dsp)
