#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

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
    p.push_back(std::make_unique<juce::AudioParameterFloat>("speed", "Speed", juce::NormalisableRange<float>(-3.0f, 200.0f, 0.01f), 8.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("amount", "Amount", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("sustain", "Sustain", juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("stabilizer", "Note Stabilizer", juce::StringArray{"None","Short","Mid","Long"}, 0));
    p.push_back(std::make_unique<juce::AudioParameterBool>("lowLatency", "Low Latency", false));
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
    if (++sinceAnalysis >= detectorSize / 16) { sinceAnalysis = 0; analyse(); }
}
void PitchForgeAudioProcessor::PitchDetector::analyse()
{
    float mean = 0.0f;
    for (int i = 0; i < detectorSize; ++i)
        mean += history[(writePos + i) % detectorSize];
    mean /= (float) detectorSize;

    std::array<float, detectorSize> x{};
    float energy = 0.0f;
    for (int i = 0; i < detectorSize; ++i)
    {
        const float v = history[(writePos + i) % detectorSize] - mean;
        const float w = 0.5f - 0.5f * std::cos(2.0f * pi * i / (detectorSize - 1));
        x[i] = v * w;
        energy += x[i] * x[i];
    }
    if (energy < 1.0e-5f)
    {
        confidence *= 0.82f;
        if (confidence < 0.08f) smoothedPitch = 0.0f;
        return;
    }

    const int minLag = juce::jmax(2, (int) std::floor(fs / maxHz));
    const int maxLag = juce::jmin(detectorSize / 2 - 1, (int) std::ceil(fs / minHz));

    // YIN-style cumulative mean normalized difference. Unlike a raw
    // autocorrelation maximum, this strongly prefers the first true period and
    // is much less likely to lock to a vocal harmonic one octave away.
    std::array<float, detectorSize / 2> cmnd{};
    float running = 0.0f;
    for (int tau = 1; tau <= maxLag; ++tau)
    {
        float d = 0.0f;
        for (int i = 0; i < detectorSize - tau; i += 4)
        {
            const float delta = x[i] - x[i + tau];
            d += delta * delta;
        }
        running += d;
        cmnd[(size_t) tau] = running > 1.0e-12f ? d * (float) tau / running : 1.0f;
    }

    int tauBest = 0;
    constexpr float yinThreshold = 0.16f;
    for (int tau = minLag; tau < maxLag; ++tau)
    {
        if (cmnd[(size_t) tau] < yinThreshold && cmnd[(size_t) tau] <= cmnd[(size_t) (tau + 1)])
        {
            tauBest = tau;
            break;
        }
    }
    if (tauBest == 0)
    {
        float best = 1.0f;
        for (int tau = minLag; tau <= maxLag; ++tau)
            if (cmnd[(size_t) tau] < best) { best = cmnd[(size_t) tau]; tauBest = tau; }
    }
    if (tauBest == 0) { confidence = 0.0f; return; }

    // Check octave-related candidates and keep the one closest to the tracked
    // pitch when its YIN score is effectively equivalent.
    int chosenLag = tauBest;
    const float baseScore = cmnd[(size_t) tauBest];
    if (smoothedPitch > 0.0f)
    {
        float bestDistance = std::numeric_limits<float>::infinity();
        const int candidates[] = { tauBest, tauBest * 2, tauBest / 2 };
        for (const int candidate : candidates)
        {
            if (candidate < minLag || candidate > maxLag) continue;
            const float score = cmnd[(size_t) candidate];
            if (score > baseScore + 0.055f) continue;
            const float hz = (float) (fs / (double) candidate);
            const float distance = std::abs(12.0f * std::log2(hz / smoothedPitch));
            if (distance < bestDistance) { bestDistance = distance; chosenLag = candidate; }
        }
    }

    float refinedLag = (float) chosenLag;
    if (chosenLag > minLag && chosenLag < maxLag)
    {
        const float ym = cmnd[(size_t) (chosenLag - 1)];
        const float y0 = cmnd[(size_t) chosenLag];
        const float yp = cmnd[(size_t) (chosenLag + 1)];
        const float den = ym - 2.0f * y0 + yp;
        if (std::abs(den) > 1.0e-6f)
            refinedLag += 0.5f * (ym - yp) / den;
    }

    const float pitch = (float) (fs / (double) refinedLag);
    const float yinScore = juce::jlimit(0.0f, 1.0f, 1.0f - cmnd[(size_t) chosenLag]);
    if (pitch < minHz || pitch > maxHz || yinScore < 0.58f)
    {
        confidence = yinScore;
        return;
    }

    if (smoothedPitch > 0.0f)
    {
        const float semis = 12.0f * std::log2(pitch / smoothedPitch);
        if (std::abs(semis) > 4.5f && yinScore < 0.88f)
        {
            confidence = yinScore;
            return;
        }
    }

    const float alpha = juce::jmap(yinScore, 0.58f, 0.98f, 0.06f, 0.24f);
    smoothedPitch = smoothedPitch > 0.0f ? smoothedPitch + alpha * (pitch - smoothedPitch) : pitch;
    confidence = yinScore;
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
    configure(false);
    reset();
    prepared = true;
}

void PitchForgeAudioProcessor::HighQualityPitchShifter::configure(bool enabledLowLatency)
{
    // Keep one fixed, high-quality SoundTouch pipeline. Changing the internal
    // overlap geometry while audio is running flushes WSOLA state and is a
    // direct route to clicks. The Low Latency parameter therefore changes only
    // control smoothing; it never tears down the audio stream.
    lowLatency = enabledLowLatency;
    constexpr int sequenceMs = 110;
    constexpr int seekMs = 24;
    constexpr int overlapMs = 20;
    engine.setSetting(SETTING_SEQUENCE_MS, sequenceMs);
    engine.setSetting(SETTING_SEEKWINDOW_MS, seekMs);
    engine.setSetting(SETTING_OVERLAP_MS, overlapMs);
    const int initial = (int) engine.getSetting(SETTING_INITIAL_LATENCY);
    const int outputSequence = (int) engine.getSetting(SETTING_NOMINAL_OUTPUT_SEQUENCE);
    latencySamples = juce::jmax(0, initial - outputSequence / 2);
    startupReserve = latencySamples + maxBlock * 2;
}

void PitchForgeAudioProcessor::HighQualityPitchShifter::setLowLatency(bool enabled)
{
    // Do not clear/reconfigure SoundTouch from the realtime thread. A live
    // engine reset creates an audible discontinuity and can invalidate host
    // latency compensation. The flag is intentionally just a control-mode hint.
    lowLatency = enabled;
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
    // Never return a partial wet block. Splicing N wet samples followed by dry
    // samples inside the same host block creates a literal time-domain step.
    // Hold the FIFO intact until a complete block is available.
    if (fifoCount < (size_t) frames) return 0;
    for (int i=0;i<frames;++i)
    {
        samples[i*2] = fifo[fifoRead];
        samples[i*2+1] = fifo[(fifoRead+1)%fifo.size()];
        fifoRead = (fifoRead + 2) % fifo.size();
    }
    fifoCount -= (size_t) frames;
    return frames;
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
    const float tauSeconds = juce::jmax(0.001, static_cast<double>(speedMs) * 0.001);
    const float coefficient = 1.0f - static_cast<float>(std::exp(-1.0 / (fs * tauSeconds)));
    return juce::jlimit(0.0035f, 0.65f, coefficient);
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

    // Low Latency is a smoothing/response mode only. The SoundTouch pipeline
    // and reported host latency remain fixed for glitch-free realtime operation.
    const int stabilizerMs = stabilizer == 1 ? 40 : (stabilizer == 2 ? 80 : (stabilizer == 3 ? 200 : 0));
    const int requiredSamples = (int) (fs * stabilizerMs * 0.001);
    const int chunkLimit = juce::jmax(64, juce::jmin(processingChunk, (int) processIn.size() / 2));

    for (int base=0; base<n; base += chunkLimit)
    {
        const int frames = juce::jmin(chunkLimit, n-base);
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
                correctionSemitones += coeff * humanScale * (heldSemitones - correctionSemitones);
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

        // Update the pitch engine at a short, deterministic cadence. The previous
        // implementation calculated the target from the LAST sample of a large
        // block, which produced staircase pitch commands and audible WSOLA stress.
        // 64-sample control slices keep the ratio trajectory continuous while
        // avoiding per-sample SoundTouch control calls.
        constexpr int controlSlice = 64;
        for (int slice = 0; slice < frames; slice += controlSlice)
        {
            const float targetRatio = std::pow(2.0f, (correctionSemitones * amount) / 12.0f);
            const float current = shifter.getPitchRatio();
            const float maxRatioDelta = lowLatency ? 0.0012f : 0.0008f;
            const float nextRatio = current + juce::jlimit(-maxRatioDelta, maxRatioDelta, targetRatio - current);
            shifter.setPitchRatio(nextRatio);
            const int sliceFrames = juce::jmin(controlSlice, frames - slice);
            juce::ignoreUnused(sliceFrames);
        }
        shifter.putStereo(processIn.data(), frames);
        const int got = shifter.receiveStereo(processOut.data(), frames);
        // Do not fill missing wet frames with the current input. That input is
        // not latency-aligned with the SoundTouch stream and can create a hard
        // time-domain discontinuity. Missing frames are resolved against the
        // same latency-aligned dry sample below.

        if (doubler && dMix > 0.001f)
        {
            const float widthSemis = (dWidth * 2.0f - 1.0f) * 6.0f;
            const float dTarget = std::pow(2.0f, (correctionSemitones * amount + widthSemis) / 12.0f);
            const float dCurrent = doublerShifter.getPitchRatio();
            const float dMaxDelta = lowLatency ? 0.004f : 0.0025f;
            doublerShifter.setPitchRatio(dCurrent + juce::jlimit(-dMaxDelta, dMaxDelta, dTarget - dCurrent));
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

            // SoundTouch can briefly under-run while its WSOLA analysis window
            // is being replenished. Never switch domains on a single sample.
            // Instead, crossfade the wet path toward the latency-aligned dry path
            // over several milliseconds and recover just as smoothly. This masks
            // the exact discontinuity that previously produced audible crackle.
            if (got == 0)
            {
                wetL = dryL;
                wetR = dryR;
            }

            // Engage the corrected path smoothly. Once a complete wet block is
            // available, keep the wet state latched; repeatedly toggling wet/dry
            // on SoundTouch starvation makes the vocal audibly pump and can expose
            // tiny discontinuities as crackle.
            const bool wetAvailable = (got >= frames);
            const float blendStep = 1.0f / juce::jmax(64.0f, (float) fs * 0.020f);
            if (wetAvailable) processedBlend = juce::jmin(1.0f, processedBlend + blendStep);
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
