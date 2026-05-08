#include "ModPage.h"
#include "EditorLayout.h"
#include "../PluginProcessor.h"
#include "../LookAndFeel.h"

using namespace VK;
using namespace VKEditorLayout;

ModPage::ModPage (VaporKeyAudioProcessor& p)
{
    auto destNames = ModDest::names();

    l1Shape = std::make_unique<VaporCombo>  (p.apvts, "lfo1_shape", "Shape", LfoShape::names()); addAndMakeVisible (*l1Shape);
    l1Rate  = std::make_unique<VaporKnob>   (p.apvts, "lfo1_rate", "Rate");  addAndMakeVisible (*l1Rate);
    l1Amt   = std::make_unique<VaporKnob>   (p.apvts, "lfo1_amt",  "Amt");   addAndMakeVisible (*l1Amt);
    l1Sync  = std::make_unique<VaporToggle> (p.apvts, "lfo1_sync", "SYNC");  addAndMakeVisible (*l1Sync);
    l1Div   = std::make_unique<VaporCombo>  (p.apvts, "lfo1_div",  "Div", syncDivNames()); addAndMakeVisible (*l1Div);

    l2Shape = std::make_unique<VaporCombo>  (p.apvts, "lfo2_shape", "Shape", LfoShape::names()); addAndMakeVisible (*l2Shape);
    l2Rate  = std::make_unique<VaporKnob>   (p.apvts, "lfo2_rate", "Rate");  addAndMakeVisible (*l2Rate);
    l2Amt   = std::make_unique<VaporKnob>   (p.apvts, "lfo2_amt",  "Amt");   addAndMakeVisible (*l2Amt);
    l2Sync  = std::make_unique<VaporToggle> (p.apvts, "lfo2_sync", "SYNC");  addAndMakeVisible (*l2Sync);
    l2Div   = std::make_unique<VaporCombo>  (p.apvts, "lfo2_div",  "Div", syncDivNames()); addAndMakeVisible (*l2Div);

    for (int m = 0; m < SynthParams::kNumMacros; ++m)
    {
        const juce::String pf = "macro" + juce::String (m + 1) + "_";
        macros[m].val  = std::make_unique<VaporKnob>  (p.apvts, pf + "val",  "Macro " + juce::String (m + 1));
        macros[m].dest = std::make_unique<VaporCombo> (p.apvts, pf + "dest", "Dest", destNames);
        macros[m].amt  = std::make_unique<VaporKnob>  (p.apvts, pf + "amt",  "Amt");
        addAndMakeVisible (*macros[m].val);
        addAndMakeVisible (*macros[m].dest);
        addAndMakeVisible (*macros[m].amt);
    }

    mwUI.dest = std::make_unique<VaporCombo> (p.apvts, "mw_dest", "Dest", destNames);
    mwUI.amt  = std::make_unique<VaporKnob>  (p.apvts, "mw_amt",  "Amt");
    addAndMakeVisible (*mwUI.dest);
    addAndMakeVisible (*mwUI.amt);

    atUI.dest = std::make_unique<VaporCombo> (p.apvts, "at_dest", "Dest", destNames);
    atUI.amt  = std::make_unique<VaporKnob>  (p.apvts, "at_amt",  "Amt");
    addAndMakeVisible (*atUI.dest);
    addAndMakeVisible (*atUI.amt);
}

namespace { constexpr int kNumModSlots = SynthParams::kNumMacros + 2; }

void ModPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (r.getHeight() / 2 - 5);
    r.removeFromTop (10);
    auto bot = r;

    const int tw = top.getWidth();
    juce::Rectangle<int> s1 (top.getX(),              top.getY(), tw / 2 - 5, top.getHeight());
    juce::Rectangle<int> s2 (top.getX() + tw / 2 + 5, top.getY(), tw / 2 - 5, top.getHeight());
    drawSectionBg (g, s1, Colors::neonCyan, "LFO 1   >  CUTOFF");
    drawSectionBg (g, s2, Colors::neonPink, "LFO 2   >  WT POSITION");

    const int mw = bot.getWidth() / kNumModSlots;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        juce::Rectangle<int> mr (bot.getX() + i * mw + (i > 0 ? 5 : 0), bot.getY(), mw - 5, bot.getHeight());
        juce::String title;
        juce::Colour col;
        if (i < SynthParams::kNumMacros) { title = "MACRO " + juce::String (i + 1); col = Colors::neonAmber; }
        else if (i == SynthParams::kNumMacros) { title = "MOD WHEEL"; col = Colors::neonGreen; }
        else                                   { title = "AFTERTOUCH"; col = Colors::neonGreen; }
        drawSectionBg (g, mr, col, title);
    }
}

void ModPage::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (r.getHeight() / 2 - 5);
    r.removeFromTop (10);
    auto bot = r;

    const int tw = top.getWidth();
    juce::Rectangle<int> s1 (top.getX(),              top.getY(), tw / 2 - 5, top.getHeight());
    juce::Rectangle<int> s2 (top.getX() + tw / 2 + 5, top.getY(), tw / 2 - 5, top.getHeight());

    auto layoutLfo = [] (juce::Rectangle<int> a, VaporCombo* shape, VaporKnob* rate, VaporKnob* amt,
                         VaporToggle* sync, VaporCombo* div)
    {
        a.removeFromTop (kSectionTitleH); a.reduce (10, 8);
        auto top1 = a.removeFromTop (40);
        shape->setBounds (top1.removeFromLeft (a.getWidth() / 3));
        sync->setBounds  (top1.removeFromLeft (70).reduced (4, 4));
        div->setBounds   (top1);
        layoutKnobRow (a, { rate, amt }, 0);
    };
    layoutLfo (s1, l1Shape.get(), l1Rate.get(), l1Amt.get(), l1Sync.get(), l1Div.get());
    layoutLfo (s2, l2Shape.get(), l2Rate.get(), l2Amt.get(), l2Sync.get(), l2Div.get());

    const int mw = bot.getWidth() / kNumModSlots;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        juce::Rectangle<int> mr (bot.getX() + i * mw + (i > 0 ? 5 : 0), bot.getY(), mw - 5, bot.getHeight());
        mr.removeFromTop (kSectionTitleH); mr.reduce (10, 8);
        if (i < SynthParams::kNumMacros)
        {
            macros[i].dest->setBounds (mr.removeFromTop (50));
            layoutKnobRow (mr, { macros[i].val.get(), macros[i].amt.get() }, 0);
        }
        else
        {
            auto& ui = (i == SynthParams::kNumMacros) ? mwUI : atUI;
            ui.dest->setBounds (mr.removeFromTop (50));
            layoutKnobRow (mr, { ui.amt.get() }, 0);
        }
    }
}
