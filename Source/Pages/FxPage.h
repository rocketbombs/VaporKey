#pragma once
#include <JuceHeader.h>
#include "../Widgets/VaporWidgets.h"
#include "../Widgets/EqCurve.h"

class VaporKeyAudioProcessor;

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
    std::unique_ptr<EqCurve>    eqCurve;
    std::unique_ptr<VaporKnob>  dlMix, dlTime, dlFb;
    std::unique_ptr<VaporToggle> dlSync;
    std::unique_ptr<VaporCombo> dlDiv;
    std::unique_ptr<VaporKnob>  rvMix, rvSize, rvDamp;
    std::unique_ptr<VaporToggle> compOn;
    std::unique_ptr<VaporKnob>  compThr, compRatio, compAtk, compRel, compMakeup;
};
