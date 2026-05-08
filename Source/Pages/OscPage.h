#pragma once
#include <JuceHeader.h>
#include "../Widgets/VaporWidgets.h"
#include "../Widgets/WavetableDisplay.h"

class VaporKeyAudioProcessor;

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
