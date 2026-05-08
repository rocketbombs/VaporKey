#include "FilterEnvPage.h"
#include "EditorLayout.h"
#include "../PluginProcessor.h"
#include "../LookAndFeel.h"

using namespace VK;
using namespace VKEditorLayout;

FilterEnvPage::FilterEnvPage (VaporKeyAudioProcessor& p)
{
    cut    = std::make_unique<VaporKnob>  (p.apvts, "f_cut",   "Cutoff");  addAndMakeVisible (*cut);
    res    = std::make_unique<VaporKnob>  (p.apvts, "f_res",   "Reso");    addAndMakeVisible (*res);
    env    = std::make_unique<VaporKnob>  (p.apvts, "f_env",   "Env");     addAndMakeVisible (*env);
    drive  = std::make_unique<VaporKnob>  (p.apvts, "f_drive", "Drive");   addAndMakeVisible (*drive);
    key    = std::make_unique<VaporKnob>  (p.apvts, "f_key",   "Key");     addAndMakeVisible (*key);
    type   = std::make_unique<VaporCombo> (p.apvts, "f_type",  "Type", juce::StringArray { "LP", "BP", "HP" });
    addAndMakeVisible (*type);

    aA = std::make_unique<VaporKnob> (p.apvts, "a_a",   "A");   addAndMakeVisible (*aA);
    aD = std::make_unique<VaporKnob> (p.apvts, "a_d",   "D");   addAndMakeVisible (*aD);
    aS = std::make_unique<VaporKnob> (p.apvts, "a_s",   "S");   addAndMakeVisible (*aS);
    aR = std::make_unique<VaporKnob> (p.apvts, "a_r",   "R");   addAndMakeVisible (*aR);
    aVel = std::make_unique<VaporKnob> (p.apvts, "a_vel", "Vel"); addAndMakeVisible (*aVel);

    mA = std::make_unique<VaporKnob> (p.apvts, "m_a",   "A");   addAndMakeVisible (*mA);
    mD = std::make_unique<VaporKnob> (p.apvts, "m_d",   "D");   addAndMakeVisible (*mD);
    mS = std::make_unique<VaporKnob> (p.apvts, "m_s",   "S");   addAndMakeVisible (*mS);
    mR = std::make_unique<VaporKnob> (p.apvts, "m_r",   "R");   addAndMakeVisible (*mR);
    fVel = std::make_unique<VaporKnob> (p.apvts, "f_vel", "FVel"); addAndMakeVisible (*fVel);

    pAmt   = std::make_unique<VaporKnob> (p.apvts, "p_env_amt",   "Amt");   addAndMakeVisible (*pAmt);
    pDecay = std::make_unique<VaporKnob> (p.apvts, "p_env_decay", "Decay"); addAndMakeVisible (*pDecay);

    grit  = std::make_unique<VaporKnob> (p.apvts, "grit",  "Grit");  addAndMakeVisible (*grit);
    vibe  = std::make_unique<VaporKnob> (p.apvts, "vibe",  "Vibe");  addAndMakeVisible (*vibe);
    drift = std::make_unique<VaporKnob> (p.apvts, "drift", "Drift"); addAndMakeVisible (*drift);
    sat   = std::make_unique<VaporKnob> (p.apvts, "sat",   "Sat");   addAndMakeVisible (*sat);
}

void FilterEnvPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (r.getHeight() / 2 - 5);
    r.removeFromTop (10);
    auto bot = r;

    const int tW = top.getWidth();
    juce::Rectangle<int> sFilter (top.getX(),                top.getY(), tW * 6 / 10 - 5, top.getHeight());
    juce::Rectangle<int> sPitch  (top.getX() + tW * 6 / 10 + 5, top.getY(), tW * 4 / 10 - 5, top.getHeight());

    const int bW = bot.getWidth();
    juce::Rectangle<int> sAmp   (bot.getX(),                  bot.getY(), bW / 3 - 6, bot.getHeight());
    juce::Rectangle<int> sMod   (bot.getX() + bW / 3 + 3,     bot.getY(), bW / 3 - 6, bot.getHeight());
    juce::Rectangle<int> sWarm  (bot.getX() + 2 * bW / 3 + 6, bot.getY(), bW / 3 - 6, bot.getHeight());

    drawSectionBg (g, sFilter, Colors::neonCyan,   "FILTER");
    drawSectionBg (g, sPitch,  Colors::neonAmber,  "PITCH ENV");
    drawSectionBg (g, sAmp,    Colors::neonPink,   "AMP ENVELOPE");
    drawSectionBg (g, sMod,    Colors::neonPurple, "MOD ENVELOPE");
    drawSectionBg (g, sWarm,   Colors::neonAmber,  "WARMTH");
}

void FilterEnvPage::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (r.getHeight() / 2 - 5);
    r.removeFromTop (10);
    auto bot = r;

    const int tW = top.getWidth();
    juce::Rectangle<int> sFilter (top.getX(),                top.getY(), tW * 6 / 10 - 5, top.getHeight());
    juce::Rectangle<int> sPitch  (top.getX() + tW * 6 / 10 + 5, top.getY(), tW * 4 / 10 - 5, top.getHeight());

    {
        auto a = sFilter; a.removeFromTop (kSectionTitleH); a.reduce (10, 8);
        auto topRow = a.removeFromTop (40);
        type->setBounds (topRow.removeFromLeft (160).reduced (2, 0));
        layoutKnobRow (a, { cut.get(), res.get(), env.get(), drive.get(), key.get() }, 0);
    }
    {
        auto a = sPitch; a.removeFromTop (kSectionTitleH); a.reduce (10, 8);
        layoutKnobRow (a, { pAmt.get(), pDecay.get() }, 0);
    }

    const int bW = bot.getWidth();
    juce::Rectangle<int> sAmp   (bot.getX(),                  bot.getY(), bW / 3 - 6, bot.getHeight());
    juce::Rectangle<int> sMod   (bot.getX() + bW / 3 + 3,     bot.getY(), bW / 3 - 6, bot.getHeight());
    juce::Rectangle<int> sWarm  (bot.getX() + 2 * bW / 3 + 6, bot.getY(), bW / 3 - 6, bot.getHeight());

    layoutKnobRow (sAmp,  { aA.get(), aD.get(), aS.get(), aR.get(), aVel.get() });
    layoutKnobRow (sMod,  { mA.get(), mD.get(), mS.get(), mR.get(), fVel.get() });
    layoutKnobGrid (sWarm,{ grit.get(), vibe.get(), drift.get(), sat.get() }, 2);
}
