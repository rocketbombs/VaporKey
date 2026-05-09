#include "OscPage.h"
#include "EditorLayout.h"
#include "../PluginProcessor.h"
#include "../LookAndFeel.h"

using namespace VK;
using namespace VKEditorLayout;

OscPage::OscPage (VaporKeyAudioProcessor& p) : proc (p)
{
    for (int i = 0; i < 3; ++i)
    {
        auto& u = oscUI[i];
        u.title.setText ("OSC " + juce::String (i + 1), juce::dontSendNotification);
        u.title.setJustificationType (juce::Justification::centredLeft);
        u.title.setColour (juce::Label::textColourId, Colors::neonPink);
        u.title.setFont (Fonts::subheader());
        addAndMakeVisible (u.title);

        const juce::String pf = "osc" + juce::String (i + 1) + "_";
        u.on    = std::make_unique<VaporToggle> (proc.apvts, pf + "on", "ON");      addAndMakeVisible (*u.on);
        u.shape = std::make_unique<WavetableShapePicker> (proc.apvts, pf + "shape"); addAndMakeVisible (*u.shape);
        u.display = std::make_unique<WavetableDisplay> (proc, i); addAndMakeVisible (*u.display);
        u.position = std::make_unique<VaporKnob> (proc.apvts, pf + "pos",    "Pos");  addAndMakeVisible (*u.position);
        u.level    = std::make_unique<VaporKnob> (proc.apvts, pf + "level",  "Lvl");  addAndMakeVisible (*u.level);
        u.pan      = std::make_unique<VaporKnob> (proc.apvts, pf + "pan",    "Pan");  addAndMakeVisible (*u.pan);
        u.coarse   = std::make_unique<VaporKnob> (proc.apvts, pf + "coarse", "Semi"); addAndMakeVisible (*u.coarse);
        u.fine     = std::make_unique<VaporKnob> (proc.apvts, pf + "fine",   "Fine"); addAndMakeVisible (*u.fine);
        u.unison   = std::make_unique<VaporKnob> (proc.apvts, pf + "unison", "Uni");  addAndMakeVisible (*u.unison);
        u.detune   = std::make_unique<VaporKnob> (proc.apvts, pf + "detune", "Det");  addAndMakeVisible (*u.detune);
        u.phase    = std::make_unique<VaporKnob> (proc.apvts, pf + "phase",  "Phase"); addAndMakeVisible (*u.phase);
    }

    subOn      = std::make_unique<VaporToggle>(proc.apvts, "sub_on", "SUB ON");                         addAndMakeVisible (*subOn);
    subShape   = std::make_unique<VaporCombo> (proc.apvts, "sub_shape", "Shape", SubShape::names());    addAndMakeVisible (*subShape);
    subOct     = std::make_unique<VaporKnob>  (proc.apvts, "sub_oct", "Oct");                           addAndMakeVisible (*subOct);
    subLevel   = std::make_unique<VaporKnob>  (proc.apvts, "sub_level", "Lvl");                         addAndMakeVisible (*subLevel);

    noiseOn    = std::make_unique<VaporToggle>(proc.apvts, "noise_on", "NOISE ON");                     addAndMakeVisible (*noiseOn);
    noiseColor = std::make_unique<VaporCombo> (proc.apvts, "noise_color", "Color", NoiseColor::names());addAndMakeVisible (*noiseColor);
    noiseLevel = std::make_unique<VaporKnob>  (proc.apvts, "noise_level", "Lvl");                       addAndMakeVisible (*noiseLevel);

    glide      = std::make_unique<VaporKnob>  (proc.apvts, "glide", "Glide");                           addAndMakeVisible (*glide);
    mono       = std::make_unique<VaporToggle>(proc.apvts, "mono", "MONO");                             addAndMakeVisible (*mono);
    legato     = std::make_unique<VaporToggle>(proc.apvts, "legato", "LEGATO");                         addAndMakeVisible (*legato);
    bendRange  = std::make_unique<VaporKnob>  (proc.apvts, "bend_range", "Bend");                       addAndMakeVisible (*bendRange);

    xmod = std::make_unique<OscModMatrix> (proc.apvts);
    addAndMakeVisible (*xmod);
}

// Pixel-band heights for the three horizontal regions. X-MOD takes a fixed-ish
// slice in the middle so the diagram never collapses to the point where it
// reads as visual noise.
namespace { constexpr int kXmodBandH = 140; constexpr int kBandPad = 8; }

void OscPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (10);
    const int total = r.getHeight();
    // Bottom (sub/noise/voicing) takes ~32% of the page; X-MOD a fixed band;
    // OSC panels use whatever remains.
    const int bottomH = juce::jmax (110, (int) (total * 0.32f));
    const int topH    = juce::jmax (180, total - bottomH - kXmodBandH - kBandPad * 2);

    auto top      = r.removeFromTop (topH);
    r.removeFromTop (kBandPad);
    auto xmodR    = r.removeFromTop (kXmodBandH);
    r.removeFromTop (kBandPad);
    auto bottom   = r;

    const int oscW = (top.getWidth() - 20) / 3;
    for (int i = 0; i < 3; ++i)
    {
        juce::Rectangle<int> panel (top.getX() + i * (oscW + 10), top.getY(), oscW, top.getHeight());
        drawSectionBg (g, panel, Colors::neonPink, "");
    }

    drawSectionBg (g, xmodR, Colors::neonPurple, "X-MOD");

    const int bw = bottom.getWidth();
    juce::Rectangle<int> sub  (bottom.getX(),                  bottom.getY(), bw / 3 - 6, bottom.getHeight());
    juce::Rectangle<int> noi  (bottom.getX() + bw / 3 + 3,     bottom.getY(), bw / 3 - 6, bottom.getHeight());
    juce::Rectangle<int> voc  (bottom.getX() + 2 * bw / 3 + 6, bottom.getY(), bw / 3 - 6, bottom.getHeight());
    drawSectionBg (g, sub, Colors::neonAmber, "SUB OSC");
    drawSectionBg (g, noi, Colors::neonAmber, "NOISE");
    drawSectionBg (g, voc, Colors::neonCyan,  "VOICING");
}

