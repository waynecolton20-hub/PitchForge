#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace {
constexpr float pi = 3.14159265358979323846f;
constexpr float minHz = 70.0f;
constexpr float maxHz = 1000.0f;
}

PitchForgeAudioProcessor::PitchForgeAudioProcessor()
: AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  apvts(*this, nullptr, "PARAMETERS", createParameters()) {}

juce::AudioProcessorValueTreeState::ParameterLayout PitchForgeAudioProcessor::createParameters() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterFloat>("speed", "Speed", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.75f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("amount", "Amount", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("humanize", "Humanize", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.15f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("scale", "Scale", juce::StringArray{"Chromatic","Major","Minor"}, 0));
    p.push_back(std::make_unique<juce::AudioParameterInt>("key", "Key", 0, 11, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("retuneRange", "Range", juce::NormalisableRange<float>(0.0f, 12.0f, 0.01f), 12.0f));
    return {p.begin(), p.end()};
}

bool PitchForgeAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    auto out = layouts.getMainOutputChannelSet();
    auto in = layouts.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == in;
}

void PitchForgeAudioProcessor::PitchDetector::prepare(double sampleRate) { fs = sampleRate; reset(); }
void PitchForgeAudioProcessor::PitchDetector::reset() {
    history.fill(0.0f); writePos = 0; sinceAnalysis = 0; smoothedPitch = 0.0f; confidence = 0.0f;
}
void PitchForgeAudioProcessor::PitchDetector::push(float sample) {
    history[writePos] = sample;
    writePos = (writePos + 1) % detectorSize;
    if (++sinceAnalysis >= detectorSize / 4) { sinceAnalysis = 0; analyse(); }
}
void PitchForgeAudioProcessor::PitchDetector::analyse() {
    std::array<float, detectorSize> x{};
    float mean = 0.0f;
    for (int i = 0; i < detectorSize; ++i) mean += history[(writePos + i) % detectorSize];
    mean /= detectorSize;
    float energy = 0.0f;
    for (int i = 0; i < detectorSize; ++i) {
        const float v = history[(writePos + i) % detectorSize] - mean;
        x[i] = v * (0.5f - 0.5f * std::cos(2.0f * pi * i / (detectorSize - 1)));
        energy += x[i] * x[i];
    }
    if (energy < 1.0e-5f) { confidence *= 0.85f; if (confidence < 0.05f) smoothedPitch = 0.0f; return; }

    const int minLag = (int)std::floor(fs / maxHz);
    const int maxLag = (int)std::ceil(fs / minHz);
    float bestCorr = 0.0f;
    int bestLag = 0;
    for (int lag = minLag; lag <= maxLag && lag < detectorSize / 2; ++lag) {
        float corr = 0.0f, e1 = 0.0f, e2 = 0.0f;
        for (int i = 0; i < detectorSize - lag; i += 2) {
            const float a = x[i], b = x[i + lag];
            corr += a * b; e1 += a * a; e2 += b * b;
        }
        const float c = corr / std::sqrt(e1 * e2 + 1.0e-12f);
        if (c > bestCorr) { bestCorr = c; bestLag = lag; }
    }
    if (bestLag == 0 || bestCorr < 0.72f) { confidence = bestCorr; return; }

    float refinedLag = (float)bestLag;
    if (bestLag > minLag && bestLag < maxLag) {
        auto corrAt = [&](int lag) {
            float corr = 0.0f, e1 = 0.0f, e2 = 0.0f;
            for (int i = 0; i < detectorSize - lag; i += 2) { float a=x[i], b=x[i+lag]; corr+=a*b; e1+=a*a; e2+=b*b; }
            return corr / std::sqrt(e1*e2 + 1e-12f);
        };
        const float ym = corrAt(bestLag - 1), y0 = bestCorr, yp = corrAt(bestLag + 1);
        const float denom = ym - 2.0f*y0 + yp;
        if (std::abs(denom) > 1e-5f) refinedLag += 0.5f * (ym - yp) / denom;
    }
    const float pitch = (float)(fs / refinedLag);
    if (pitch >= minHz && pitch <= maxHz) {
        const float alpha = 0.25f + 0.55f * juce::jlimit(0.0f, 1.0f, bestCorr);
        smoothedPitch = smoothedPitch <= 0.0f ? pitch : smoothedPitch + alpha * (pitch - smoothedPitch);
        confidence = bestCorr;
    }
}

void PitchForgeAudioProcessor::GranularPitchShifter::prepare(double sampleRate) {
    fs = sampleRate;
    window.fill(0.0f);
    for (int i=0;i<grainSize;++i) window[i] = 0.5f - 0.5f*std::cos(2.0f*pi*i/(grainSize-1));
    reset();
}
void PitchForgeAudioProcessor::GranularPitchShifter::reset() {
    inputRing.fill(0.0f); outRing.fill(0.0f); writePos=0; readA=readB=0.0; grainCounter=0; initialised=false;
}
float PitchForgeAudioProcessor::GranularPitchShifter::readSample(double pos) const {
    while (pos < 0.0) pos += maxDelay;
    while (pos >= maxDelay) pos -= maxDelay;
    const int i0=(int)std::floor(pos), i1=(i0+1)%maxDelay;
    const float frac=(float)(pos-i0);
    return inputRing[i0] + frac*(inputRing[i1]-inputRing[i0]);
}
void PitchForgeAudioProcessor::GranularPitchShifter::startGrain(double& head, double offsetSamples) {
    head = (double)writePos - offsetSamples;
    while (head < 0.0) head += maxDelay;
    while (head >= maxDelay) head -= maxDelay;
}
float PitchForgeAudioProcessor::GranularPitchShifter::process(float input, float ratio) {
    inputRing[writePos] = input;
    if (!initialised) {
        startGrain(readA, grainSize * 0.75); readB = readA + grainHop; while(readB>=maxDelay) readB-=maxDelay; initialised=true;
    }
    const int outIndex = writePos;
    const float wa = window[grainCounter];
    const int bIndex = (grainCounter + grainHop) % grainSize;
    const float wb = window[bIndex];
    const float sampleA = readSample(readA);
    const float sampleB = readSample(readB);
    float out = sampleA * wa + sampleB * wb;
    outRing[outIndex] = out;

    readA += ratio;
    readB += ratio;
    if (++grainCounter >= grainHop) {
        grainCounter = 0;
        // Crossfade the active grain pair by moving the older head one half-grain behind.
        const double oldA = readA;
        readA = readB;
        readB = oldA;
        const double desiredDelay = grainSize * 0.75;
        readB = (double)writePos - desiredDelay;
        while (readB < 0.0) readB += maxDelay;
        while (readB >= maxDelay) readB -= maxDelay;
    }
    writePos = (writePos + 1) % maxDelay;
    return out;
}

void PitchForgeAudioProcessor::prepareToPlay(double sampleRate, int) {
    fs = sampleRate;
    detector.prepare(sampleRate);
    shifter.prepare(sampleRate);
    currentPitch = targetPitch = correctionSemitones = lastValidPitch = 0.0f;
    pitchConfidence = 0.0f; targetMidi = -1;
}

float PitchForgeAudioProcessor::midiToHz(float midi) const { return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f); }

