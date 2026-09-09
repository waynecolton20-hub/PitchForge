#include "PluginEditor.h"

namespace { constexpr int notes=12; const char* noteNames[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}; }

PitchForgeLookAndFeel::PitchForgeLookAndFeel(){ setColour(juce::Slider::textBoxTextColourId,juce::Colours::white); }
void PitchForgeLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider&)
{
    auto b=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(6.0f); auto c=b.getCentre(); float r=juce::jmin(b.getWidth(),b.getHeight())*0.43f;
    g.setColour(juce::Colour(0xff0e1117)); g.fillEllipse(c.x-r,c.y-r,2*r,2*r);
    juce::Colour ring(0xff9b61ff); g.setColour(juce::Colour(0xff2c313b)); g.drawEllipse(c.x-r,c.y-r,2*r,2*r,8.0f);
    juce::Path arc; arc.addCentredArc(c.x,c.y,r,r,r,start,end*pos+start*(1.0f-pos),true); g.setColour(ring); g.strokePath(arc,juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colour(0xffd8c7ff)); g.fillEllipse(c.x-3,c.y-3,6,6);
}
void PitchForgeLookAndFeel::drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool highlighted,bool)
{ auto r=b.getLocalBounds().toFloat().reduced(2); g.setColour(b.getToggleState()?juce::Colour(0xff8f62ff):juce::Colour(0xff303540)); g.fillRoundedRectangle(r,8); g.setColour(juce::Colours::white.withAlpha(0.88f)); g.setFont(12); g.drawText(b.getButtonText(),r,juce::Justification::centred); }

PitchForgeAudioProcessorEditor::PitchForgeAudioProcessorEditor(PitchForgeAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lf);
    setResizable(true, true);
    setSize(1040, 660);
    setResizeLimits(900, 580, 1500, 900);

    title.setText("PITCHFORGE", juce::dontSendNotification);
    title.setFont(juce::Font(18.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    subtitle.setText("PRECISION VOCAL TUNING  •  PRO", juce::dontSendNotification);
    subtitle.setFont(juce::Font(10.0f));
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff9fa5b1));
    addAndMakeVisible(subtitle);

    addKnob(speed, "Speed", -3, 200, 20);
    addKnob(amount, "Amount", 0, 1, 1);
    addKnob(sustain, "Sustain", -1, 1, 0);
    addKnob(mix, "Mix", 0, 1, 1);
    addKnob(humanize, "Humanize", 0, 1, 0.10);
    addKnob(range, "Range", 0, 12, 12);

    speed.setTextValueSuffix(" ms");
    amount.textFromValueFunction = [](double v){ return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
    amount.valueFromTextFunction = [](const juce::String& t){ return t.getFloatValue() * 0.01; };
    sustain.textFromValueFunction = [](double v){ return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
    sustain.valueFromTextFunction = [](const juce::String& t){ return t.getFloatValue() * 0.01; };
    mix.textFromValueFunction = [](double v){ return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
    mix.valueFromTextFunction = [](const juce::String& t){ return t.getFloatValue() * 0.01; };
    humanize.textFromValueFunction = [](double v){ return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
    humanize.valueFromTextFunction = [](const juce::String& t){ return t.getFloatValue() * 0.01; };
    range.setTextValueSuffix(" st");

    scale.addItem("Chromatic", 1);
    scale.addItem("Major", 2);
    scale.addItem("Minor", 3);
    addAndMakeVisible(scale);
    stabilizer.addItem("None", 1);
    stabilizer.addItem("Short", 2);
    stabilizer.addItem("Mid", 3);
    stabilizer.addItem("Long", 4);
    addAndMakeVisible(stabilizer);
    for (int i = 0; i < 12; ++i) key.addItem(noteNames[i], i + 1);
    addAndMakeVisible(key);

    auto addToggle = [&](juce::ToggleButton& b, const juce::String& text)
    {
        b.setButtonText(text);
        b.setLookAndFeel(&lf);
        addAndMakeVisible(b);
    };
    addToggle(chromatic, "Chromatic");
    addToggle(lowLatency, "Low Latency");
    addToggle(detected, "Detected Notes");
    addToggle(heatmap, "HeatMap");
    addToggle(doubler, "Doubler");

    noteMode.setButtonText("Note");
    majorMode.setButtonText("Major");
    minorMode.setButtonText("Minor");
    holdMajor.setButtonText("Hold");
    holdMinor.setButtonText("Hold");
    resetButton.setButtonText("RESET");
    for (auto* b : { &noteMode, &majorMode, &minorMode, &holdMajor, &holdMinor, &resetButton })
        addAndMakeVisible(b);

    noteMode.onClick = [this]{ setScaleMode(0); };
    majorMode.onClick = [this]{ setScaleMode(1); };
    minorMode.onClick = [this]{ setScaleMode(2); };
    holdMajor.onClick = [this]{ stabilizer.setSelectedId(3, juce::sendNotificationSync); };
    holdMinor.onClick = [this]{ stabilizer.setSelectedId(4, juce::sendNotificationSync); };
    resetButton.onClick = [this]
    {
        speed.setValue(20.0, juce::sendNotificationSync);
        amount.setValue(1.0, juce::sendNotificationSync);
        sustain.setValue(0.0, juce::sendNotificationSync);
        mix.setValue(1.0, juce::sendNotificationSync);
        humanize.setValue(0.10, juce::sendNotificationSync);
        range.setValue(12.0, juce::sendNotificationSync);
    };

    pitchRef.setRange(430, 450, 0.01);
    pitchRef.setTextValueSuffix(" Hz");
    pitchRef.setSliderStyle(juce::Slider::LinearHorizontal);
    addAndMakeVisible(pitchRef);
    width.setRange(0, 100, 0.1);
    width.setValue(50);
    width.setTextValueSuffix(" %");
    addAndMakeVisible(width);
    doublerMix.setRange(0, 100, 0.1);
    doublerMix.setValue(0);
    doublerMix.setTextValueSuffix(" %");
    addAndMakeVisible(doublerMix);

    latencyLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb9bec9));
    confidenceLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb9bec9));
    pitchLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    outputLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaeb4c0));
    addAndMakeVisible(latencyLabel);
    addAndMakeVisible(confidenceLabel);
    addAndMakeVisible(pitchLabel);
    addAndMakeVisible(outputLabel);

    speedAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(), "speed", speed);
    amountAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(), "amount", amount);
    sustainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(), "sustain", sustain);
    mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(), "mix", mix);
    humanizeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(), "humanize", humanize);
    rangeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(), "range", range);
    refAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(), "pitchReference", pitchRef);
    widthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(), "doublerWidth", width);
    doublerMixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(), "doublerMix", doublerMix);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.getAPVTS(), "scale", scale);
    stabilizerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.getAPVTS(), "stabilizer", stabilizer);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.getAPVTS(), "key", key);
    chromAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.getAPVTS(), "chromatic", chromatic);
    latencyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.getAPVTS(), "lowLatency", lowLatency);
    detectedAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.getAPVTS(), "detectedNotes", detected);
    heatmapAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.getAPVTS(), "heatMap", heatmap);
    doublerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.getAPVTS(), "doubler", doubler);
    startTimerHz(30);
}

