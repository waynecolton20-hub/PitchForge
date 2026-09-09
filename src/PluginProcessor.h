#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

class PitchForgeAudioProcessor : public juce::AudioProcessor
{
public:
    PitchForgeAudioProcessor();
    ~PitchForgeAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PitchForge"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    float getInputPitchHz() const noexcept { return inputPitch.load(); }
    float getOutputPitchHz() const noexcept { return outputPitch.load(); }
    float getConfidence() const noexcept { return confidence.load(); }
    int getDetectedMidi() const noexcept { return detectedMidi.load(); }
    int getTargetMidi() const noexcept { return targetMidi.load(); }
    float getCorrectionCents() const noexcept { return correctionCents.load(); }

private:
    static constexpr int detectorSize = 2048;
    static constexpr int maxDelay = 32768;
    static constexpr int grainSize = 2048;
    static constexpr int grainHop = grainSize / 2;

    class PitchDetector
    {
    public:
        void prepare(double sampleRate);
        void reset();
        void push(float sample);
        float getPitchHz() const noexcept { return smoothedPitch; }
        float getConfidence() const noexcept { return confidence; }
    private:
        double fs = 44100.0;
        std::array<float, detectorSize> history{};
        int writePos = 0, sinceAnalysis = 0;
        float smoothedPitch = 0.0f, confidence = 0.0f;
        void analyse();
    };

    // Two-window, overlap-add time-domain pitch shifter. Unlike the old implementation,
    // both grains remain phase-continuous and are repositioned only at zero-crossing-style
    // overlap boundaries, which removes the repeated hard resets that caused chopping.
    class SmoothPitchShifter
    {
    public:
        void prepare(double sampleRate);
        void reset();
        float process(float input, float ratio);
        int getLatencySamples() const noexcept { return grainSize; }
        float getDelayedDry() const noexcept { return readAt(writePos - grainSize); }
    private:
        double fs = 44100.0;
        std::array<float, maxDelay> ring{};
        std::array<float, grainSize> window{};
        double readA = 0.0, readB = 0.0;
        int writePos = 0, phaseA = 0, phaseB = grainHop;
        bool ready = false;
        float smoothedRatio = 1.0f;
        double baseDelay = grainSize * 1.5;
        float readAt(double pos) const noexcept;
        static double wrap(double x);
    };

    juce::AudioProcessorValueTreeState apvts;
    double fs = 44100.0;
    int blockSize = 0;
    std::array<SmoothPitchShifter, 4> shifters;
    PitchDetector detector;

    float correctionSemitones = 0.0f;
    float heldSemitones = 0.0f;
    int stableBlocks = 0;
    int lastTargetMidiInternal = -1;

    std::atomic<float> inputPitch { 0.0f }, outputPitch { 0.0f }, confidence { 0.0f }, correctionCents { 0.0f };
    std::atomic<int> detectedMidi { -1 }, targetMidi { -1 };

    float midiToHz(float midi) const;
    int nearestScaleNote(int midi, int key, int scale) const;
    float quantizePitch(float hz);
    float getReferenceHz() const;
    float getSpeedCoefficient(float speedMs) const;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchForgeAudioProcessor)
};
