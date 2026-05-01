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

class VaporKeyAudioProcessor;

// Click-drag-to-scrub wavetable display (drives the position parameter).
// Also accepts .wav files via drag-and-drop or right-click "Load .wav..." to
// fill the per-oscillator Custom wavetable slot.
class WavetableDisplay : public juce::Component,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
{
public:
    WavetableDisplay (VaporKeyAudioProcessor& proc, int oscIndex);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit  (const juce::StringArray&) override;
    void filesDropped  (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override { repaint(); }
    void setPositionFromMouse (const juce::MouseEvent& e);
    void showLoadMenu();
    void chooseWavFile();

    VaporKeyAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    int idx;
    bool dragHover = false;
    std::unique_ptr<juce::FileChooser> chooser;
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

class ArpPage : public juce::Component
{
public:
    explicit ArpPage (VaporKeyAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    std::unique_ptr<VaporToggle> on, latch;
    std::unique_ptr<VaporCombo>  mode, div;
    std::unique_ptr<VaporKnob>   octaves, gate, swing;
    juce::Label                   blurb;
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

    // Combined factory + user preset listing.
    struct PresetEntry { juce::String name; bool isFactory; int category; };
    std::vector<PresetEntry> entries;
    std::vector<int>         visibleRows;   // entries indices that match the active filter
    void rebuildEntries();
    void rebuildVisible();
    int  findEntryIndex (const juce::String& name, bool isFactory) const;
    int  visibleRowFromEntryIndex (int entryIndex) const;
    void loadEntry (int index);
    void refreshNowPlaying();

private:
    VaporKeyAudioProcessor& proc;
    std::unique_ptr<VaporKnob> gain, width;
    juce::ListBox presetList;
    juce::TextButton prevBtn { "<  PREV" }, nextBtn { "NEXT  >" };
    juce::TextButton saveBtn { "SAVE" }, renameBtn { "RENAME" }, deleteBtn { "DELETE" };
    juce::TextEditor nameField;
    juce::Label presetLabel, presetNowLabel, nameLabel, brand, tagline, copy;
    juce::ComboBox categoryFilter;     // "All / Bass / Lead / ... / User"
    juce::Label    categoryLabel;

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

    void onSave();
    void onRename();
    void onDelete();
    void stepPreset (int dir);
    void showStatus (const juce::String& msg, juce::Colour col);
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