int PitchForgeAudioProcessor::nearestScaleNote(int midi, int key, int scale) const {
    if (scale == 0) return midi;
    static constexpr int major[] = {0,2,4,5,7,9,11};
    static constexpr int minor[] = {0,2,3,5,7,8,10};
    const int* intervals = scale == 1 ? major : minor;
    int best = midi; int bestDist = 99;
    for (int d=-12; d<=12; ++d) {
        const int n = midi + d;
        int pc = (n - key) % 12; if (pc < 0) pc += 12;
        bool inScale=false; for (int j=0;j<7;++j) if (intervals[j]==pc) inScale=true;
        if (inScale && std::abs(d)<bestDist) { best=n; bestDist=std::abs(d); }
    }
    return best;
}

float PitchForgeAudioProcessor::quantizePitch(float hz) {
    if (hz <= 0.0f) return 0.0f;
    const int key = (int)apvts.getRawParameterValue("key")->load();
    const int scale = (int)apvts.getRawParameterValue("scale")->load();
    const float midi = 69.0f + 12.0f * std::log2(hz / 440.0f);
    const int nearest = (int)std::lround(midi);
    targetMidi = nearestScaleNote(nearest, key, scale);
    return midiToHz((float)targetMidi);
}

void PitchForgeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int n=buffer.getNumSamples(), ch=buffer.getNumChannels();
    const float speed=apvts.getRawParameterValue("speed")->load();
    const float amount=apvts.getRawParameterValue("amount")->load();
    const float human=apvts.getRawParameterValue("humanize")->load();
    const float mix=apvts.getRawParameterValue("mix")->load();
    const float maxRange=apvts.getRawParameterValue("retuneRange")->load();

    for (int i=0;i<n;++i) {
        float mono=0.0f;
        for(int c=0;c<ch;++c) mono += buffer.getSample(c,i);
        mono /= (float)juce::jmax(1,ch);
        detector.push(mono);
        const float detected=detector.getPitchHz();
        pitchConfidence=detector.getConfidence();
        if (std::abs(mono) > silenceGate && detected > 0.0f && pitchConfidence > 0.72f) {
            currentPitch = detected;
            lastValidPitch = detected;
            targetPitch = quantizePitch(detected);
            const float semis = 12.0f * std::log2(targetPitch / detected);
            const float capped = juce::jlimit(-maxRange, maxRange, semis);
            // Speed is attack time: higher speed means more immediate correction.
            const float coeff = juce::jlimit(0.002f, 0.35f, 0.002f + speed * speed * 0.30f);
            correctionSemitones += coeff * (capped - correctionSemitones);
            // Humanize relaxes correction around already-good notes.
            const float nearNote = juce::jlimit(0.0f,1.0f,1.0f-std::abs(semis)/0.5f);
            correctionSemitones *= (1.0f - human * 0.45f * nearNote);
        } else {
            correctionSemitones *= 0.995f;
        }
        const float ratio = std::pow(2.0f, correctionSemitones * amount / 12.0f);
        const float shifted=shifter.process(mono, ratio);
        for(int c=0;c<ch;++c) {
            const float dry=buffer.getSample(c,i);
            const float wetSample=ch==1 ? shifted : shifted;
            buffer.setSample(c,i, dry + (wetSample-dry)*mix);
        }
    }
}

void PitchForgeAudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if(auto xml=apvts.copyState().createXml()) copyXmlToBinary(*xml,dest);
}
void PitchForgeAudioProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* PitchForgeAudioProcessor::createEditor() { return new PitchForgeAudioProcessorEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PitchForgeAudioProcessor(); }
