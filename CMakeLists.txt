cmake_minimum_required(VERSION 3.22)
project(PitchForge VERSION 3.0.0)

include(FetchContent)
FetchContent_Declare(juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.8
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)

juce_add_plugin(PitchForge
  COMPANY_NAME "Independent Audio Tools"
  PLUGIN_MANUFACTURER_CODE IAT0
  PLUGIN_CODE Pf03
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

target_compile_definitions(PitchForge_VST3 PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
juce_generate_juce_header(PitchForge)
target_sources(PitchForge PRIVATE src/PluginProcessor.cpp src/PluginEditor.cpp)
target_compile_features(PitchForge PRIVATE cxx_std_17)
target_link_libraries(PitchForge PRIVATE juce::juce_audio_utils juce::juce_dsp)