PitchForgeAudioProcessorEditor::~PitchForgeAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void PitchForgeAudioProcessorEditor::addKnob(juce::Slider& s, const juce::String&, float lo, float hi, float val)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 18);
    s.setRange(lo, hi, 0.01);
    s.setValue(val);
    s.setLookAndFeel(&lf);
    addAndMakeVisible(s);
}

void PitchForgeAudioProcessorEditor::setScaleMode(int mode)
{
    scale.setSelectedId(mode + 1, juce::sendNotificationSync);
}

void PitchForgeAudioProcessorEditor::drawHistory(juce::Graphics& g, juce::Rectangle<float> area, const std::array<float,72>& history, juce::Colour lineColour)
{
    g.setColour(juce::Colour(0xff11151b));
    g.fillRoundedRectangle(area, 10.0f);
    g.setColour(juce::Colour(0xff252b34));
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine((int) (area.getY() + area.getHeight() * i / 4.0f), area.getX(), area.getRight());

    juce::Path path;
    bool started = false;
    for (size_t i = 0; i < history.size(); ++i)
    {
        const size_t idx = (historyPos + i) % history.size();
        const float v = history[idx];
        if (v <= 0.0f) continue;
        const float x = area.getX() + area.getWidth() * (float) i / (float) (history.size() - 1);
        const float y = area.getCentreY() - juce::jlimit(-1.0f, 1.0f, v) * area.getHeight() * 0.38f;
        if (!started) { path.startNewSubPath(x, y); started = true; }
        else path.lineTo(x, y);
    }
    if (started)
    {
        g.setColour(lineColour);
        g.strokePath(path, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved));
    }
}

void PitchForgeAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff06080b));
    auto bounds = getLocalBounds().toFloat().reduced(10.0f);
    g.setColour(juce::Colour(0xff14171d));
    g.fillRoundedRectangle(bounds, 18.0f);

    g.setColour(juce::Colour(0xff0c0f14));
    g.fillRoundedRectangle(bounds.withTrimmedTop(48.0f), 16.0f);

    // Header
    g.setColour(juce::Colour(0xff252a33));
    g.fillRoundedRectangle(25.0f, 58.0f, 100.0f, 3.0f, 2.0f);
    g.setColour(juce::Colour(0xff858b98));
    g.setFont(9.0f);
    g.drawText("INPUT", 34, 68, 70, 16, juce::Justification::left);
    g.drawText("OUTPUT", 110, 68, 70, 16, juce::Justification::left);

    // Hero tuner orb.
    const auto orb = juce::Rectangle<float>(30, 92, 520, 320);
    const auto c = orb.getCentre();
    const float r = 116.0f;
    for (int i = 7; i >= 0; --i)
    {
        g.setColour(juce::Colour(0xff8d59e8).withAlpha(0.018f * (float) (8 - i)));
        g.fillEllipse(c.x - r - i * 8.0f, c.y - r - i * 8.0f, 2.0f * (r + i * 8.0f), 2.0f * (r + i * 8.0f));
    }
    g.setColour(juce::Colour(0xff2b3038));
    g.drawEllipse(c.x-r, c.y-r, 2*r, 2*r, 2.0f);
    g.setColour(juce::Colour(0xff9b62f5));
    g.drawEllipse(c.x-r+10, c.y-r+10, 2*(r-10), 2*(r-10), 4.0f);

    const float cents = processor.getCorrectionCents();
    const float normalized = juce::jlimit(-1.0f, 1.0f, cents / 50.0f);
    const float angle = juce::MathConstants<float>::halfPi + normalized * 1.15f;
    juce::Path needle;
    needle.startNewSubPath(c.x, c.y);
    needle.lineTo(c.x + std::cos(angle) * 88.0f, c.y - std::sin(angle) * 88.0f);
    g.setColour(juce::Colour(0xff7edcff));
    g.strokePath(needle, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillEllipse(c.x-5, c.y-5, 10, 10);

    const int midi = processor.getDetectedMidi();
    const juce::String note = midi >= 0 ? noteNames[(midi % 12 + 12) % 12] : "--";
    g.setColour(juce::Colours::white);
    g.setFont(46.0f);
    g.drawText(note, c.x - 80, c.y + 35, 160, 58, juce::Justification::centred);
    g.setColour(juce::Colour(0xff9ea5b1));
    g.setFont(11.0f);
    g.drawText("DETECTED", c.x - 60, c.y + 94, 120, 16, juce::Justification::centred);

    // Lower visual heatmap.
    g.setColour(juce::Colour(0xff20242c));
    g.fillRoundedRectangle(30.0f, 432.0f, 520.0f, 86.0f, 12.0f);
    g.setColour(juce::Colour(0xff8f5bf0).withAlpha(0.35f));
    for (int i = 0; i < 56; ++i)
    {
        const float h = 6.0f + 28.0f * (0.5f + 0.5f * std::sin((float) i * 0.67f + (float) historyPos * 0.03f));
        g.fillRoundedRectangle(38.0f + i * 8.7f, 510.0f - h, 5.0f, h, 2.0f);
    }
    g.setColour(juce::Colour(0xffd7dbe4));
    g.setFont(9.0f);
    g.drawText("INPUT / OUTPUT HEATMAP", 42, 442, 200, 15, juce::Justification::left);

    // Right-side control tower background.
    g.setColour(juce::Colour(0xff11151b));
    g.fillRoundedRectangle(565.0f, 78.0f, 450.0f, 440.0f, 18.0f);
    g.setColour(juce::Colour(0xff252a33));
    g.fillRoundedRectangle(585.0f, 94.0f, 410.0f, 42.0f, 10.0f);
    g.setColour(juce::Colour(0xffa366ff));
    g.fillRoundedRectangle(600.0f, 130.0f, 380.0f, 3.0f, 2.0f);

    g.setColour(juce::Colour(0xff9ba1ad));
    g.setFont(9.0f);
    g.drawText("KEY", 585, 150, 50, 15, juce::Justification::left);
    g.drawText("SCALE", 700, 150, 60, 15, juce::Justification::left);
    g.drawText("NOTE STABILIZER", 815, 150, 130, 15, juce::Justification::left);
    g.drawText("TRACKING", 585, 268, 80, 15, juce::Justification::left);
    g.drawText("PITCH REFERENCE", 585, 350, 120, 15, juce::Justification::left);
    g.drawText("DOUBLER", 585, 420, 80, 15, juce::Justification::left);

    g.setColour(juce::Colour(0xffaeb4c0));
    g.setFont(10.0f);
    g.drawText("INPUT  " + juce::String(processor.getInputPitchHz(), 1) + " Hz", 585, 285, 190, 18, juce::Justification::left);
    g.drawText("OUTPUT  " + juce::String(processor.getOutputPitchHz(), 1) + " Hz", 785, 285, 190, 18, juce::Justification::right);

    // Keyboard along the bottom.
    drawKeyboard(g, juce::Rectangle<int>(30, 530, 520, 82));
}

