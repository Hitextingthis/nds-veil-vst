#include "PluginEditor.h"
#include <cmath>

namespace {
juce::Colour stage() { return juce::Colour(0xFF0D0C0A); }
juce::Colour panel() { return juce::Colour(0xFF171410); }
juce::Colour ink() { return juce::Colour(0xFFF5EFE6); }
juce::Colour secondary() { return juce::Colour(0xFFB8AC9A); }
juce::Colour hairline() { return juce::Colour(0xFF2E2820); }
juce::Colour accent() { return juce::Colour(0xFFE85D04); }
juce::Colour fieldTint() { return juce::Colour(0xFFFF4D2E); }

void styleKnob(juce::Slider& s) {
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 16);
    s.setColour(juce::Slider::rotarySliderFillColourId, accent());
    s.setColour(juce::Slider::rotarySliderOutlineColourId, hairline());
    s.setColour(juce::Slider::thumbColourId, accent());
    s.setColour(juce::Slider::textBoxTextColourId, ink());
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentWhite);
}
void styleLabel(juce::Label& l, const juce::String& text, int size = 12) {
    l.setText(text, juce::dontSendNotification);
    l.setFont(juce::Font(juce::FontOptions(static_cast<float>(size))));
    l.setColour(juce::Label::textColourId, secondary());
    l.setJustificationType(juce::Justification::centred);
}
void styleToggle(juce::ToggleButton& b, const juce::String& text) {
    b.setButtonText(text);
    b.setColour(juce::ToggleButton::textColourId, ink());
    b.setColour(juce::ToggleButton::tickColourId, accent());
}
void styleCombo(juce::ComboBox& c) {
    c.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF221E19));
    c.setColour(juce::ComboBox::outlineColourId, hairline());
    c.setColour(juce::ComboBox::textColourId, ink());
    c.setColour(juce::ComboBox::arrowColourId, accent());
}
float dbOf(float v) { return 20.0f * std::log10(std::max(0.0001f, v)); }
} // namespace

void FieldView::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    g.fillAll(stage());
    const float veil = processor.getVeilPeak();
    const float in = processor.getInPeak();
    const float cx = r.getWidth() / 2.0f, cy = r.getHeight() / 2.0f;
    const float maxR = std::min(r.getWidth(), r.getHeight()) * 0.44f;
    // Veil rings: radius follows veil level, arcs rotate slowly.
    for (int i = 0; i < 5; ++i) {
        const float rr = maxR * (0.22f + i * 0.16f) * (1.0f + veil * 6.0f + in * 0.15f);
        g.setColour(fieldTint().withAlpha(0.55f));
        g.drawEllipse(cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 2.0f + veil * 6.0f);
        g.setColour(accent().withAlpha(0.9f));
        juce::Path arc;
        arc.addArc(cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, time * 0.4f + i * 0.7f, time * 0.4f + i * 0.7f + 1.3f, true);
        g.strokePath(arc, juce::PathStrokeType(3.0f));
    }
    g.setColour(fieldTint());
    g.fillEllipse(cx - 14.0f - veil * 30.0f, cy - 14.0f - veil * 30.0f, 28.0f + veil * 60.0f, 28.0f + veil * 60.0f);
    // Waveform ribbon at the bottom of the field.
    juce::Path ribbon;
    for (int x = 0; x <= (int)r.getWidth(); x += 5) {
        const float y = r.getHeight() - 24.0f + std::sin(x / 55.0f + time * 3.0f) * (5.0f + veil * 40.0f + in * 12.0f);
        if (x == 0) ribbon.startNewSubPath(x, y); else ribbon.lineTo(x, y);
    }
    g.setColour(accent().withAlpha(0.85f));
    g.strokePath(ribbon, juce::PathStrokeType(2.0f));
}

VeilAudioEditor::VeilAudioEditor(VeilAudioProcessor& p)
    : AudioProcessorEditor(p), processor(p), field(p) {
    setSize(1020, 600);
    auto& apvts = processor.apvts;

    titleLabel.setText("NDS Veil", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId, ink());
    titleLabel.setJustificationType(juce::Justification::left);
    addAndMakeVisible(titleLabel);
    presetNameLabel.setText("Vocal Focus", juce::dontSendNotification);
    presetNameLabel.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    presetNameLabel.setColour(juce::Label::textColourId, ink());
    presetNameLabel.setJustificationType(juce::Justification::left);
    addAndMakeVisible(presetNameLabel);
    presetDescLabel.setText("Loud vocal-forward master — upper-mid veil. Before the final limiter.", juce::dontSendNotification);
    presetDescLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
    presetDescLabel.setColour(juce::Label::textColourId, secondary());
    presetDescLabel.setJustificationType(juce::Justification::left);
    addAndMakeVisible(presetDescLabel);

    addAndMakeVisible(field);
    for (int i = 0; i < 5; ++i) {
        auto& b = presetButtons[i];
        b.setButtonText(ndsveil::kPresets[i].name);
        b.setColour(juce::TextButton::buttonColourId, panel());
        b.setColour(juce::TextButton::textColourOffId, ink());
        b.onClick = [this, i] { setPreset(i); };
        addAndMakeVisible(b);
    }
    setPreset(3); // default UI state matches Vocal Focus params

    styleToggle(onToggle, "ON");
    addAndMakeVisible(onToggle);
    buttonAtts.push_back(std::make_unique<ButtonAtt>(apvts, "on", onToggle));

    auto knob = [&](juce::Slider& s, juce::Label& l, const juce::String& text, const juce::String& id) {
        styleKnob(s); styleLabel(l, text); addAndMakeVisible(s); addAndMakeVisible(l);
        sliderAtts.push_back(std::make_unique<SliderAtt>(apvts, id, s));
    };
    knob(veilSlider, veilLabel, "VEIL dB", "veilDb");
    knob(focusSlider, focusLabel, "FOCUS", "focus");
    knob(widthSlider, widthLabel, "WIDTH", "width");
    knob(mixSlider, mixLabel, "MIX", "mix");
    knob(trimSlider, trimLabel, "TRIM", "trimDb");
    // Ceiling is a linear slider (a threshold, not a knob).
    ceilingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    ceilingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 16);
    ceilingSlider.setColour(juce::Slider::trackColourId, hairline());
    ceilingSlider.setColour(juce::Slider::thumbColourId, accent());
    ceilingSlider.setColour(juce::Slider::textBoxTextColourId, ink());
    ceilingSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentWhite);
    addAndMakeVisible(ceilingSlider);
    styleLabel(ceilingLabel, "CEILING dBTP", 12);
    addAndMakeVisible(ceilingLabel);
    sliderAtts.push_back(std::make_unique<SliderAtt>(apvts, "ceilingDb", ceilingSlider));

    auto choice = [&](juce::ComboBox& c, juce::Label& l, const juce::String& text, const juce::String& id, juce::StringArray items) {
        styleCombo(c); styleLabel(l, text); addAndMakeVisible(c); addAndMakeVisible(l);
        for (int i = 0; i < items.size(); ++i) c.addItem(items[i], i + 1);
        comboAtts.push_back(std::make_unique<ComboAtt>(apvts, id, c));
    };
    choice(monitorCombo, monitorLabel, "MONITOR", "monitor", { "LR", "Mono", "Delta" });
    choice(targetCombo, targetLabel, "TARGET", "target", { "Streaming -14 LUFS", "Loud label -9 LUFS", "Dynamic -16 LUFS", "Custom" });

    startTimerHz(30);
}

