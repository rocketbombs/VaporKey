#include "FxPage.h"
#include "EditorLayout.h"
#include "../PluginProcessor.h"
#include "../LookAndFeel.h"

using namespace VK;
using namespace VKEditorLayout;

FxPage::FxPage (VaporKeyAudioProcessor& p)
{
    distDrive = std::make_unique<VaporKnob>  (p.apvts, "dist_drive", "Drive"); addAndMakeVisible (*distDrive);
    distMix   = std::make_unique<VaporKnob>  (p.apvts, "dist_mix",   "Mix");   addAndMakeVisible (*distMix);
    distType  = std::make_unique<VaporCombo> (p.apvts, "dist_type",  "Type", DistType::names()); addAndMakeVisible (*distType);

    chMix   = std::make_unique<VaporKnob> (p.apvts, "chorus",       "Mix");   addAndMakeVisible (*chMix);
    chRate  = std::make_unique<VaporKnob> (p.apvts, "chorus_rate",  "Rate");  addAndMakeVisible (*chRate);
    chDepth = std::make_unique<VaporKnob> (p.apvts, "chorus_depth", "Depth"); addAndMakeVisible (*chDepth);

    phMix   = std::make_unique<VaporKnob> (p.apvts, "phaser",       "Mix");   addAndMakeVisible (*phMix);
    phRate  = std::make_unique<VaporKnob> (p.apvts, "phaser_rate",  "Rate");  addAndMakeVisible (*phRate);
    phDepth = std::make_unique<VaporKnob> (p.apvts, "phaser_depth", "Depth"); addAndMakeVisible (*phDepth);
    phFb    = std::make_unique<VaporKnob> (p.apvts, "phaser_fb",    "FB");    addAndMakeVisible (*phFb);

    eqCurve = std::make_unique<EqCurve> (p.apvts); addAndMakeVisible (*eqCurve);

    dlMix   = std::make_unique<VaporKnob>   (p.apvts, "delay",      "Mix");  addAndMakeVisible (*dlMix);
    dlTime  = std::make_unique<VaporKnob>   (p.apvts, "delay_time", "Time"); addAndMakeVisible (*dlTime);
    dlFb    = std::make_unique<VaporKnob>   (p.apvts, "delay_fb",   "FB");   addAndMakeVisible (*dlFb);
    dlSync  = std::make_unique<VaporToggle> (p.apvts, "delay_sync", "SYNC"); addAndMakeVisible (*dlSync);
    dlDiv   = std::make_unique<VaporCombo>  (p.apvts, "delay_div",  "Div", syncDivNames()); addAndMakeVisible (*dlDiv);

    rvMix  = std::make_unique<VaporKnob> (p.apvts, "reverb",      "Mix");  addAndMakeVisible (*rvMix);
    rvSize = std::make_unique<VaporKnob> (p.apvts, "reverb_size", "Size"); addAndMakeVisible (*rvSize);
    rvDamp = std::make_unique<VaporKnob> (p.apvts, "reverb_damp", "Damp"); addAndMakeVisible (*rvDamp);

    compOn     = std::make_unique<VaporToggle>(p.apvts, "comp_on",     "COMP");   addAndMakeVisible (*compOn);
    compThr    = std::make_unique<VaporKnob>  (p.apvts, "comp_thr",    "Thr");    addAndMakeVisible (*compThr);
    compRatio  = std::make_unique<VaporKnob>  (p.apvts, "comp_ratio",  "Ratio");  addAndMakeVisible (*compRatio);
    compAtk    = std::make_unique<VaporKnob>  (p.apvts, "comp_atk",    "Atk");    addAndMakeVisible (*compAtk);
    compRel    = std::make_unique<VaporKnob>  (p.apvts, "comp_rel",    "Rel");    addAndMakeVisible (*compRel);
    compMakeup = std::make_unique<VaporKnob>  (p.apvts, "comp_makeup", "Makeup"); addAndMakeVisible (*compMakeup);
}

void FxPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (10);
    const int rowH = (r.getHeight() - 20) / 3;
    auto row1 = r.removeFromTop (rowH); r.removeFromTop (10);
    auto row2 = r.removeFromTop (rowH); r.removeFromTop (10);
    auto row3 = r;

    const int w1 = row1.getWidth();
    juce::Rectangle<int> sDist  (row1.getX(),                    row1.getY(), w1 / 3 - 6, row1.getHeight());
    juce::Rectangle<int> sChor  (row1.getX() + w1 / 3 + 3,       row1.getY(), w1 / 3 - 6, row1.getHeight());
    juce::Rectangle<int> sPhas  (row1.getX() + 2 * w1 / 3 + 6,   row1.getY(), w1 / 3 - 6, row1.getHeight());
    drawSectionBg (g, sDist, Colors::neonAmber,  "DISTORTION");
    drawSectionBg (g, sChor, Colors::neonPink,   "CHORUS");
    drawSectionBg (g, sPhas, Colors::neonPurple, "PHASER");

    const int w2 = row2.getWidth();
    juce::Rectangle<int> sEq    (row2.getX(),                    row2.getY(), w2 / 2 - 5, row2.getHeight());
    juce::Rectangle<int> sDly   (row2.getX() + w2 / 2 + 5,       row2.getY(), w2 / 2 - 5, row2.getHeight());
    drawSectionBg (g, sEq,  Colors::neonCyan, "EQ");
    drawSectionBg (g, sDly, Colors::neonPink, "DELAY");

    const int w3 = row3.getWidth();
    juce::Rectangle<int> sRv    (row3.getX(),                    row3.getY(), w3 * 2 / 5 - 5, row3.getHeight());
    juce::Rectangle<int> sCmp   (row3.getX() + w3 * 2 / 5 + 5,   row3.getY(), w3 * 3 / 5 - 5, row3.getHeight());
    drawSectionBg (g, sRv,  Colors::neonAmber, "REVERB");
    drawSectionBg (g, sCmp, Colors::neonCyan,  "COMPRESSOR");
}

void FxPage::resized()
{
    auto r = getLocalBounds().reduced (10);
    const int rowH = (r.getHeight() - 20) / 3;
    auto row1 = r.removeFromTop (rowH); r.removeFromTop (10);
    auto row2 = r.removeFromTop (rowH); r.removeFromTop (10);
    auto row3 = r;

    const int w1 = row1.getWidth();
    juce::Rectangle<int> sDist  (row1.getX(),                    row1.getY(), w1 / 3 - 6, row1.getHeight());
    juce::Rectangle<int> sChor  (row1.getX() + w1 / 3 + 3,       row1.getY(), w1 / 3 - 6, row1.getHeight());
    juce::Rectangle<int> sPhas  (row1.getX() + 2 * w1 / 3 + 6,   row1.getY(), w1 / 3 - 6, row1.getHeight());

    {
        auto a = sDist; a.removeFromTop (kSectionTitleH); a.reduce (10, 8);
        distType->setBounds (a.removeFromTop (40).reduced (2, 0));
        layoutKnobRow (a, { distDrive.get(), distMix.get() }, 0);
    }
    layoutKnobRow (sChor, { chMix.get(), chRate.get(), chDepth.get() });
    layoutKnobRow (sPhas, { phMix.get(), phRate.get(), phDepth.get(), phFb.get() });

    const int w2 = row2.getWidth();
    juce::Rectangle<int> sEq    (row2.getX(),                    row2.getY(), w2 / 2 - 5, row2.getHeight());
    juce::Rectangle<int> sDly   (row2.getX() + w2 / 2 + 5,       row2.getY(), w2 / 2 - 5, row2.getHeight());
    {
        auto a = sEq; a.removeFromTop (kSectionTitleH); a.reduce (10, 8);
        eqCurve->setBounds (a);
    }
    {
        auto a = sDly; a.removeFromTop (kSectionTitleH); a.reduce (10, 8);
        auto top1 = a.removeFromTop (40);
        dlSync->setBounds (top1.removeFromLeft (70).reduced (4, 4));
        dlDiv->setBounds  (top1.removeFromLeft (160));
        layoutKnobRow (a, { dlMix.get(), dlTime.get(), dlFb.get() }, 0);
    }

    const int w3 = row3.getWidth();
    juce::Rectangle<int> sRv    (row3.getX(),                    row3.getY(), w3 * 2 / 5 - 5, row3.getHeight());
    juce::Rectangle<int> sCmp   (row3.getX() + w3 * 2 / 5 + 5,   row3.getY(), w3 * 3 / 5 - 5, row3.getHeight());
    layoutKnobRow (sRv,  { rvMix.get(), rvSize.get(), rvDamp.get() });
    {
        auto a = sCmp; a.removeFromTop (kSectionTitleH); a.reduce (10, 8);
        compOn->setBounds (a.removeFromTop (40).removeFromLeft (96).reduced (2, 4));
        layoutKnobRow (a, { compThr.get(), compRatio.get(), compAtk.get(), compRel.get(), compMakeup.get() }, 0);
    }
}
