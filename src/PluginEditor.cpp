#include "PluginEditor.h"
#include <cmath>

namespace
{
constexpr const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const juce::Colour bg(0xff07090d);
const juce::Colour panel(0xff10141b);
const juce::Colour panel2(0xff141922);
const juce::Colour border(0xff252c37);
const juce::Colour text(0xfff4f5f8);
const juce::Colour muted(0xff8992a1);
const juce::Colour accent(0xffb18cff);
const juce::Colour cyan(0xff75e7ff);
const juce::Colour green(0xff63e6a4);
}

PitchForgeLookAndFeel::PitchForgeLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0a0d12));
    setColour(juce::Slider::textBoxOutlineColourId, border);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0b0f15));
    setColour(juce::ComboBox::outlineColourId, border);
    setColour(juce::ComboBox::textColourId, text);
}

void PitchForgeLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                              float pos, float start, float end, juce::Slider& slider)
{
    auto b = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h).reduced(7.0f);
    const auto c = b.getCentre();
    const float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.40f;
    const float arcStart = juce::MathConstants<float>::pi * 1.25f;
    const float arcEnd = juce::MathConstants<float>::pi * 2.75f;

    g.setColour(juce::Colour(0xff0a0d12));
    g.fillEllipse(c.x - r - 5.0f, c.y - r - 5.0f, (r + 5.0f) * 2.0f, (r + 5.0f) * 2.0f);
    g.setColour(border);
    g.drawEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f, 2.0f);

    juce::Path track;
    track.addCentredArc(c.x, c.y, r, r, 0.0f, arcStart, arcEnd, true);
    g.setColour(juce::Colour(0xff303744));
    g.strokePath(track, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc(c.x, c.y, r, r, 0.0f, arcStart, arcStart + (arcEnd - arcStart) * pos, true);
    g.setColour(accent);
    g.strokePath(value, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float angle = arcStart + (arcEnd - arcStart) * pos;
    const float px = c.x + std::cos(angle) * (r - 2.0f);
    const float py = c.y + std::sin(angle) * (r - 2.0f);
    g.setColour(cyan);
    g.fillEllipse(px - 3.0f, py - 3.0f, 6.0f, 6.0f);
    juce::ignoreUnused(start, end, slider);
}

void PitchForgeLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    const bool on = b.getToggleState();
    g.setColour(on ? juce::Colour(0xff2a2042) : juce::Colour(0xff0b0f15));
    g.fillRoundedRectangle(r, 9.0f);
    g.setColour(on ? accent : border);
    g.drawRoundedRectangle(r, 9.0f, 1.0f);
    g.setColour(on ? text : muted);
    g.setFont(12.0f);
    g.drawText(b.getButtonText(), r.reduced(9.0f, 0.0f), juce::Justification::centredLeft);
    const float d = 8.0f;
    const float cx = r.getRight() - 15.0f;
    const float cy = r.getCentreY();
    g.setColour(on ? cyan : juce::Colour(0xff3a424f));
    g.fillEllipse(cx - d * 0.5f, cy - d * 0.5f, d, d);
    juce::ignoreUnused(highlighted);
}

