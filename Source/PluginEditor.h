#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"

// ----- Widget primitives -----

class VaporKnob : public juce::Component
{
public:
    VaporKnob (juce::AudioProcessorValueTreeState& s, const juce::String& paramID, const juce::String& displayName);
    void resized() override;

    juce::Slider slider;
    juce::Label  label;
    juce::String name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class VaporCombo : public juce::Component
{
public:
    VaporCombo (juce::AudioProcessorValueTreeState& s, const juce::String& paramID,
                const juce::String& displayName, const juce::StringArray& items);
    void resized() override;

    juce::ComboBox box;
    juce::Label    label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class VaporToggle : public juce::Component
{
public:
    VaporToggle (juce::AudioProcessorValueTreeState& s, const juce::String& paramID, const juce::String& text);
    void resized() override;

    juce::TextButton btn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// Click-drag-to-scrub wavetable display (drives the position parameter).
class WavetableDisplay : public juce::Component, private juce::Timer
{
public:
    WavetableDisplay (juce::AudioProcessorValueTreeState& s, int oscIndex);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    void timerCallback() override { repaint(); }
    void setPositionFromMouse (const juce::MouseEvent& e);

    juce::AudioProcessorValueTreeState& apvts;
    int idx;
};

// ----- Pages -----

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
    juce::ListBox presetList;
    juce::TextButton prevBtn { "<  PREV" }, nextBtn { "NEXT  >" };
    juce::Label presetLabel, presetNowLabel, brand, tagline, copy;

    class PresetListModel : public juce::ListBoxModel
    {
    public:
        explicit PresetListModel (MasterPage& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    private:
        MasterPage& owner;
    };
    std::unique_ptr<PresetListModel> presetModel;
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
