#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PitchForgeAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit PitchForgeAudioProcessorEditor(PitchForgeAudioProcessor&);
    ~PitchForgeAudioProcessorEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    PitchForgeAudioProcessor& processor;
    juce::Slider speed, amount, humanize, mix;
    juce::ComboBox scale;
    juce::Slider key;
    juce::Label title, status;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAtt, amountAtt, humanAtt, mixAtt, keyAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAtt;
    void styleSlider(juce::Slider& s);
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchForgeAudioProcessorEditor)
};