PitchForgeAudioProcessorEditor::PitchForgeAudioProcessorEditor(PitchForgeAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lf);
    setResizable(true, true);
    setSize(1400, 900);
    setResizeLimits(1120, 760, 1920, 1200);

    title.setText("PITCHFORGE", juce::dontSendNotification);
    title.setFont(juce::Font(22.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, text);
    addAndMakeVisible(title);

    subtitle.setText("PRECISION VOCAL TUNING  /  PROFESSIONAL EDITION", juce::dontSendNotification);
    subtitle.setFont(juce::Font(10.0f, juce::Font::bold));
    subtitle.setColour(juce::Label::textColourId, muted);
    addAndMakeVisible(subtitle);

    addKnob(speed, "Speed", -3.0f, 200.0f, 20.0f);
    addKnob(amount, "Amount", 0.0f, 1.0f, 1.0f);
    addKnob(sustain, "Sustain", -1.0f, 1.0f, 0.0f);
    addKnob(mix, "Mix", 0.0f, 1.0f, 1.0f);
    addKnob(humanize, "Humanize", 0.0f, 1.0f, 0.10f);
    addKnob(range, "Range", 0.0f, 12.0f, 12.0f);

    speed.setTooltip("RETUNE SPEED — how quickly the correction moves toward the target note. Lower = smoother, higher = tighter.");
    amount.setTooltip("CORRECTION — how much of the detected pitch error is corrected. 100% = full correction.");
    sustain.setTooltip("SUSTAIN — stabilizes held notes and prevents rapid target changes.");
    mix.setTooltip("WET / DRY — blend between the corrected vocal and the original vocal.");
    humanize.setTooltip("HUMANIZE — adds controlled natural movement so correction does not sound unnaturally static.");
    range.setTooltip("MAX CORRECTION — the maximum number of semitones PitchForge may move a detected note.");

    speed.setTextValueSuffix(" ms");
    amount.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
    amount.valueFromTextFunction = [](const juce::String& t) { return t.getFloatValue() * 0.01; };
    sustain.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
    sustain.valueFromTextFunction = [](const juce::String& t) { return t.getFloatValue() * 0.01; };
    mix.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
    mix.valueFromTextFunction = [](const juce::String& t) { return t.getFloatValue() * 0.01; };
    humanize.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
    humanize.valueFromTextFunction = [](const juce::String& t) { return t.getFloatValue() * 0.01; };
    range.setTextValueSuffix(" st");

    scale.addItem("Chromatic", 1); scale.addItem("Major", 2); scale.addItem("Minor", 3);
    stabilizer.addItem("None", 1); stabilizer.addItem("Short", 2); stabilizer.addItem("Mid", 3); stabilizer.addItem("Long", 4);
    for (int i = 0; i < 12; ++i) key.addItem(noteNames[i], i + 1);
    for (auto* c : { &scale, &stabilizer, &key }) addAndMakeVisible(*c);

    auto addToggle = [&](juce::ToggleButton& b, const juce::String& label)
    {
        b.setButtonText(label); b.setLookAndFeel(&lf); addAndMakeVisible(b);
    };
    addToggle(chromatic, "Chromatic");
    addToggle(lowLatency, "Low Latency");
    addToggle(detected, "Detected Notes");
    addToggle(heatmap, "HeatMap");
    addToggle(doubler, "Doubler");

    noteMode.setButtonText("Chromatic"); majorMode.setButtonText("Major"); minorMode.setButtonText("Minor");
    holdMajor.setButtonText("Hold Major"); holdMinor.setButtonText("Hold Minor"); resetButton.setButtonText("RESET");
    for (auto* b : { &noteMode, &majorMode, &minorMode, &holdMajor, &holdMinor, &resetButton }) addAndMakeVisible(b);
    noteMode.onClick = [this] { setScaleMode(0); };
    majorMode.onClick = [this] { setScaleMode(1); };
    minorMode.onClick = [this] { setScaleMode(2); };
    holdMajor.onClick = [this] { stabilizer.setSelectedId(3, juce::sendNotificationSync); };
    holdMinor.onClick = [this] { stabilizer.setSelectedId(4, juce::sendNotificationSync); };
    resetButton.onClick = [this]
    {
        speed.setValue(20.0, juce::sendNotificationSync); amount.setValue(1.0, juce::sendNotificationSync);
        sustain.setValue(0.0, juce::sendNotificationSync); mix.setValue(1.0, juce::sendNotificationSync);
        humanize.setValue(0.10, juce::sendNotificationSync); range.setValue(12.0, juce::sendNotificationSync);
        pitchRef.setValue(440.0, juce::sendNotificationSync); width.setValue(50.0, juce::sendNotificationSync);
        doublerMix.setValue(0.0, juce::sendNotificationSync);
    };

    pitchRef.setRange(430.0, 450.0, 0.01); pitchRef.setTextValueSuffix(" Hz");
    pitchRef.setSliderStyle(juce::Slider::LinearHorizontal); pitchRef.setTextBoxStyle(juce::Slider::TextBoxRight, false, 78, 24);
    width.setRange(0.0, 100.0, 0.1); width.setTextValueSuffix(" %");
    doublerMix.setRange(0.0, 100.0, 0.1); doublerMix.setTextValueSuffix(" %");
    addAndMakeVisible(pitchRef); addAndMakeVisible(width); addAndMakeVisible(doublerMix);

    for (auto* l : { &latencyLabel, &confidenceLabel, &pitchLabel, &outputLabel }) addAndMakeVisible(*l);
    latencyLabel.setColour(juce::Label::textColourId, cyan);
    confidenceLabel.setColour(juce::Label::textColourId, muted);
    pitchLabel.setColour(juce::Label::textColourId, text);
    outputLabel.setColour(juce::Label::textColourId, muted);

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

PitchForgeAudioProcessorEditor::~PitchForgeAudioProcessorEditor() { setLookAndFeel(nullptr); }

void PitchForgeAudioProcessorEditor::addKnob(juce::Slider& s, const juce::String& label, float lo, float hi, float val)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 92, 24);
    s.setRange(lo, hi, 0.01); s.setValue(val); s.setName(label); s.setLookAndFeel(&lf); addAndMakeVisible(s);
}

