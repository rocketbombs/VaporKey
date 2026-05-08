#pragma once
#include <JuceHeader.h>
#include "../Widgets/VaporWidgets.h"

class VaporKeyAudioProcessor;

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
