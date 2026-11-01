// NDS Veil — JUCE processor shell around the pure-C++ veil DSP.
// All parameters live in APVTS so FL Studio automation and preset
// recall work. Preset buttons in the editor are shortcuts: they
// set the Veil / Focus / Width APVTS parameters to the preset's
// values. Recall therefore restores those values, not a name.
// Monitor modes: LR (normal), Mono (full-output mono collapse,
// veil included — a listening check), Delta (veil only).
// Bypass / Mix 0 returns dry exactly, overriding monitor modes.
// Ceiling and Target are meter references only: they never limit
// or change loudness. Place this plugin BEFORE the final limiter.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include <vector>
#include "VeilDSP.h"

class VeilAudioProcessor : public juce::AudioProcessor {
public:
    VeilAudioProcessor();
    ~VeilAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NDS Veil"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    float getInPeak() const { return inPeak.load(); }
    float getOutPeak() const { return outPeak.load(); }
    float getVeilPeak() const { return veilPeak.load(); }
    float getInRms() const { return inRms.load(); }

    void applyPreset(int index); // editor shortcut, sets APVTS params

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParams();

    std::vector<ndsveil::VeilChannel> channels;
    ndsveil::NoiseSource commonNoise{ 0x9E3779B9u };
    double currentSampleRate = 48000.0;

    std::atomic<float> inPeak{ 0.0f }, outPeak{ 0.0f }, veilPeak{ 0.0f }, inRms{ 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VeilAudioProcessor)
};