void PitchForgeAudioProcessorEditor::setScaleMode(int mode) { scale.setSelectedId(mode + 1, juce::sendNotificationSync); }

void PitchForgeAudioProcessorEditor::drawHistory(juce::Graphics& g, juce::Rectangle<float> area,
                                                 const std::array<float,72>& history, juce::Colour lineColour)
{
    g.setColour(juce::Colour(0xff0b0f15)); g.fillRoundedRectangle(area, 12.0f);
    g.setColour(juce::Colour(0xff202631));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine((int)(area.getY() + area.getHeight() * i / 4.0f), area.getX(), area.getRight());
    juce::Path p; bool started = false;
    for (size_t i = 0; i < history.size(); ++i)
    {
        const auto idx = (historyPos + i) % history.size();
        const float v = juce::jlimit(-1.0f, 1.0f, history[idx]);
        if (v == 0.0f) continue;
        const float x = area.getX() + area.getWidth() * (float)i / (float)(history.size() - 1);
        const float y = area.getCentreY() - v * area.getHeight() * 0.38f;
        if (!started) { p.startNewSubPath(x, y); started = true; } else p.lineTo(x, y);
    }
    if (started) { g.setColour(lineColour); g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved)); }
}

void PitchForgeAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll(bg);

    auto frame = bounds.reduced(14.0f);
    g.setColour(panel);
    g.fillRoundedRectangle(frame, 20.0f);
    g.setColour(border);
    g.drawRoundedRectangle(frame, 20.0f, 1.0f);

    // Header
    auto header = frame.reduced(12.0f).removeFromTop(58.0f);
    g.setColour(juce::Colour(0xff0b0f15));
    g.fillRoundedRectangle(header, 14.0f);
    g.setColour(accent);
    g.fillRoundedRectangle(header.getX() + 18.0f, header.getBottom() - 3.0f, 108.0f, 3.0f, 2.0f);

    // Main panels
    const int gap = 12;
    auto content = frame.reduced(12.0f);
    content.removeFromTop(70);
    auto bottom = content.removeFromBottom(156);
    content.removeFromBottom(gap);
    auto left = content.removeFromLeft((int)(content.getWidth() * 0.62f));
    content.removeFromLeft(gap);
    auto right = content;

    auto drawPanel = [&g](juce::Rectangle<float> r)
    {
        g.setColour(juce::Colour(0xff0c1016));
        g.fillRoundedRectangle(r, 15.0f);
        g.setColour(border);
        g.drawRoundedRectangle(r, 15.0f, 1.0f);
    };

    drawPanel(left);
    drawPanel(right);
    drawPanel(bottom);

    // Left: tuner / pitch tracking
    g.setColour(muted);
    g.setFont(9.0f);
    g.drawText("PITCH TRACKING", left.getX() + 18.0f, left.getY() + 12.0f, 180.0f, 14.0f, juce::Justification::left);
    g.setColour(text);
    g.setFont(16.0f);
    g.drawText("Live tuner", left.getX() + 18.0f, left.getY() + 27.0f, 180.0f, 22.0f, juce::Justification::left);

    const auto tunerArea = juce::Rectangle<float>(left.getX() + 18.0f, left.getY() + 54.0f,
                                                   left.getWidth() - 36.0f, 150.0f);
    const auto c = tunerArea.getCentre();
    const float r = juce::jmin(tunerArea.getWidth(), tunerArea.getHeight()) * 0.34f;
    g.setColour(juce::Colour(0xff151b24));
    g.fillEllipse(c.x - r - 10.0f, c.y - r - 10.0f, (r + 10.0f) * 2.0f, (r + 10.0f) * 2.0f);
    g.setColour(juce::Colour(0xff242c38));
    g.drawEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f, 2.0f);
    g.setColour(accent.withAlpha(0.35f));
    g.drawEllipse(c.x - r + 8.0f, c.y - r + 8.0f, (r - 8.0f) * 2.0f, (r - 8.0f) * 2.0f, 3.0f);

    // Premium control wells behind each rotary control.
    for (auto* knob : { &speed, &amount, &sustain, &mix, &humanize, &range })
    {
        auto rr = knob->getBounds().toFloat().expanded(8.0f, 12.0f);
        g.setColour(panel2); g.fillRoundedRectangle(rr, 12.0f);
        g.setColour(border); g.drawRoundedRectangle(rr, 12.0f, 1.0f);
    }

    const float cents = processor.getCorrectionCents();
    const float normalized = juce::jlimit(-1.0f, 1.0f, cents / 50.0f);
    const float angle = juce::MathConstants<float>::halfPi + normalized * 1.15f;
    juce::Path needle;
    needle.startNewSubPath(c.x, c.y);
    needle.lineTo(c.x + std::cos(angle) * r * 0.76f, c.y - std::sin(angle) * r * 0.76f);
    g.setColour(cyan);
    g.strokePath(needle, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillEllipse(c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);

    const int midi = processor.getDetectedMidi();
    const juce::String note = midi >= 0 ? noteNames[(midi % 12 + 12) % 12] : "--";
    g.setColour(text);
    g.setFont(42.0f);
    g.drawText(note, c.x - 70.0f, c.y + 25.0f, 140.0f, 52.0f, juce::Justification::centred);
    g.setColour(muted);
    g.setFont(9.0f);
    g.drawText("DETECTED NOTE", c.x - 80.0f, c.y + 72.0f, 160.0f, 15.0f, juce::Justification::centred);

    // Plain-English control map: always visible, so the user never has to guess what a knob means.
    struct ControlInfo { juce::Slider* slider; const char* title; const char* body; };
    const ControlInfo controls[] = {
        { &speed, "RETUNE SPEED", "How fast pitch moves" },
        { &amount, "CORRECTION", "How much error is fixed" },
        { &sustain, "SUSTAIN", "Stability for held notes" },
        { &mix, "WET / DRY", "Corrected vs original" },
        { &humanize, "HUMANIZE", "Keeps movement natural" },
        { &range, "MAX CORRECTION", "Largest allowed move" }
    };
    for (auto& cInfo : controls)
    {
        auto rr = cInfo.slider->getBounds().toFloat().expanded(4.0f, 10.0f);
        g.setColour(text); g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(cInfo.title, rr.getX(), rr.getY() - 14.0f, rr.getWidth(), 13.0f, juce::Justification::centred);
        g.setColour(muted); g.setFont(8.5f);
        g.drawText(cInfo.body, rr.getX(), rr.getBottom() - 12.0f, rr.getWidth(), 12.0f, juce::Justification::centred);
    }

    // Live graph has its own reserved rectangle: never overlaps the knobs.
    const float graphHeight = 70.0f;
    auto graph = juce::Rectangle<float>(left.getX() + 18.0f, left.getBottom() - graphHeight - 18.0f,
                                         left.getWidth() - 36.0f, graphHeight);
    drawHistory(g, graph, inputHistory, cyan);
    g.setColour(muted);
    g.setFont(9.0f);
    g.drawText("LIVE CORRECTION HISTORY", graph.getX() + 10.0f, graph.getY() + 7.0f,
               190.0f, 14.0f, juce::Justification::left);

    // Right: tuning controls with explicit rows and no hard-coded overflow.
    g.setColour(muted); g.setFont(9.0f);
    g.drawText("CORRECTION ENGINE", right.getX() + 18.0f, right.getY() + 12.0f, 180.0f, 14.0f, juce::Justification::left);
    g.setColour(text); g.setFont(16.0f);
    g.drawText("Core tuning", right.getX() + 18.0f, right.getY() + 27.0f, 180.0f, 22.0f, juce::Justification::left);

    g.setColour(border);
    g.drawHorizontalLine((int)right.getY() + 60, right.getX() + 18.0f, right.getRight() - 18.0f);
    g.setColour(muted); g.setFont(8.5f);
    g.drawText("KEY", key.getX(), key.getY() - 14.0f, key.getWidth(), 12.0f, juce::Justification::left);
    g.drawText("SCALE", scale.getX(), scale.getY() - 14.0f, scale.getWidth(), 12.0f, juce::Justification::left);
    g.drawText("STABILIZER", stabilizer.getX(), stabilizer.getY() - 14.0f, stabilizer.getWidth(), 12.0f, juce::Justification::left);

    g.setColour(muted); g.setFont(8.5f);
    g.drawText("ENGINE MODES", right.getX() + 18.0f, right.getY() + 160.0f, 140.0f, 13.0f, juce::Justification::left);
    g.setColour(border);
    g.drawHorizontalLine((int)right.getY() + 274, right.getX() + 18.0f, right.getRight() - 18.0f);

    g.setColour(muted); g.setFont(8.5f);
    g.drawText("SIGNAL STATUS", right.getX() + 18.0f, right.getY() + 288.0f, 140.0f, 13.0f, juce::Justification::left);

    // Bottom: precision / performance controls.
    g.setColour(muted); g.setFont(9.0f);
    g.drawText("PRECISION & PERFORMANCE", bottom.getX() + 18.0f, bottom.getY() + 12.0f, 220.0f, 14.0f, juce::Justification::left);
    g.setColour(text); g.setFont(15.0f);
    g.drawText("Fine control", bottom.getX() + 18.0f, bottom.getY() + 27.0f, 180.0f, 20.0f, juce::Justification::left);

    g.setColour(border);
    g.drawVerticalLine((int)(bottom.getX() + bottom.getWidth() * 0.58f), bottom.getY() + 14.0f, bottom.getBottom() - 14.0f);
    g.setColour(muted); g.setFont(8.5f);
    g.drawText("PITCH REFERENCE", pitchRef.getX(), pitchRef.getY() - 14.0f, pitchRef.getWidth() - 88.0f, 12.0f, juce::Justification::left);
    g.drawText("DOUBLER WIDTH", width.getX(), width.getY() - 14.0f, width.getWidth(), 12.0f, juce::Justification::centred);
    g.drawText("DOUBLER MIX", doublerMix.getX(), doublerMix.getY() - 14.0f, doublerMix.getWidth(), 12.0f, juce::Justification::centred);
}

