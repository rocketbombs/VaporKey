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
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 13);
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
        label.setBounds (r.removeFromTop (14));
        slider.setBounds (r);
    }

    juce::Slider slider;
    juce::Label  label;
    juce::String name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class VaporCombo : public juce::Component
{
public:
    VaporCombo (juce::AudioProcessorValueTreeState& s, const juce::String& paramID,
                const juce::String& displayName, const juce::StringArray& items)
    {
        for (int i = 0; i < items.size(); ++i) box.addItem (items[i], i + 1);
        addAndMakeVisible (box);
        label.setText (displayName, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, VK::Colors::neonCyan);
        addAndMakeVisible (label);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, paramID, box);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (14));
        box.setBounds (r.reduced (4, 6));
    }
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class VaporToggle : public juce::Component
{
public:
    VaporToggle (juce::AudioProcessorValueTreeState& s, const juce::String& paramID, const juce::String& text)
    {
        btn.setButtonText (text);
        btn.setClickingTogglesState (true);
        addAndMakeVisible (btn);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, paramID, btn);
    }
    void resized() override { btn.setBounds (getLocalBounds().reduced (2)); }
    juce::TextButton btn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

class WavetableDisplay : public juce::Component, private juce::Timer
{
public:
    WavetableDisplay (juce::AudioProcessorValueTreeState& s, int oscIndex)
        : apvts (s), idx (oscIndex)
    {
        startTimerHz (24);
    }

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override { repaint(); }
    juce::AudioProcessorValueTreeState& apvts;
    int idx;
};

// --- Pages ---

class OscPage : public juce::Component
{
public:
    explicit OscPage (VaporKeyAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    struct OscUI {
        std::unique_ptr<VaporToggle> on;
        std::unique_ptr<VaporCombo>  shape;
        std::unique_ptr<WavetableDisplay> display;
        std::unique_ptr<VaporKnob> position, level, pan, coarse, fine, unison, detune, phase;
        juce::Label title;
    };
    OscUI oscUI[3];

    std::unique_ptr<VaporToggle> subOn, noiseOn;
    std::unique_ptr<VaporCombo>  subShape, noiseColor;
    std::unique_ptr<VaporKnob>   subOct, subLevel, noiseLevel;

    std::unique_ptr<VaporKnob>   glide, bendRange;
    std::unique_ptr<VaporToggle> mono, legato;

    VaporKeyAudioProcessor& proc;
};

class FilterEnvPage : public juce::Component
{
public:
    explicit FilterEnvPage (VaporKeyAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    std::unique_ptr<VaporKnob>  cut, res, env, drive, key;
    std::unique_ptr<VaporCombo> type;
    std::unique_ptr<VaporKnob>  aA, aD, aS, aR, aVel;
    std::unique_ptr<VaporKnob>  mA, mD, mS, mR, fVel;
    std::unique_ptr<VaporKnob>  pAmt, pDecay;
    std::unique_ptr<VaporKnob>  grit, vibe, drift, sat;
};

class ModPage : public juce::Component
{
public:
    explicit ModPage (VaporKeyAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    std::unique_ptr<VaporCombo> l1Shape, l2Shape, l1Div, l2Div;
    std::unique_ptr<VaporToggle> l1Sync, l2Sync;
    std::unique_ptr<VaporKnob> l1Rate, l1Amt, l2Rate, l2Amt;

    struct MacroUI {
        std::unique_ptr<VaporKnob>  val, amt;
        std::unique_ptr<VaporCombo> dest;
    };
    MacroUI macros[SynthParams::kNumMacros];
};

class FxPage : public juce::Component
{
public:
    explicit FxPage (VaporKeyAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    std::unique_ptr<VaporKnob>  distDrive, distMix;
    std::unique_ptr<VaporCombo> distType;

    std::unique_ptr<VaporKnob>  chMix, chRate, chDepth;
    std::unique_ptr<VaporKnob>  phMix, phRate, phDepth, phFb;

    std::unique_ptr<VaporKnob>  eqLow, eqMid, eqMidF, eqHigh;

    std::unique_ptr<VaporKnob>  dlMix, dlTime, dlFb;
    std::unique_ptr<VaporToggle> dlSync;
    std::unique_ptr<VaporCombo> dlDiv;

    std::unique_ptr<VaporKnob>  rvMix, rvSize, rvDamp;

    std::unique_ptr<VaporToggle> compOn;
    std::unique_ptr<VaporKnob>  compThr, compRatio, compAtk, compRel, compMakeup;
};

class MasterPage : public juce::Component
{
public:
    explicit MasterPage (VaporKeyAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    VaporKeyAudioProcessor& proc;
    std::unique_ptr<VaporKnob> gain, width;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    juce::Label about;
};

// Top-level editor with TabbedComponent
class VaporKeyAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VaporKeyAudioProcessorEditor (VaporKeyAudioProcessor&);
    ~VaporKeyAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    VaporKeyAudioProcessor& proc;
    VaporLookAndFeel lnf;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VaporKeyAudioProcessorEditor)
};