VeilAudioEditor::~VeilAudioEditor() { stopTimer(); }

void VeilAudioEditor::setPreset(int index) {
    currentPreset = index;
    processor.applyPreset(index);
    presetNameLabel.setText(ndsveil::kPresets[index].name, juce::dontSendNotification);
    presetDescLabel.setText(juce::String(ndsveil::kPresets[index].job) + " — veil " +
                            juce::String(ndsveil::kPresets[index].veilDb, 0) + " dB. Before the final limiter.",
                            juce::dontSendNotification);
    for (int i = 0; i < 5; ++i)
        presetButtons[i].setColour(juce::TextButton::buttonColourId, i == index ? accent() : panel());
}

void VeilAudioEditor::timerCallback() {
    field.time += 0.016f;
    field.repaint();
    repaint();
}

void VeilAudioEditor::paint(juce::Graphics& g) {
    g.fillAll(stage());
    g.setColour(secondary());
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText("Preset psychoacoustic enhancer — for mastering. Place BEFORE the final limiter.",
               180, 56, 700, 18, juce::Justification::left);
    // Meters in the right panel area.
    auto bar = [&](int x, int y, float v, const juce::String& label) {
        g.setColour(secondary());
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText(label, x, y - 14, 60, 12, juce::Justification::left);
        g.setColour(hairline());
        g.fillRect(x, y, 150, 6);
        g.setColour(accent());
        g.fillRect(x, y, static_cast<int>(150.0f * juce::jlimit(0.0f, 1.0f, v)), 6);
    };
    const float ceiling = processor.apvts.getRawParameterValue("ceilingDb") != nullptr
        ? processor.apvts.getRawParameterValue("ceilingDb")->load() : -1.0f;
    bar(748, 512, processor.getInPeak(), "IN");
    bar(748, 532, processor.getOutPeak(), "OUT");
    bar(748, 552, std::min(1.0f, processor.getVeilPeak() * 40.0f), "VEIL x40");
    if (dbOf(processor.getOutPeak()) > ceiling) {
        g.setColour(juce::Colours::red);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText("OVER CEILING — check before sending", 748, 562, 260, 16, juce::Justification::left);
    } else {
        g.setColour(secondary());
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText("Peak meters only.", 748, 562, 260, 16, juce::Justification::left);
        g.drawText("Verify LUFS/TP in RX / StreamCheck.", 748, 578, 260, 16, juce::Justification::left);
    }
}

void VeilAudioEditor::resized() {
    titleLabel.setBounds(16, 12, 160, 32);
    presetNameLabel.setBounds(180, 8, 220, 26);
    presetDescLabel.setBounds(180, 34, 560, 18);
    onToggle.setBounds(900, 18, 60, 24);
    // Preset rail.
    for (int i = 0; i < 5; ++i)
        presetButtons[i].setBounds(16, 80 + i * 62, 200, 52);
    field.setBounds(232, 80, 480, 420);
    // Right controls: knobs in two columns, then combos, then ceiling.
    veilSlider.setBounds(728, 80, 88, 88); veilLabel.setBounds(728, 162, 88, 16);
    focusSlider.setBounds(836, 80, 88, 88); focusLabel.setBounds(836, 162, 88, 16);
    widthSlider.setBounds(728, 190, 88, 88); widthLabel.setBounds(728, 272, 88, 16);
    mixSlider.setBounds(836, 190, 88, 88); mixLabel.setBounds(836, 272, 88, 16);
    trimSlider.setBounds(782, 300, 88, 88); trimLabel.setBounds(782, 382, 88, 16);
    monitorLabel.setBounds(728, 410, 90, 16); monitorCombo.setBounds(728, 428, 196, 24);
    targetLabel.setBounds(728, 458, 90, 16); targetCombo.setBounds(728, 476, 196, 24);
    // Meter bars are drawn in paint() at y 512/532/552, x 748.
    ceilingLabel.setBounds(16, 540, 110, 16);
    ceilingSlider.setBounds(130, 536, 220, 24);
}
