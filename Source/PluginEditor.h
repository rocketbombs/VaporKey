#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"

class VaporKnob : public juce::Component
{
public:
    VaporKnob (juce::AudioProcessorValueTreeState& s, const juce::String& paramID, const juce::String& displayName)
        : name (displayName)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 14);
        slider.setName (displayName);
        addAndMakeVisible (slider);

        label.setText (displayName, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, VK::Colors::neonCyan);
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramID, slider);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (16));
        slider.setBounds (r);
    }

    juce::Slider slider;
    juce::Label  label;
    juce::String name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class WavetableDisplay : public juce::Component, private juce::Timer
{
public:
    WavetableDisplay (juce::AudioProcessorValueTreeState& s, int oscIndex)
        : apvts (s), idx (oscIndex)
    {
        startTimerHz (24);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (VK::Colors::bg);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (VK::Colors::neonPink.withAlpha (0.35f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        // grid
        g.setColour (VK::Colors::grid);
        for (int i = 1; i < 4; ++i)
        {
            const float y = r.getY() + r.getHeight() * (float) i / 4.0f;
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        }

        const int shape = (int) apvts.getRawParameterValue ("osc" + juce::String (idx + 1) + "_shape")->load();
        const float pos = apvts.getRawParameterValue ("osc" + juce::String (idx + 1) + "_pos")->load();

        const auto& wt = WavetableLibrary::get().getTable (shape);
        juce::Path p;
        const int N = 256;
        for (int n = 0; n < N; ++n)
        {
            const float ph = (float) n / (float) N;
            const float v = wt.sample (pos, ph, 2);
            const float x = r.getX() + r.getWidth() * (float) n / (float) (N - 1);
            const float y = r.getCentreY() - v * r.getHeight() * 0.42f;
            if (n == 0) p.startNewSubPath (x, y);
            else        p.lineTo (x, y);
        }

        for (int i = 4; i > 0; --i)
        {
            g.setColour (VK::Colors::neonCyan.withAlpha (0.10f * (float) i));
            g.strokePath (p, juce::PathStrokeType ((float) i));
        }
        g.setColour (VK::Colors::neonCyan);
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }

private:
    void timerCallback() override { repaint(); }
    juce::AudioProcessorValueTreeState& apvts;
    int idx;
};

class VaporKeyAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VaporKeyAudioProcessorEditor (VaporKeyAudioProcessor&);
    ~VaporKeyAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    std::vector<juce::Rectangle<int>> sectionRects;
    std::vector<juce::String>         sectionTitles;
    std::vector<juce::Colour>         sectionColors;

private:
    void buildOscPanel (int i);

    VaporKeyAudioProcessor& proc;
    VaporLookAndFeel lnf;

    // OSC panel components (3 oscillators)
    struct OscUI {
        juce::ToggleButton onBtn;
        juce::ComboBox shapeBox;
        std::unique_ptr<WavetableDisplay> display;
        std::unique_ptr<VaporKnob> position, level, pan, coarse, fine, unison, detune;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shapeAtt;
        juce::Label title;
    };
    OscUI oscUI[3];

    // Filter
    std::unique_ptr<VaporKnob> kCut, kRes, kEnv;
    juce::ComboBox fTypeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fTypeAtt;

    // Envelopes
    std::unique_ptr<VaporKnob> kAmpA, kAmpD, kAmpS, kAmpR;
    std::unique_ptr<VaporKnob> kModA, kModD, kModS, kModR;

    // LFOs
    std::unique_ptr<VaporKnob> kLfo1Rate, kLfo1Amt, kLfo2Rate, kLfo2Amt;

    // Warmth
    std::unique_ptr<VaporKnob> kGrit, kVibe, kDrift, kSat;

    // FX
    std::unique_ptr<VaporKnob> kChorus, kDelay, kDelayTime, kDelayFb, kReverb;

    // Master
    std::unique_ptr<VaporKnob> kGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VaporKeyAudioProcessorEditor)
};
