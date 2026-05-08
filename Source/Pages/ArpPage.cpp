#include "ArpPage.h"
#include "EditorLayout.h"
#include "../PluginProcessor.h"
#include "../LookAndFeel.h"

using namespace VK;
using namespace VKEditorLayout;

ArpPage::ArpPage (VaporKeyAudioProcessor& p)
{
    on    = std::make_unique<VaporToggle> (p.apvts, "arp_on",    "ARP ON");      addAndMakeVisible (*on);
    latch = std::make_unique<VaporToggle> (p.apvts, "arp_latch", "LATCH");       addAndMakeVisible (*latch);
    mode  = std::make_unique<VaporCombo>  (p.apvts, "arp_mode",  "Mode", ArpMode::names());     addAndMakeVisible (*mode);
    div   = std::make_unique<VaporCombo>  (p.apvts, "arp_div",   "Rate", syncDivNames());       addAndMakeVisible (*div);
    octaves = std::make_unique<VaporKnob> (p.apvts, "arp_octaves", "Octaves");   addAndMakeVisible (*octaves);
    gate    = std::make_unique<VaporKnob> (p.apvts, "arp_gate",    "Gate");      addAndMakeVisible (*gate);
    swing   = std::make_unique<VaporKnob> (p.apvts, "arp_swing",   "Swing");     addAndMakeVisible (*swing);

    blurb.setText ("Hold a chord to step through it.  Sync follows host tempo.\n"
                   "MODE picks pattern.  OCTAVES extends range upward.\n"
                   "GATE sets note length (0.05 - 1.0 of a step).\n"
                   "SWING delays even-numbered steps for shuffled rhythms.\n"
                   "LATCH keeps the chord sounding after you release the keys.",
                   juce::dontSendNotification);
    blurb.setFont (Fonts::value());
    blurb.setColour (juce::Label::textColourId, Colors::textDim);
    blurb.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (blurb);
}

void ArpPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop ((int) (r.getHeight() * 0.62));
    r.removeFromTop (10);
    auto bot = r;
    drawSectionBg (g, top, Colors::neonPink, "ARPEGGIATOR");
    drawSectionBg (g, bot, Colors::neonCyan, "HOW IT WORKS");
}

void ArpPage::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop ((int) (r.getHeight() * 0.62));
    r.removeFromTop (10);
    auto bot = r;

    {
        auto a = top; a.removeFromTop (kSectionTitleH); a.reduce (16, 12);

        auto headerRow = a.removeFromTop (44);
        on   ->setBounds (headerRow.removeFromLeft (110).reduced (2, 4));
        headerRow.removeFromLeft (10);
        latch->setBounds (headerRow.removeFromLeft (110).reduced (2, 4));
        headerRow.removeFromLeft (16);
        mode ->setBounds (headerRow.removeFromLeft (juce::jmin (240, headerRow.getWidth() / 2)).reduced (2, 0));
        headerRow.removeFromLeft (10);
        div  ->setBounds (headerRow.reduced (2, 0));
        a.removeFromTop (10);

        layoutKnobRow (a, { octaves.get(), gate.get(), swing.get() }, 0);
    }

    {
        auto a = bot; a.removeFromTop (kSectionTitleH); a.reduce (16, 12);
        blurb.setBounds (a);
    }
}
