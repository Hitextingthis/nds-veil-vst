// NDS Veil editor — Field System v1, adapted to JUCE.
// Dark warm stage, serif product name, script sign-off only,
// preset rail left, living field centre, veil controls right.
// The field is the only animated part; chrome stays still.
// Meters are peak/RMS from the processor, not LUFS/TP measurement:
// verify a real master in RX / StreamCheck before sending.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class FieldView : public juce::Component {
public:
    explicit FieldView(VeilAudioProcessor& p) : processor(p) {}
    void paint(juce::Graphics& g) override;
    float time = 0.0f;
private:
    VeilAudioProcessor& processor;
};

class VeilAudioEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit VeilAudioEditor(VeilAudioProcessor& p);
    ~VeilAudioEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void setPreset(int index);

    VeilAudioProcessor& processor;
    FieldView field;

    juce::Label presetNameLabel, presetDescLabel, titleLabel;
    juce::TextButton presetButtons[5];
    int currentPreset = 3; // Vocal Focus default
    juce::ToggleButton onToggle;
    juce::ComboBox monitorCombo, targetCombo, carrierCombo;
    juce::Label monitorLabel, targetLabel, carrierLabel, ceilingLabel;
    juce::Slider veilSlider, focusSlider, widthSlider, mixSlider, trimSlider, ceilingSlider;
    juce::Label veilLabel, focusLabel, widthLabel, mixLabel, trimLabel;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::vector<std::unique_ptr<SliderAtt>> sliderAtts;
    std::vector<std::unique_ptr<ButtonAtt>> buttonAtts;
    std::vector<std::unique_ptr<ComboAtt>> comboAtts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VeilAudioEditor)
};
