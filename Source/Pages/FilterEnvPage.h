#pragma once
#include <JuceHeader.h>
#include "../Widgets/VaporWidgets.h"

class VaporKeyAudioProcessor;

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
