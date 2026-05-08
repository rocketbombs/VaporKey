#pragma once
#include <JuceHeader.h>
#include "../Widgets/VaporWidgets.h"
#include "../Parameters.h"

class VaporKeyAudioProcessor;

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

    // Mod wheel + aftertouch behave like macros whose value is supplied by
    // MIDI rather than a knob - so they only need destination + amount.
    struct MidiSrcUI {
        std::unique_ptr<VaporKnob>  amt;
        std::unique_ptr<VaporCombo> dest;
    };
    MidiSrcUI mwUI, atUI;
};
