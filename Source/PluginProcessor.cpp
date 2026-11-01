#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
float paramFloat(juce::AudioProcessorValueTreeState& apvts, const char* id) {
    auto* p = apvts.getRawParameterValue(id);
    return p != nullptr ? p->load() : 0.0f;
}
bool paramBool(juce::AudioProcessorValueTreeState& apvts, const char* id) {
    return paramFloat(apvts, id) > 0.5f;
}
void setParam(juce::AudioProcessorValueTreeState& apvts, const char* id, float value) {
    auto* p = apvts.getParameter(id);
    if (p != nullptr) p->setValueNotifyingHost(p->convertTo0to1(value));
}
} // namespace

VeilAudioProcessor::VeilAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParams()) {}

VeilAudioProcessor::~VeilAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout VeilAudioProcessor::createParams() {
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{ "carrier", 1 }, "Carrier",
        StringArray{ "Noise", "Source" }, 1));
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "veilDb", 1 }, "Veil Level",
        NormalisableRange<float>(-60.0f, -40.0f, 0.1f), -50.0f, "dB"));
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "focus", 1 }, "Focus",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.60f, ""));
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "width", 1 }, "Width",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.40f, ""));
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "mix", 1 }, "Mix",
        NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f, "%"));
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "trimDb", 1 }, "Output Trim",
        NormalisableRange<float>(-6.0f, 6.0f, 0.1f), 0.0f, "dB"));
    p.push_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "on", 1 }, "On", true));
    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{ "monitor", 1 }, "Monitor",
        StringArray{ "LR", "Mono", "Delta" }, 0));
    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{ "target", 1 }, "Destination Target",
        StringArray{ "Streaming -14 LUFS", "Loud label -9 LUFS", "Dynamic -16 LUFS", "Custom" }, 1));
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "ceilingDb", 1 }, "True Peak Ceiling",
        NormalisableRange<float>(-2.0f, 0.0f, 0.1f), -1.0f, "dBTP"));
    return { p.begin(), p.end() };
}

void VeilAudioProcessor::applyPreset(int index) {
    if (index < 0 || index >= 5) return;
    const auto& pr = ndsveil::kPresets[index];
    setParam(apvts, "veilDb", pr.veilDb);
    setParam(apvts, "focus", pr.focus);
    setParam(apvts, "width", pr.width);
    setParam(apvts, "mix", 100.0f);
    // v0.2: presets are source-carrier presets. A user who wants
    // the old noise carrier can switch Carrier back to Noise.
    auto* carrier = apvts.getParameter("carrier");
    if (carrier != nullptr) carrier->setValueNotifyingHost(carrier->convertTo0to1(1.0f));
}

void VeilAudioProcessor::prepareToPlay(double sampleRate, int) {
    currentSampleRate = sampleRate;
    channels.clear();
    channels.emplace_back(0x12345678u, false);
    channels.emplace_back(0x87654321u, true);
    for (auto& ch : channels) ch.prepare(sampleRate);
}

void VeilAudioProcessor::releaseResources() {}

bool VeilAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet()) return false;
    const auto set = layouts.getMainOutputChannelSet();
    return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
}

void VeilAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) return;
    auto inBus = getBusBuffer(buffer, true, 0);
    auto outBus = getBusBuffer(buffer, false, 0);
    const int inCh = inBus.getNumChannels();
    const int outCh = outBus.getNumChannels();
    if (inCh == 0 || outCh == 0) return;
    const int numCh = std::min({ inCh, outCh, static_cast<int>(channels.size()) });
    if (numCh <= 0) return;
    for (int ch = numCh; ch < outCh; ++ch) outBus.clear(ch, 0, buffer.getNumSamples());
    const int numSamples = buffer.getNumSamples();

    const bool on = paramBool(apvts, "on");
    const ndsveil::Carrier carrier = static_cast<int>(paramFloat(apvts, "carrier")) == 1
        ? ndsveil::Carrier::Source : ndsveil::Carrier::Noise;
    const float mix = paramFloat(apvts, "mix") / 100.0f;
    const float veilDb = paramFloat(apvts, "veilDb");
    const float focus = paramFloat(apvts, "focus");
    const float width = paramFloat(apvts, "width");
    const float trimGain = ndsveil::dbToGain(paramFloat(apvts, "trimDb"));
    const int monitor = static_cast<int>(paramFloat(apvts, "monitor")); // 0 LR, 1 Mono, 2 Delta

    // Bypass / Mix 0: dry exactly, regardless of monitor.
    if (!on || mix <= 0.0f) {
        float inPeakVal = 0.0f, sumSq = 0.0f;
        for (int ch = 0; ch < numCh; ++ch) {
            const float* d = inBus.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i) { inPeakVal = std::max(inPeakVal, std::abs(d[i])); sumSq += d[i] * d[i]; }
            if (outBus.getWritePointer(ch) != d)
                outBus.copyFrom(ch, 0, inBus, ch, 0, numSamples);
        }
        inPeak.store(inPeakVal);
        outPeak.store(inPeakVal);
        veilPeak.store(0.0f);
        inRms.store(std::sqrt(sumSq / std::max(1, numSamples * numCh)));
        return;
    }

    std::vector<float> dryL(numSamples, 0.0f), dryR(numSamples, 0.0f);
    std::vector<float> veilL(numSamples, 0.0f), veilR(numSamples, 0.0f);
    float inPeakVal = 0.0f, veilPeakVal = 0.0f, sumSq = 0.0f;

    for (int ch = 0; ch < numCh; ++ch) {
        const float* input = inBus.getReadPointer(ch);
        auto& dry = (ch == 0 ? dryL : dryR);
        auto& veil = (ch == 0 ? veilL : veilR);
        for (int i = 0; i < numSamples; ++i) {
            dry[i] = input[i];
            inPeakVal = std::max(inPeakVal, std::abs(input[i]));
            sumSq += input[i] * input[i];
        }
        // Common noise is generated once per frame (see below loop
        // for channel 0); for channel 1 the same common sample must
        // be reused, so generate the whole common sequence first.
        (void)veil;
    }
    // Common noise sequence for this block.
    std::vector<float> common(numSamples, 0.0f);
    for (int i = 0; i < numSamples; ++i) common[i] = commonNoise.next();
    for (int ch = 0; ch < numCh; ++ch) {
        const float* input = inBus.getReadPointer(ch);
        auto& veil = (ch == 0 ? veilL : veilR);
        for (int i = 0; i < numSamples; ++i) {
            veil[i] = channels[ch].process(input[i], common[i], width, mix, carrier, focus, veilDb);
            veilPeakVal = std::max(veilPeakVal, std::abs(veil[i]));
        }
    }

    float outPeakVal = 0.0f;
    auto writeStereo = [&](float* outL, float* outR) {
        for (int i = 0; i < numSamples; ++i) {
            float oL = 0.0f, oR = 0.0f;
            if (monitor == 2) { // Delta: veil only
                oL = veilL[i] * trimGain; oR = (numCh > 1 ? veilR[i] : veilL[i]) * trimGain;
            } else if (monitor == 1) { // Mono: full output collapsed
                const float m = 0.5f * ((dryL[i] + veilL[i]) + (numCh > 1 ? (dryR[i] + veilR[i]) : (dryL[i] + veilL[i])));
                oL = m * trimGain; oR = m * trimGain;
            } else {
                oL = (dryL[i] + veilL[i]) * trimGain;
                oR = (numCh > 1 ? (dryR[i] + veilR[i]) : (dryL[i] + veilL[i])) * trimGain;
            }
            outL[i] = oL; if (outR != nullptr) outR[i] = oR;
            outPeakVal = std::max({ outPeakVal, std::abs(oL), std::abs(oR) });
        }
    };
    if (numCh == 1) {
        writeStereo(outBus.getWritePointer(0), nullptr);
        // Mono host: the single output is the mono result; the
        // writeStereo mono path above already handles it via oL.
        if (monitor != 1) {
            // For LR/Delta on a mono bus, oL already holds the
            // correct single-channel result.
        }
    } else {
        writeStereo(outBus.getWritePointer(0), outBus.getWritePointer(1));
    }

    inPeak.store(inPeakVal);
    outPeak.store(outPeakVal);
    veilPeak.store(veilPeakVal);
    inRms.store(std::sqrt(sumSq / std::max(1, numSamples * numCh)));
}

juce::AudioProcessorEditor* VeilAudioProcessor::createEditor() {
    return new VeilAudioEditor(*this);
}

void VeilAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VeilAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new VeilAudioProcessor();
}
