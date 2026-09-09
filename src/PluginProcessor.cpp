#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    p.push_back(std::make_unique<juce::AudioParameterFloat>("amount", "Amount", 0.0f, 1.0f, 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("sustain", "Sustain", -1.0f, 1.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("stabilizer", "Note Stabilizer", juce::StringArray{"None","Short","Mid","Long"}, 0));
    p.push_back(std::make_unique<juce::AudioParameterBool>("lowLatency", "Low Latency", true));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("humanize", "Humanize", 0.0f, 1.0f, 0.10f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", 0.0f, 1.0f, 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("scale", "Scale", juce::StringArray{"Chromatic","Major","Minor"}, 1));
    p.push_back(std::make_unique<juce::AudioParameterInt>("key", "Key", 0, 11, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("pitchReference", "Pitch Reference", 430.0f, 450.0f, 440.0f, " Hz"));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("range", "Correction Range", 0.0f, 12.0f, 12.0f, " st"));
    p.push_back(std::make_unique<juce::AudioParameterBool>("chromatic", "Chromatic", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("detectedNotes", "Detected Notes", true));
    p.push_back(std::make_unique<juce::AudioParameterBool>("heatMap", "HeatMap", true));
    p.push_back(std::make_unique<juce::AudioParameterBool>("doubler", "Doubler", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("doublerWidth", "Doubler Width", 0.0f, 1.0f, 0.50f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("doublerMix", "Doubler Mix", 0.0f, 1.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("toneVolume", "Tone Volume", 0.0f, 1.0f, 0.35f));
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
        // Hysteresis prevents octave jumps from instantly taking over a sustained vowel.
        if(smoothedPitch>0.0f){ float semis=12.0f*std::log2(pitch/smoothedPitch); if(std::abs(semis)>7.0f && best<0.88f) pitch=smoothedPitch; }
        smoothedPitch += alpha*(pitch-smoothedPitch); confidence=best;
    }
}

void PitchForgeAudioProcessor::SmoothPitchShifter::prepare(double sampleRate)
{
    fs = sampleRate;
    for (int i = 0; i < grainSize; ++i)
        window[i] = 0.5f - 0.5f * std::cos(2.0f * pi * (float) i / (float) (grainSize - 1));
    reset();
}

void PitchForgeAudioProcessor::SmoothPitchShifter::reset()
{
    ring.fill(0.0f);
    writePos = 0;
    phaseA = 0;
    phaseB = grainHop;
    readA = readB = 0.0;
    ready = false;
    smoothedRatio = 1.0f;
}

double PitchForgeAudioProcessor::SmoothPitchShifter::wrap(double x)
{
    while (x < 0.0) x += maxDelay;
    while (x >= maxDelay) x -= maxDelay;
    return x;
}

float PitchForgeAudioProcessor::SmoothPitchShifter::readAt(double pos) const noexcept
{
    pos = wrap(pos);
    const int i0 = (int) pos;
    const int i1 = (i0 + 1) % maxDelay;
    const float frac = (float) (pos - (double) i0);
    return ring[i0] + frac * (ring[i1] - ring[i0]);
}

float PitchForgeAudioProcessor::SmoothPitchShifter::process(float input, float ratio)
{
    // Dual-head, Hann-windowed granular pitch shifting. Each head is relaunched
    // independently at a half-grain interval; the old implementation relaunched
    // both heads together, which produced the audible chopping/crackling.
    ring[writePos] = input;

    if (!ready)
    {
        readA = wrap((double) writePos - baseDelay);
        readB = wrap(readA + grainHop * 0.5);
        phaseA = 0;
        phaseB = grainHop;
        ready = true;
    }

    const float targetRatio = juce::jlimit(0.5f, 2.0f, ratio);
    // Smooth pitch-ratio modulation over ~5 ms to prevent zippering from the detector.
    const float smoothing = 1.0f - std::exp(-1.0f / (float) (fs * 0.005));
    smoothedRatio += smoothing * (targetRatio - smoothedRatio);

    const float a = window[juce::jlimit(0, grainSize - 1, phaseA)];
    const float b = window[juce::jlimit(0, grainSize - 1, phaseB)];
    const float sum = juce::jmax(0.25f, a + b);
    float out = (readAt(readA) * a + readAt(readB) * b) / sum;

    readA = wrap(readA + smoothedRatio);
    readB = wrap(readB + smoothedRatio);

    if (++phaseA >= grainSize)
    {
        phaseA = 0;
        readA = wrap((double) writePos - baseDelay);
    }

    if (++phaseB >= grainSize)
    {
        phaseB = 0;
        readB = wrap((double) writePos - baseDelay);
    }

    writePos = (writePos + 1) % maxDelay;
    return out;
}

void PitchForgeAudioProcessor::prepareToPlay(double sampleRate,int samplesPerBlock)
{
    fs=sampleRate; blockSize=samplesPerBlock; detector.prepare(sampleRate); for(auto& s:shifters)s.prepare(sampleRate);
    correctionSemitones=heldSemitones=0.0f; stableBlocks=0; lastTargetMidiInternal=-1;
    inputPitch=outputPitch=confidence=correctionCents=0.0f; detectedMidi=targetMidi=-1;
    setLatencySamples(0);
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
    int nearest=(int)std::lround(midi); int t=chrom?nearest:nearestScaleNote(nearest,key,scale); targetMidi.store(t); return midiToHz((float)t);
}
float PitchForgeAudioProcessor::getSpeedCoefficient(float speedMs) const
{
    // Map -3..200ms to a musically useful attack coefficient. Negative speed is deliberately
    // aggressive; it does not mean a negative physical time.
    if(speedMs<=0.0f) return 0.65f;
    return juce::jlimit(0.002f,0.65f,1.0f-std::exp(-1.0f/(fs*(speedMs*0.001f)+1.0f)));
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

    // The pitch shifter has real, fixed algorithmic latency. Report that latency
    // and delay the dry path by the same amount so Mix never creates comb filtering.
    const int algorithmicLatency = shifters[0].getLatencySamples();
    setLatencySamples(algorithmicLatency);

    const int stabilizerMs = stabilizer == 1 ? 40 : (stabilizer == 2 ? 80 : (stabilizer == 3 ? 200 : 0));
    const int requiredSamples = (int) (fs * stabilizerMs * 0.001);

    for (int i = 0; i < n; ++i)
    {
        const float leftIn = buffer.getSample(0, i);
        const float rightIn = ch > 1 ? buffer.getSample(1, i) : leftIn;
        const float mono = 0.5f * (leftIn + rightIn);

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
            if (std::abs(heldSemitones - capped) < 0.35f)
                effectiveSpeed += sustain * 60.0f;
            effectiveSpeed = juce::jlimit(-3.0f, 200.0f, effectiveSpeed);
            const float coeff = getSpeedCoefficient(effectiveSpeed);

            if (stabilizer > 0 && !lowLatency)
            {
                if (lastTargetMidiInternal == targetMidi.load())
                    stableBlocks += 1;
                else
                    stableBlocks = 0;

                lastTargetMidiInternal = targetMidi.load();
                const bool stableLongEnough = stableBlocks * juce::jmax(1, blockSize) >= requiredSamples;
                if (!stableLongEnough)
                    capped = heldSemitones;
            }

            heldSemitones += coeff * (capped - heldSemitones);
            // Humanize reduces the amount of instantaneous correction without adding
            // random pitch jitter, which is what caused the previous roughness.
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
        const float ratio = std::pow(2.0f, applied / 12.0f);

        const float wetL = shifters[0].process(leftIn, ratio);
        const float wetR = shifters[1].process(rightIn, ratio);
        const float dryL = shifters[0].getDelayedDry();
        const float dryR = shifters[1].getDelayedDry();

        float left = wetL;
        float right = wetR;

        if (doubler && dMix > 0.001f)
        {
            const float widthGain = 0.5f + 0.5f * dWidth;
            const float detune = 7.0f * widthGain;
            const float dRatioL = std::pow(2.0f, (applied + detune) / 12.0f);
            const float dRatioR = std::pow(2.0f, (applied - detune) / 12.0f);
            const float dL = shifters[2].process(leftIn, dRatioL);
            const float dR = shifters[3].process(rightIn, dRatioR);
            left = wetL * (1.0f - dMix) + dL * dMix;
            right = wetR * (1.0f - dMix) + dR * dMix;
        }

        buffer.setSample(0, i, dryL + (left - dryL) * mix);
        if (ch > 1)
            buffer.setSample(1, i, dryR + (right - dryR) * mix);

        if (detected > 0.0f)
        {
            const float outputMidi = 69.0f + 12.0f * std::log2(detected / getReferenceHz()) + applied;
            outputPitch.store(midiToHz(outputMidi));
        }
        else
        {
            outputPitch.store(0.0f);
        }
    }
}

void PitchForgeAudioProcessor::getStateInformation(juce::MemoryBlock& dest){if(auto xml=apvts.copyState().createXml())copyXmlToBinary(*xml,dest);}
void PitchForgeAudioProcessor::setStateInformation(const void* data,int size){if(auto xml=getXmlFromBinary(data,size))if(xml->hasTagName(apvts.state.getType()))apvts.replaceState(juce::ValueTree::fromXml(*xml));}
juce::AudioProcessorEditor* PitchForgeAudioProcessor::createEditor(){return new PitchForgeAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new PitchForgeAudioProcessor();}