void PitchForgeAudioProcessorEditor::drawKeyboard(juce::Graphics& g, juce::Rectangle<int> area)
{
    const int whiteW = area.getWidth() / 7;
    static const int blackPos[] = { 1, 2, 4, 5, 6 };
    const int selected = processor.getTargetMidi();
    for (int i = 0; i < 7; ++i)
    {
        const int pc = (i * 2 + 2) % 12;
        const bool on = selected >= 0 && selected % 12 == pc;
        g.setColour(on ? juce::Colour(0xff8d5bf3) : juce::Colour(0xffc5c8d0));
        g.fillRoundedRectangle((float) area.getX() + i * whiteW, (float) area.getY(), whiteW - 2.0f, (float) area.getHeight(), 3.0f);
        g.setColour(on ? juce::Colours::white : juce::Colour(0xff252832));
        g.setFont(9.0f);
        g.drawText(noteNames[pc], area.getX() + i * whiteW, area.getBottom() - 18, whiteW, 12, juce::Justification::centred);
    }
    for (int p : blackPos)
    {
        const int x = area.getX() + p * whiteW - whiteW / 4;
        g.setColour(juce::Colour(0xff191c22));
        g.fillRoundedRectangle((float) x, (float) area.getY(), whiteW / 2.0f, area.getHeight() * 0.60f, 3.0f);
    }
}

void PitchForgeAudioProcessorEditor::resized()
{
    const int w = getWidth();
    title.setBounds(28, 18, 150, 24);
    subtitle.setBounds(180, 21, 250, 18);
    resetButton.setBounds(w - 118, 17, 90, 26);

    speed.setBounds(w - 430, 78, 118, 122);
    amount.setBounds(w - 295, 78, 118, 122);
    sustain.setBounds(w - 155, 88, 105, 105);
    mix.setBounds(w - 420, 190, 100, 100);
    humanize.setBounds(w - 305, 190, 100, 100);
    range.setBounds(w - 190, 190, 100, 100);

    key.setBounds(w - 430, 150, 95, 26);
    scale.setBounds(w - 320, 150, 105, 26);
    stabilizer.setBounds(w - 200, 150, 150, 26);

    chromatic.setBounds(w - 430, 300, 105, 26);
    lowLatency.setBounds(w - 315, 300, 105, 26);
    detected.setBounds(w - 200, 300, 105, 26);
    heatmap.setBounds(w - 430, 332, 105, 26);
    doubler.setBounds(w - 315, 332, 105, 26);
    latencyLabel.setBounds(w - 200, 332, 150, 22);
    confidenceLabel.setBounds(w - 430, 365, 300, 22);

    pitchRef.setBounds(w - 430, 385, 380, 22);
    width.setBounds(w - 420, 430, 100, 62);
    doublerMix.setBounds(w - 300, 430, 100, 62);
    pitchLabel.setBounds(w - 180, 430, 130, 22);
    outputLabel.setBounds(w - 180, 456, 130, 22);

    noteMode.setBounds(w - 430, 118, 55, 24);
    majorMode.setBounds(w - 370, 118, 60, 24);
    minorMode.setBounds(w - 305, 118, 60, 24);
    holdMajor.setBounds(w - 240, 118, 55, 24);
    holdMinor.setBounds(w - 180, 118, 55, 24);
}

void PitchForgeAudioProcessorEditor::timerCallback()
{
    const float conf = processor.getConfidence();
    const float cents = processor.getCorrectionCents();
    confidenceLabel.setText("Confidence  " + juce::String(conf * 100.0f, 0) + "%    Correction  " + juce::String(cents, 1) + " cents", juce::dontSendNotification);
    const bool ll = processor.getAPVTS().getRawParameterValue("lowLatency")->load() > 0.5f;
    const int stab = (int) processor.getAPVTS().getRawParameterValue("stabilizer")->load();
    latencyLabel.setText(ll ? "LOW LATENCY" : (stab > 0 ? "STABILIZER" : "TRACKING"), juce::dontSendNotification);
    pitchLabel.setText("Pitch  " + juce::String(processor.getInputPitchHz(), 1) + " Hz", juce::dontSendNotification);
    outputLabel.setText("Out  " + juce::String(processor.getOutputPitchHz(), 1) + " Hz", juce::dontSendNotification);

    inputHistory[historyPos] = processor.getInputPitchHz() > 0.0f ? juce::jlimit(-1.0f, 1.0f, processor.getCorrectionCents() / 50.0f) : 0.0f;
    outputHistory[historyPos] = processor.getOutputPitchHz() > 0.0f ? juce::jlimit(-1.0f, 1.0f, processor.getCorrectionCents() / 50.0f) : 0.0f;
    historyPos = (historyPos + 1) % (int) inputHistory.size();
    repaint();
}
