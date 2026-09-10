#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr float pi = 3.14159265358979323846f;
constexpr float minHz = 70.0f;
constexpr float maxHz = 1100.0f;
constexpr const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
}

PitchForgeAudioProcessor::PitchForgeAudioProcessor()
: AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  apvts(*this, nullptr, "PARAMETERS", createParameters()) {}

juce::AudioProcessorValueTreeState::ParameterLayout PitchForgeAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterFloat>("speed", "Speed", juce::NormalisableRange<float>(-3.0f, 200.0f, 0.01f), 20.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("amount", "Amount", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("sustain", "Sustain", juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("stabilizer", "Note Stabilizer", juce::StringArray{"None","Short","Mid","Long"}, 0));
    p.push_back(std::make_unique<juce::AudioParameterBool>("lowLatency", "Low Latency", true));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("humanize", "Humanize", juce::NormalisableRange<float>(0.0f, 1.0f), 0.10f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("scale", "Scale", juce::StringArray{"Chromatic","Major","Minor"}, 1));
    p.push_back(std::make_unique<juce::AudioParameterInt>("key", "Key", 0, 11, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("pitchReference", "Pitch Reference", juce::NormalisableRange<float>(430.0f, 450.0f), 440.0f, " Hz"));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("range", "Correction Range", juce::NormalisableRange<float>(0.0f, 12.0f), 12.0f, " st"));
    p.push_back(std::make_unique<juce::AudioParameterBool>("chromatic", "Chromatic", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("detectedNotes", "Detected Notes", true));
    p.push_back(std::make_unique<juce::AudioParameterBool>("heatMap", "HeatMap", true));
    p.push_back(std::make_unique<juce::AudioParameterBool>("doubler", "Doubler", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("doublerWidth", "Doubler Width", juce::NormalisableRange<float>(0.0f, 1.0f), 0.50f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("doublerMix", "Doubler Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("toneVolume", "Tone Volume", juce::NormalisableRange<float>(0.0f, 1.0f), 0.35f));
    return {p.begin(), p.end()};
}

bool PitchForgeAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto in = layouts.getMainInputChannelSet();
    auto out = layouts.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo()) && in == out;
}

void PitchForgeAudioProcessor::PitchDetector::prepare(double sampleRate) { fs = sampleRate; reset(); }
void PitchForgeAudioProcessor::PitchDetector::reset() { history.fill(0.0f); writePos=0; sinceAnalysis=0; smoothedPitch=0.0f; confidence=0.0f; }
void PitchForgeAudioProcessor::PitchDetector::push(float sample)
{
    history[writePos] = sample;
    writePos = (writePos + 1) % detectorSize;
    if (++sinceAnalysis >= detectorSize / 8) { sinceAnalysis = 0; analyse(); }
}
void PitchForgeAudioProcessor::PitchDetector::analyse()
{
    float mean=0.0f, energy=0.0f;
    for(int i=0;i<detectorSize;++i) mean += history[(writePos+i)%detectorSize];
    mean /= detectorSize;
    std::array<float, detectorSize> x{};
    for(int i=0;i<detectorSize;++i){ float v=history[(writePos+i)%detectorSize]-mean; float w=0.5f-0.5f*std::cos(2.0f*pi*i/(detectorSize-1)); x[i]=v*w; energy += x[i]*x[i]; }
    if(energy < 1.0e-5f){ confidence *= 0.80f; if(confidence<0.08f) smoothedPitch=0.0f; return; }

    const int minLag=(int)std::floor(fs/maxHz), maxLag=(int)std::ceil(fs/minHz);
    float best=0.0f; int lagBest=0;
    for(int lag=minLag; lag<=maxLag && lag<detectorSize/2; ++lag){
        float corr=0,e1=0,e2=0;
        for(int i=0;i<detectorSize-lag;i+=2){ float a=x[i],b=x[i+lag]; corr+=a*b; e1+=a*a; e2+=b*b; }
        float c=corr/std::sqrt(e1*e2+1e-12f);
        if(c>best){best=c;lagBest=lag;}
    }
    if(lagBest==0 || best<0.62f){ confidence=best; return; }
    auto corrAt=[&](int lag){ float c=0,e1=0,e2=0; for(int i=0;i<detectorSize-lag;i+=2){float a=x[i],b=x[i+lag];c+=a*b;e1+=a*a;e2+=b*b;} return c/std::sqrt(e1*e2+1e-12f); };
    float refined=(float)lagBest;
    if(lagBest>minLag && lagBest<maxLag){ float ym=corrAt(lagBest-1), yp=corrAt(lagBest+1), den=ym-2*best+yp; if(std::abs(den)>1e-5f) refined += 0.5f*(ym-yp)/den; }
    float pitch=(float)(fs/refined);
    if(pitch>=minHz && pitch<=maxHz){
        float alpha=juce::jmap(best,0.62f,0.98f,0.10f,0.35f);
        if(smoothedPitch>0.0f){ float semis=12.0f*std::log2(pitch/smoothedPitch); if(std::abs(semis)>7.0f && best<0.88f) pitch=smoothedPitch; }
        smoothedPitch += alpha*(pitch-smoothedPitch); confidence=best;
    }
}

void PitchForgeAudioProcessor::HighQualityPitchShifter::prepare(double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    maxBlock = juce::jmax(samplesPerBlock, 64);
    inputScratch.resize((size_t) 2 * juce::jmax(maxBlock, scratchFrames));
    outputScratch.resize((size_t) 2 * juce::jmax(maxBlock, scratchFrames));
    fifo.assign((size_t) 2 * fifoCapacityFrames, 0.0f);
    dry.assign((size_t) fifoCapacityFrames, 0.0f);
    engine.setSampleRate((unsigned int) std::lround(sampleRate));
    engine.setChannels(2);
    engine.setSetting(SETTING_USE_AA_FILTER, 1);
    engine.setSetting(SETTING_AA_FILTER_LENGTH, 64);
    engine.setSetting(SETTING_USE_QUICKSEEK, 0);
    engine.setPitch(1.0f);
    configure(true);
    reset();
    prepared = true;
}

void PitchForgeAudioProcessor::HighQualityPitchShifter::configure(bool enabledLowLatency)
{
    lowLatency = enabledLowLatency;
    const int sequenceMs = lowLatency ? 24 : 40;
    const int seekMs = lowLatency ? 10 : 15;
    const int overlapMs = lowLatency ? 6 : 8;
    engine.setSetting(SETTING_SEQUENCE_MS, sequenceMs);
    engine.setSetting(SETTING_SEEKWINDOW_MS, seekMs);
    engine.setSetting(SETTING_OVERLAP_MS, overlapMs);
    const int initial = (int) engine.getSetting(SETTING_INITIAL_LATENCY);
    // SoundTouch exposes its input/output pipeline latency directly. Do not
    // subtract the nominal output sequence: that value describes the internal
    // processing window, not host compensation.
    latencySamples = juce::jmax(0, initial);
    startupReserve = latencySamples + maxBlock * 2;
}

void PitchForgeAudioProcessor::HighQualityPitchShifter::setLowLatency(bool enabled)
{
    if (!prepared || enabled == lowLatency) return;
    engine.clear();
    fifoRead = fifoWrite = fifoCount = 0;
    primed = false;
    configure(enabled);
    engine.setPitch(currentRatio);
}

void PitchForgeAudioProcessor::HighQualityPitchShifter::reset()
{
    engine.clear();
    engine.setPitch(1.0f);
    currentRatio = 1.0f;
    fifoRead = fifoWrite = fifoCount = 0;
    primed = false;
    std::fill(fifo.begin(), fifo.end(), 0.0f);
    std::fill(dry.begin(), dry.end(), 0.0f);
}

void PitchForgeAudioProcessor::HighQualityPitchShifter::setPitchRatio(float ratio)
{
    currentRatio = juce::jlimit(0.5f, 2.0f, ratio);
    engine.setPitch(currentRatio);
}

void PitchForgeAudioProcessor::HighQualityPitchShifter::pushFifo(const float* samples, int frames)
{
    const size_t capacity = fifoCapacityFrames;
    if ((size_t) frames > capacity) { samples += (frames - (int) capacity) * 2; frames = (int) capacity; }
    if (fifoCount + (size_t) frames > capacity)
    {
        const size_t discard = fifoCount + (size_t) frames - capacity;
        fifoRead = (fifoRead + discard * 2) % fifo.size();
        fifoCount -= discard;
    }
    for (int i=0;i<frames;++i)
    {
        fifo[fifoWrite] = samples[i*2];
        fifo[(fifoWrite+1)%fifo.size()] = samples[i*2+1];
        fifoWrite = (fifoWrite + 2) % fifo.size();
    }
    fifoCount += (size_t) frames;
}

int PitchForgeAudioProcessor::HighQualityPitchShifter::popFifo(float* samples, int frames)
{
    const int available = (int) juce::jmin((size_t) frames, fifoCount);
    for (int i=0;i<available;++i)
    {
        samples[i*2] = fifo[fifoRead];
        samples[i*2+1] = fifo[(fifoRead+1)%fifo.size()];
        fifoRead = (fifoRead + 2) % fifo.size();
    }
    fifoCount -= (size_t) available;
    return available;
}

void PitchForgeAudioProcessor::HighQualityPitchShifter::drainOutput()
{
    while (engine.numSamples() > 0)
    {
        const unsigned int available = engine.numSamples();
        const int frames = (int) juce::jmin<unsigned int>(available, (unsigned int) scratchFrames);
        const unsigned int got = engine.receiveSamples(outputScratch.data(), (unsigned int) frames);
        if (got == 0) break;
        pushFifo(outputScratch.data(), (int) got);
    }
}

void PitchForgeAudioProcessor::HighQualityPitchShifter::putStereo(const float* interleaved, int frames)
{
    if (!prepared || frames <= 0) return;
    engine.putSamples(interleaved, (unsigned int) frames);
    drainOutput();
}

int PitchForgeAudioProcessor::HighQualityPitchShifter::receiveStereo(float* interleaved, int maxFrames)
{
    if (!prepared || maxFrames <= 0) return 0;
    return popFifo(interleaved, maxFrames);
}

void PitchForgeAudioProcessor::prepareToPlay(double sampleRate,int samplesPerBlock)
{
    fs=sampleRate; blockSize=samplesPerBlock; detector.prepare(sampleRate);
    shifter.prepare(sampleRate, samplesPerBlock);
    doublerShifter.prepare(sampleRate, samplesPerBlock);
    processIn.resize((size_t) 2 * juce::jmax(samplesPerBlock, 512));
    processOut.resize((size_t) 2 * juce::jmax(samplesPerBlock, 512));
    doublerOut.resize((size_t) 2 * juce::jmax(samplesPerBlock, 512));
    dryDelay.assign((size_t) 2 * 32768, 0.0f);
    dryDelayWrite = 0;
    correctionSemitones=heldSemitones=0.0f; stableBlocks=0; lastTargetMidiInternal=-1;
    inputPitch=outputPitch=confidence=correctionCents=0.0f; detectedMidi=targetMidi=-1;
    previousLowLatency = true;
    processedBlend = 0.0f;
    const int latency = shifter.getLatencySamples();
    algorithmicLatencySamples.store(latency);
    setLatencySamples(latency);
}

float PitchForgeAudioProcessor::midiToHz(float midi) const
{
    const float ref=getReferenceHz(); return ref*std::pow(2.0f,(midi-69.0f)/12.0f);
}
float PitchForgeAudioProcessor::getReferenceHz() const
{ return apvts.getRawParameterValue("pitchReference")->load(); }
int PitchForgeAudioProcessor::nearestScaleNote(int midi,int key,int scale) const
{
    if(scale==0) return midi;
    static constexpr int major[]={0,2,4,5,7,9,11}; static constexpr int minor[]={0,2,3,5,7,8,10};
    const int* intervals=scale==1?major:minor; int best=midi,bestDist=99;
    for(int d=-12;d<=12;++d){int n=midi+d,pc=(n-key)%12;if(pc<0)pc+=12;for(int j=0;j<7;++j)if(intervals[j]==pc && std::abs(d)<bestDist){best=n;bestDist=std::abs(d);}}
    return best;
}
float PitchForgeAudioProcessor::quantizePitch(float hz)
{
    if(hz<=0)return 0;
    const int key=(int)apvts.getRawParameterValue("key")->load();
    const int scale=(int)apvts.getRawParameterValue("scale")->load();
    const bool chrom=apvts.getRawParameterValue("chromatic")->load()>0.5f;
    float midi=69.0f+12.0f*std::log2(hz/getReferenceHz());
    int nearest=(int)std::lround(midi); int t=chrom?nearest:nearestScaleNote(nearest,key,scale); targetMidi.store(t); detectedMidi.store(nearest); return midiToHz((float)t);
}
float PitchForgeAudioProcessor::getSpeedCoefficient(float speedMs) const
{
    if(speedMs<=0.0f) return 0.65f;
    const float coefficient = 1.0f - static_cast<float>(std::exp(-1.0 / (fs * (static_cast<double>(speedMs) * 0.001) + 1.0)));
    return juce::jlimit(0.002f, 0.65f, coefficient);
}

void PitchForgeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin(2, buffer.getNumChannels());
    if (ch == 0) return;

    const float speed = apvts.getRawParameterValue("speed")->load();
    const float amount = apvts.getRawParameterValue("amount")->load();
    const float sustain = apvts.getRawParameterValue("sustain")->load();
    const int stabilizer = (int) apvts.getRawParameterValue("stabilizer")->load();
    const bool lowLatency = apvts.getRawParameterValue("lowLatency")->load() > 0.5f;
    const float human = apvts.getRawParameterValue("humanize")->load();
    const float mix = apvts.getRawParameterValue("mix")->load();
    const float range = apvts.getRawParameterValue("range")->load();
    const bool doubler = apvts.getRawParameterValue("doubler")->load() > 0.5f;
    const float dWidth = apvts.getRawParameterValue("doublerWidth")->load();
    const float dMix = apvts.getRawParameterValue("doublerMix")->load();

    if (lowLatency != previousLowLatency)
    {
        // Reconfiguring SoundTouch flushes its internal overlap buffers.
        // Fade the processed path back in instead of exposing that flush as a
        // hard discontinuity when the user toggles Low Latency live.
        shifter.setLowLatency(lowLatency);
        doublerShifter.setLowLatency(lowLatency);
        processedBlend = 0.0f;
        previousLowLatency = lowLatency;
    }
    const int latency = shifter.getLatencySamples();
    if (latency != algorithmicLatencySamples.load())
    {
        algorithmicLatencySamples.store(latency);
        setLatencySamples(latency);
    }

    const int stabilizerMs = stabilizer == 1 ? 40 : (stabilizer == 2 ? 80 : (stabilizer == 3 ? 200 : 0));
    const int requiredSamples = (int) (fs * stabilizerMs * 0.001);
    const int chunkLimit = juce::jmax(1, juce::jmin(processingChunk, (int) processIn.size() / 2));

    for (int base=0; base<n; base += chunkLimit)
    {
        const int frames = juce::jmin(chunkLimit, n-base);
        float chunkRatio = std::pow(2.0f, (correctionSemitones * amount) / 12.0f);
        shifter.setPitchRatio(chunkRatio);
        if (doubler && dMix > 0.001f)
        {
            const float widthSemis = (dWidth * 2.0f - 1.0f) * 6.0f;
            doublerShifter.setPitchRatio(std::pow(2.0f, (correctionSemitones * amount + widthSemis) / 12.0f));
        }

        for (int i=0;i<frames;++i)
        {
            const float left = buffer.getSample(0, base+i);
            const float right = ch > 1 ? buffer.getSample(1, base+i) : left;
            const float mono = 0.5f * (left + right);
            detector.push(mono);
            const float detected = detector.getPitchHz();
            const float conf = detector.getConfidence();
            confidence.store(conf);
            inputPitch.store(detected);

            if (detected > 0.0f && conf > 0.62f && std::abs(mono) > 0.0006f)
            {
                const float target = quantizePitch(detected);
                const float rawSemi = 12.0f * std::log2(target / detected);
                float capped = juce::jlimit(-range, range, rawSemi);
                float effectiveSpeed = speed;
                if (std::abs(heldSemitones - capped) < 0.35f) effectiveSpeed += sustain * 60.0f;
                effectiveSpeed = juce::jlimit(-3.0f, 200.0f, effectiveSpeed);
                const float coeff = getSpeedCoefficient(effectiveSpeed);

                if (stabilizer > 0 && !lowLatency)
                {
                    if (lastTargetMidiInternal == targetMidi.load()) stableBlocks += 1; else stableBlocks = 0;
                    lastTargetMidiInternal = targetMidi.load();
                    const bool stableLongEnough = stableBlocks * juce::jmax(1, blockSize) >= requiredSamples;
                    if (!stableLongEnough) capped = heldSemitones;
                }
                heldSemitones += coeff * (capped - heldSemitones);
                const float humanScale = 1.0f - human * 0.35f;
                correctionSemitones += coeff * (heldSemitones - correctionSemitones) * humanScale;
            }
            else
            {
                correctionSemitones *= 0.9985f;
                stableBlocks = 0;
            }

            const float applied = correctionSemitones * amount;
            correctionCents.store(applied * 100.0f);
            processIn[(size_t)i*2] = left;
            processIn[(size_t)i*2+1] = right;
        }

        // Use the ratio calculated at the end of the chunk so the DSP control is
        // updated frequently without calling SoundTouch's control path every sample.
        const float finalRatio = std::pow(2.0f, (correctionSemitones * amount) / 12.0f);
        shifter.setPitchRatio(finalRatio);
        shifter.putStereo(processIn.data(), frames);
        const int got = shifter.receiveStereo(processOut.data(), frames);
        // SoundTouch may temporarily return fewer frames while its internal
        // WSOLA pipeline is replenishing. Never inject zero samples into the
        // wet path: use the original input as a continuity fallback and let
        // processedBlend fade back to the delayed dry signal.
        for (int i = got; i < frames; ++i)
        {
            processOut[(size_t)i * 2] = processIn[(size_t)i * 2];
            processOut[(size_t)i * 2 + 1] = processIn[(size_t)i * 2 + 1];
        }

        if (doubler && dMix > 0.001f)
        {
            const float widthSemis = (dWidth * 2.0f - 1.0f) * 6.0f;
            doublerShifter.setPitchRatio(std::pow(2.0f, (correctionSemitones * amount + widthSemis) / 12.0f));
            doublerShifter.putStereo(processIn.data(), frames);
        }
        int dGot = 0;
        if (doubler && dMix > 0.001f) {
            std::fill(doublerOut.begin(), doublerOut.begin() + (size_t)frames * 2, 0.0f);
            dGot = doublerShifter.receiveStereo(doublerOut.data(), frames);
        }

        for (int i=0;i<frames;++i)
        {
            const float leftIn = processIn[(size_t)i*2];
            const float rightIn = processIn[(size_t)i*2+1];
            float wetL = processOut[(size_t)i*2];
            float wetR = processOut[(size_t)i*2+1];
            if (doubler && dMix > 0.001f && i < dGot)
            {
                const float dL = doublerOut[(size_t)i*2];
                const float dR = doublerOut[(size_t)i*2+1];
                wetL = wetL * (1.0f-dMix) + dL*dMix;
                wetR = wetR * (1.0f-dMix) + dR*dMix;
            }

            // Delay the dry path by the measured average SoundTouch pipeline latency.
            const size_t dryCapacity = dryDelay.size() / 2;
            const size_t write = dryDelayWrite;
            dryDelay[write*2] = leftIn;
            dryDelay[write*2+1] = rightIn;
            const size_t read = (write + dryCapacity - (size_t)juce::jlimit(0, (int)dryCapacity-1, latency)) % dryCapacity;
            const float dryL = dryDelay[read*2];
            const float dryR = dryDelay[read*2+1];
            dryDelayWrite = (dryDelayWrite + 1) % dryCapacity;

            // Smoothly enter/leave the processed stream. This is a safety net for
            // SoundTouch's variable internal output availability and guarantees
            // that an underflow can never become a hard zero-sample discontinuity.
            const float desiredBlend = (i < got ? 1.0f : 0.0f);
            const float blendStep = 1.0f / 256.0f;
            if (processedBlend < desiredBlend) processedBlend = juce::jmin(desiredBlend, processedBlend + blendStep);
            else if (processedBlend > desiredBlend) processedBlend = juce::jmax(desiredBlend, processedBlend - blendStep);

            const float safeWetL = dryL + (wetL - dryL) * processedBlend;
            const float safeWetR = dryR + (wetR - dryR) * processedBlend;
            const float outL = dryL + (safeWetL - dryL) * mix;
            const float outR = dryR + (safeWetR - dryR) * mix;
            buffer.setSample(0, base+i, outL);
            if (ch > 1) buffer.setSample(1, base+i, outR);

            const float detected = inputPitch.load();
            const float applied = correctionCents.load() / 100.0f;
            if (detected > 0.0f)
            {
                const float outputMidi = 69.0f + 12.0f*std::log2(detected/getReferenceHz()) + applied;
                outputPitch.store(midiToHz(outputMidi));
            }
            else outputPitch.store(0.0f);
        }
    }
}

void PitchForgeAudioProcessor::getStateInformation(juce::MemoryBlock& dest){if(auto xml=apvts.copyState().createXml())copyXmlToBinary(*xml,dest);}
void PitchForgeAudioProcessor::setStateInformation(const void* data,int size){if(auto xml=getXmlFromBinary(data,size))if(xml->hasTagName(apvts.state.getType()))apvts.replaceState(juce::ValueTree::fromXml(*xml));}
juce::AudioProcessorEditor* PitchForgeAudioProcessor::createEditor(){return new PitchForgeAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new PitchForgeAudioProcessor();}
