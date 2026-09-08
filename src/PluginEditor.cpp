#include "PluginEditor.h"

PitchForgeAudioProcessorEditor::PitchForgeAudioProcessorEditor(PitchForgeAudioProcessor& p) : AudioProcessorEditor(&p), processor(p) {
    setSize(720, 430);
    title.setText("PITCHFORGE", juce::dontSendNotification); title.setFont(juce::Font(26.0f, juce::Font::bold)); title.setColour(juce::Label::textColourId, juce::Colours::white); addAndMakeVisible(title);
    status.setText("REAL-TIME PITCH CONTROL", juce::dontSendNotification); status.setColour(juce::Label::textColourId, juce::Colours::lightgrey); addAndMakeVisible(status);
    styleSlider(speed); styleSlider(amount); styleSlider(humanize); styleSlider(mix); styleSlider(key);
    addAndMakeVisible(speed); addAndMakeVisible(amount); addAndMakeVisible(humanize); addAndMakeVisible(mix); addAndMakeVisible(key);
    speed.setTextValueSuffix(" %"); amount.setTextValueSuffix(" %"); humanize.setTextValueSuffix(" %"); mix.setTextValueSuffix(" %"); key.setRange(0,11,1);
    scale.addItemList({"Chromatic","Major","Minor"},1); addAndMakeVisible(scale);
    speedAtt=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(),"speed",speed);
    amountAtt=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(),"amount",amount);
    humanAtt=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(),"humanize",humanize);
    mixAtt=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(),"mix",mix);
    keyAtt=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(),"key",key);
    scaleAtt=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.getAPVTS(),"scale",scale);
    startTimerHz(30);
}
void PitchForgeAudioProcessorEditor::styleSlider(juce::Slider& s){ s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,20); s.setRange(0,1,0.001); }
void PitchForgeAudioProcessorEditor::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(0xff0b0d10));
    g.setColour(juce::Colour(0xff151920)); g.fillRoundedRectangle(18,18,684,394,18);
    g.setColour(juce::Colour(0xff222832)); g.fillRoundedRectangle(34,86,652,104,14);
    g.setColour(juce::Colour(0xff8be9a8)); g.fillRoundedRectangle(54,151,612,3,2);
    g.setColour(juce::Colours::white); g.setFont(13); g.drawText("KEY / SCALE", 54, 208, 130,20,juce::Justification::left);
    g.drawText("RETUNE", 54, 285, 100,20,juce::Justification::left);
    g.drawText("CORRECTION", 208, 285, 120,20,juce::Justification::left);
    g.drawText("HUMANIZE", 370, 285, 110,20,juce::Justification::left);
    g.drawText("MIX", 534, 285, 60,20,juce::Justification::left);
    g.setColour(juce::Colours::lightgrey); g.setFont(12); g.drawText("Original pitch-processing alternative — no proprietary assets or algorithms.",54,374,600,20,juce::Justification::left);
}
void PitchForgeAudioProcessorEditor::resized(){ title.setBounds(44,34,240,36); status.setBounds(440,40,230,25); scale.setBounds(54,232,160,28); key.setBounds(240,218,90,55); speed.setBounds(38,300,135,80); amount.setBounds(195,300,135,80); humanize.setBounds(355,300,135,80); mix.setBounds(515,300,135,80); }
void PitchForgeAudioProcessorEditor::timerCallback(){ status.setText("REAL-TIME PITCH CONTROL",juce::dontSendNotification); }