void OscPage::resized()
{
    auto r = getLocalBounds().reduced (10);
    const int total = r.getHeight();
    const int bottomH = juce::jmax (110, (int) (total * 0.32f));
    const int topH    = juce::jmax (180, total - bottomH - kXmodBandH - kBandPad * 2);

    auto top    = r.removeFromTop (topH);
    r.removeFromTop (kBandPad);
    auto xmodR  = r.removeFromTop (kXmodBandH);
    r.removeFromTop (kBandPad);
    auto bottom = r;

    if (xmod)
    {
        // Slot the matrix below the X-MOD section title strip drawn by
        // drawSectionBg, with the same inset as the bottom panels.
        auto xmodInner = xmodR;
        xmodInner.removeFromTop (kSectionTitleH);
        xmodInner.reduce (10, 4);
        xmod->setBounds (xmodInner);
    }

    const int oscW = (top.getWidth() - 20) / 3;
    for (int i = 0; i < 3; ++i)
    {
        auto osc = juce::Rectangle<int> (top.getX() + i * (oscW + 10), top.getY(),
                                         oscW, top.getHeight()).reduced (12, 12);

        auto headerR = osc.removeFromTop (32);
        oscUI[i].title.setBounds (headerR.removeFromLeft (70));
        oscUI[i].on   ->setBounds (headerR.removeFromLeft (60).reduced (2, 4));
        headerR.removeFromLeft (6);
        oscUI[i].shape->setBounds (headerR.reduced (2, 0));

        auto disp = osc.removeFromTop ((int) (osc.getHeight() * 0.34));
        oscUI[i].display->setBounds (disp.reduced (4));

        osc.reduce (4, 6);
        const int kW = osc.getWidth() / 4;
        const int kH = osc.getHeight() / 2;

        auto row1 = osc.removeFromTop (kH);
        oscUI[i].position->setBounds (row1.removeFromLeft (kW));
        oscUI[i].level   ->setBounds (row1.removeFromLeft (kW));
        oscUI[i].pan     ->setBounds (row1.removeFromLeft (kW));
        oscUI[i].unison  ->setBounds (row1.removeFromLeft (kW));

        auto row2 = osc;
        oscUI[i].coarse->setBounds (row2.removeFromLeft (kW));
        oscUI[i].fine  ->setBounds (row2.removeFromLeft (kW));
        oscUI[i].detune->setBounds (row2.removeFromLeft (kW));
        oscUI[i].phase ->setBounds (row2.removeFromLeft (kW));
    }

    const int bw = bottom.getWidth();
    juce::Rectangle<int> sub  (bottom.getX(),                  bottom.getY(), bw / 3 - 6, bottom.getHeight());
    juce::Rectangle<int> noi  (bottom.getX() + bw / 3 + 3,     bottom.getY(), bw / 3 - 6, bottom.getHeight());
    juce::Rectangle<int> voc  (bottom.getX() + 2 * bw / 3 + 6, bottom.getY(), bw / 3 - 6, bottom.getHeight());

    auto layoutSubLikeBox = [] (juce::Rectangle<int> a, VaporToggle* tog, VaporCombo* combo,
                                VaporKnob* k1, VaporKnob* k2)
    {
        a.removeFromTop (kSectionTitleH);
        a.reduce (10, 6);
        auto top1 = a.removeFromTop (34);
        tog->setBounds   (top1.removeFromLeft (a.getWidth() / 2).reduced (2, 2));
        combo->setBounds (top1.reduced (2, 0));
        const int kw = a.getWidth() / 2;
        k1->setBounds (a.removeFromLeft (kw));
        k2->setBounds (a);
    };
    layoutSubLikeBox (sub, subOn.get(), subShape.get(), subOct.get(), subLevel.get());

    {
        auto a = noi; a.removeFromTop (kSectionTitleH); a.reduce (10, 6);
        auto top1 = a.removeFromTop (34);
        noiseOn->setBounds    (top1.removeFromLeft (a.getWidth() / 2).reduced (2, 2));
        noiseColor->setBounds (top1.reduced (2, 0));
        noiseLevel->setBounds (a);
    }
    {
        auto a = voc; a.removeFromTop (kSectionTitleH); a.reduce (10, 6);
        auto top1 = a.removeFromTop (34);
        mono->setBounds   (top1.removeFromLeft (a.getWidth() / 2).reduced (2, 2));
        legato->setBounds (top1.reduced (2, 2));
        const int kw = a.getWidth() / 2;
        glide->setBounds (a.removeFromLeft (kw));
        bendRange->setBounds (a);
    }
}
