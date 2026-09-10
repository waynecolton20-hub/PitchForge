#pragma once
#include <JuceHeader.h>
#include <SoundTouch.h>
#include <array>
#include <atomic>
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
    int getAlgorithmicLatencySamples() const noexcept { return algorithmicLatencySamples.load(); }

private:
    static constexpr int detectorSize = 2048;
    static constexpr int processingChunk = 128;
    static constexpr int fifoCapacityFrames = 65536;
    static constexpr float pi = 3.14159265358979323846f;
    static constexpr float minHz = 70.0f;
    static constexpr float maxHz = 1100.0f;

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

    // SoundTouch provides a continuous WSOLA/time-domain pitch processor instead of
    // the previous grain-reset algorithm. It is fed in short blocks and drained into
    // a preallocated FIFO so the plugin always returns exactly one output frame per
    // input frame, avoiding the block starvation/clicking failure of v3.
    class HighQualityPitchShifter
    {
    public:
        void prepare(double sampleRate, int samplesPerBlock);
        void reset();
        void setLowLatency(bool enabled);
        void setPitchRatio(float ratio);
        float getPitchRatio() const noexcept { return currentRatio; }
        void putStereo(const float* interleaved, int frames);
        int receiveStereo(float* interleaved, int maxFrames);
        int getLatencySamples() const noexcept { return latencySamples; }
        bool isReady() const noexcept { return prepared; }
    private:
        void configure(bool lowLatency);
        void drainOutput();
        static constexpr int scratchFrames = 4096;
        soundtouch::SoundTouch engine;
        std::vector<float> inputScratch;
        std::vector<float> outputScratch;
        std::vector<float> fifo;
        std::vector<float> dry;
        size_t fifoRead = 0, fifoWrite = 0;
        size_t fifoCount = 0;
        int startupReserve = 0;
        double fs = 44100.0;
        int maxBlock = 512;
        int latencySamples = 0;
        bool prepared = false;
        bool lowLatency = true;
        bool primed = false;
        float currentRatio = 1.0f;
        void pushFifo(const float* samples, int frames);
        int popFifo(float* samples, int frames);
    };

    juce::AudioProcessorValueTreeState apvts;
    double fs = 44100.0;
    int blockSize = 0;
    HighQualityPitchShifter shifter;
    HighQualityPitchShifter doublerShifter;
    PitchDetector detector;

    float correctionSemitones = 0.0f;
    float heldSemitones = 0.0f;
    int stableBlocks = 0;
    int lastTargetMidiInternal = -1;
    bool previousLowLatency = true;

    std::vector<float> processIn;
    std::vector<float> processOut;
    std::vector<float> doublerOut;
    std::vector<float> dryDelay;
    size_t dryDelayWrite = 0;

    std::atomic<float> inputPitch { 0.0f }, outputPitch { 0.0f }, confidence { 0.0f }, correctionCents { 0.0f };
    std::atomic<int> detectedMidi { -1 }, targetMidi { -1 }, algorithmicLatencySamples { 0 };
    float processedBlend = 0.0f;

    float midiToHz(float midi) const;
    int nearestScaleNote(int midi, int key, int scale) const;
    float quantizePitch(float hz);
    float getReferenceHz() const;
    float getSpeedCoefficient(float speedMs) const;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchForgeAudioProcessor)
};
