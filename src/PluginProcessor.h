#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

class PitchForgeAudioProcessor : public juce::AudioProcessor {
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

private:
    static constexpr int detectorSize = 2048;
    static constexpr int maxDelay = 8192;
    static constexpr int grainSize = 2048;
    static constexpr int grainHop = grainSize / 2;

    class PitchDetector {
    public:
        void prepare(double sampleRate);
        void push(float sample);
        float getPitchHz() const { return smoothedPitch; }
        float getConfidence() const { return confidence; }
        void reset();
    private:
        double fs = 44100.0;
        std::array<float, detectorSize> history{};
        int writePos = 0;
        int sinceAnalysis = 0;
        float smoothedPitch = 0.0f;
        float confidence = 0.0f;
        void analyse();
    };

    class GranularPitchShifter {
    public:
        void prepare(double sampleRate);
        void reset();
        float process(float input, float ratio);
    private:
        double fs = 44100.0;
        std::array<float, maxDelay> inputRing{};
        std::array<float, maxDelay> outRing{};
        std::array<float, grainSize> window{};
        int writePos = 0;
        double readA = 0.0, readB = 0.0;
        int grainCounter = 0;
        bool initialised = false;
        float readSample(double pos) const;
        void startGrain(double& readHead, double offsetSamples);
    };

    juce::AudioProcessorValueTreeState apvts;
    double fs = 44100.0;
    PitchDetector detector;
    GranularPitchShifter shifter;
    float currentPitch = 0.0f;
    float targetPitch = 0.0f;
    float correctionSemitones = 0.0f;
    float lastValidPitch = 0.0f;
    float pitchConfidence = 0.0f;
    int targetMidi = -1;
    float silenceGate = 0.001f;

    float quantizePitch(float hz);
    float midiToHz(float midi) const;
    int nearestScaleNote(int midi, int key, int scale) const;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchForgeAudioProcessor)
};