void PitchForgeAudioProcessorEditor::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    const int outer = 26;
    const int gap = 12;

    title.setBounds(outer + 4, 28, 190, 28);
    subtitle.setBounds(outer + 204, 32, 420, 20);
    resetButton.setBounds(w - outer - 96, 28, 96, 30);

    const int headerBottom = 96;
    const int bottomH = 156;
    const int contentBottom = h - 28;
    const int mainBottom = contentBottom - bottomH - gap;
    const int x = outer;
    const int totalW = w - outer * 2;
    const int leftW = (int)(totalW * 0.62f);
    const int rightX = x + leftW + gap;
    const int rightW = w - outer - rightX;
    const int mainY = headerBottom;
    const int mainH = mainBottom - mainY;

    // Left panel: tuner, six knobs, and graph each own a dedicated vertical lane.
    const int knobAreaY = mainY + 214;
    const int knobAreaH = juce::jmax(160, mainH - 310);
    const int colGap = 8;
    const int innerX = x + 18;
    const int innerW = leftW - 36;
    const int colW = (innerW - colGap * 2) / 3;
    const int rowGap = 4;
    const int rowH = juce::jmax(78, (knobAreaH - rowGap) / 2);

    const int col0 = innerX;
    const int col1 = innerX + colW + colGap;
    const int col2 = innerX + (colW + colGap) * 2;
    const int row0 = knobAreaY;
    const int row1 = knobAreaY + rowH + rowGap;

    speed.setBounds(col0, row0 + 10, colW, rowH - 10);
    amount.setBounds(col1, row0 + 10, colW, rowH - 10);
    sustain.setBounds(col2, row0 + 10, colW, rowH - 10);
    mix.setBounds(col0, row1 + 10, colW, rowH - 10);
    humanize.setBounds(col1, row1 + 10, colW, rowH - 10);
    range.setBounds(col2, row1 + 10, colW, rowH - 10);

    // Right panel: three selector columns, two mode rows, then signal readout.
    const int rx = rightX + 18;
    const int rw = rightW - 36;
    const int selectorGap = 10;
    const int keyW = juce::jmax(72, (rw - selectorGap * 2) / 4);
    const int scaleW = juce::jmax(100, (rw - selectorGap * 2) / 3);
    const int stabX = rx + keyW + selectorGap + scaleW + selectorGap;
    const int stabW = rightX + rightW - 18 - stabX;
    const int selectorY = mainY + 86;

    key.setBounds(rx, selectorY, keyW, 32);
    scale.setBounds(rx + keyW + selectorGap, selectorY, scaleW, 32);
    stabilizer.setBounds(stabX, selectorY, juce::jmax(90, stabW), 32);

    const int toggleGap = 8;
    const int toggleW = (rw - toggleGap) / 2;
    const int toggleY = selectorY + 60;
    chromatic.setBounds(rx, toggleY, toggleW, 34);
    lowLatency.setBounds(rx + toggleW + toggleGap, toggleY, toggleW, 34);
    detected.setBounds(rx, toggleY + 42, toggleW, 34);
    heatmap.setBounds(rx + toggleW + toggleGap, toggleY + 42, toggleW, 34);
    doubler.setBounds(rx, toggleY + 84, toggleW, 34);
    latencyLabel.setBounds(rx + toggleW + toggleGap, toggleY + 84, toggleW, 34);

    confidenceLabel.setBounds(rx, toggleY + 130, rw, 22);
    pitchLabel.setBounds(rx, toggleY + 158, rw / 2 - 4, 22);
    outputLabel.setBounds(rx + rw / 2 + 4, toggleY + 158, rw / 2 - 4, 22);

    // Bottom panel.
    const int bottomY = mainBottom + gap;
    const int bottomX = x;
    const int bottomW = totalW;
    const int controlY = bottomY + 66;
    const int splitX = bottomX + (int)(bottomW * 0.58f);

    pitchRef.setBounds(bottomX + 18, controlY, splitX - bottomX - 48, 30);
    width.setBounds(splitX + 22, controlY, 118, 70);
    doublerMix.setBounds(splitX + 152, controlY, 118, 70);

    const int modeY = bottomY + bottomH - 42;
    int bx = bottomX + 18;
    noteMode.setBounds(bx, modeY, 100, 28); bx += 108;
    majorMode.setBounds(bx, modeY, 82, 28); bx += 90;
    minorMode.setBounds(bx, modeY, 82, 28); bx += 90;
    holdMajor.setBounds(bx, modeY, 108, 28); bx += 116;
    holdMinor.setBounds(bx, modeY, 108, 28);
}

