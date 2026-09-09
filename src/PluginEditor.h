#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PitchForgeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PitchForgeLookAndFeel();
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override;
};

class PitchForgeAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PitchForgeAudioProcessorEditor(PitchForgeAudioProcessor&);
    ~PitchForgeAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    PitchForgeAudioProcessor& processor;
    PitchForgeLookAndFeel lf;
    juce::Slider speed, amount, sustain, mix, humanize, range, pitchRef, width, doublerMix;
    juce::ComboBox scale, stabilizer, key;
    juce::ToggleButton chromatic, lowLatency, detected, heatmap, doubler;
    juce::TextButton noteMode, majorMode, minorMode, holdMajor, holdMinor, resetButton;
    juce::Label title, subtitle, latencyLabel, confidenceLabel, pitchLabel, outputLabel;
    std::array<float, 72> inputHistory{};
    std::array<float, 72> outputHistory{};
    int historyPos = 0;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAtt, amountAtt, sustainAtt, mixAtt, humanizeAtt, rangeAtt, refAtt, widthAtt, doublerMixAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAtt, stabilizerAtt, keyAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> chromAtt, latencyAtt, detectedAtt, heatmapAtt, doublerAtt;
    void addKnob(juce::Slider&,const juce::String&,float,float,float);
    void drawKeyboard(juce::Graphics&,juce::Rectangle<int> area);
    void drawHistory(juce::Graphics&, juce::Rectangle<float> area, const std::array<float,72>& history, juce::Colour lineColour);
    void setScaleMode(int mode);
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchForgeAudioProcessorEditor)
};