void PitchForgeAudioProcessorEditor::timerCallback()
{
    const float conf = processor.getConfidence();
    const float cents = processor.getCorrectionCents();
    confidenceLabel.setText("Confidence  " + juce::String(conf * 100.0f, 0) + "%    •    Correction  " + juce::String(cents, 1) + " cents", juce::dontSendNotification);
    const bool ll = processor.getAPVTS().getRawParameterValue("lowLatency")->load() > 0.5f;
    const int stab = (int)processor.getAPVTS().getRawParameterValue("stabilizer")->load();
    latencyLabel.setText(ll ? "LOW LATENCY" : (stab > 0 ? "STABILIZED" : "TRACKING"), juce::dontSendNotification);
    pitchLabel.setText("In  " + juce::String(processor.getInputPitchHz(), 1) + " Hz", juce::dontSendNotification);
    outputLabel.setText("Out  " + juce::String(processor.getOutputPitchHz(), 1) + " Hz", juce::dontSendNotification);

    inputHistory[historyPos] = processor.getInputPitchHz() > 0.0f ? juce::jlimit(-1.0f, 1.0f, processor.getCorrectionCents() / 50.0f) : 0.0f;
    outputHistory[historyPos] = processor.getOutputPitchHz() > 0.0f ? juce::jlimit(-1.0f, 1.0f, processor.getCorrectionCents() / 50.0f) : 0.0f;
    historyPos = (historyPos + 1) % (int)inputHistory.size();
    repaint();
}
