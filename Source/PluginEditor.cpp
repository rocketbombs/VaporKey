#include "PluginEditor.h"

using namespace VK;

static void styleSection (juce::Label& l, const juce::String& text, juce::Colour c = Colors::neonPink)
{
    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centredLeft);
    l.setColour (juce::Label::textColourId, c);
    l.setFont (juce::Font (juce::FontOptions (13.0f).withStyle ("Bold")));
}

VaporKeyAudioProcessorEditor::VaporKeyAudioProcessorEditor (VaporKeyAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    for (int i = 0; i < 3; ++i)
        buildOscPanel (i);

    auto mk = [this] (const juce::String& id, const juce::String& nm)
    {
        return std::make_unique<VaporKnob> (proc.apvts, id, nm);
    };

    kCut = mk ("f_cut", "Cutoff");        addAndMakeVisible (*kCut);
    kRes = mk ("f_res", "Reso");          addAndMakeVisible (*kRes);
    kEnv = mk ("f_env", "Env");           addAndMakeVisible (*kEnv);

    fTypeBox.addItem ("LP", 1); fTypeBox.addItem ("BP", 2); fTypeBox.addItem ("HP", 3);
    addAndMakeVisible (fTypeBox);
    fTypeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "f_type", fTypeBox);

    kAmpA = mk ("a_a", "A"); kAmpD = mk ("a_d", "D"); kAmpS = mk ("a_s", "S"); kAmpR = mk ("a_r", "R");
    addAndMakeVisible (*kAmpA); addAndMakeVisible (*kAmpD); addAndMakeVisible (*kAmpS); addAndMakeVisible (*kAmpR);

    kModA = mk ("m_a", "A"); kModD = mk ("m_d", "D"); kModS = mk ("m_s", "S"); kModR = mk ("m_r", "R");
    addAndMakeVisible (*kModA); addAndMakeVisible (*kModD); addAndMakeVisible (*kModS); addAndMakeVisible (*kModR);

    kLfo1Rate = mk ("lfo1_rate", "L1 Rate"); kLfo1Amt = mk ("lfo1_amt", "L1>Cut");
    kLfo2Rate = mk ("lfo2_rate", "L2 Rate"); kLfo2Amt = mk ("lfo2_amt", "L2>Pos");
    addAndMakeVisible (*kLfo1Rate); addAndMakeVisible (*kLfo1Amt);
    addAndMakeVisible (*kLfo2Rate); addAndMakeVisible (*kLfo2Amt);

    kGrit  = mk ("grit",  "Grit");
    kVibe  = mk ("vibe",  "Vibe");
    kDrift = mk ("drift", "Drift");
    kSat   = mk ("sat",   "Sat");
    addAndMakeVisible (*kGrit); addAndMakeVisible (*kVibe);
    addAndMakeVisible (*kDrift); addAndMakeVisible (*kSat);

    kChorus    = mk ("chorus",     "Chorus");
    kDelay     = mk ("delay",      "Delay");
    kDelayTime = mk ("delay_time", "Time");
    kDelayFb   = mk ("delay_fb",   "FB");
    kReverb    = mk ("reverb",     "Reverb");
    addAndMakeVisible (*kChorus); addAndMakeVisible (*kDelay);
    addAndMakeVisible (*kDelayTime); addAndMakeVisible (*kDelayFb); addAndMakeVisible (*kReverb);

    kGain = mk ("gain", "Master");
    addAndMakeVisible (*kGain);

    setResizable (true, true);
    setResizeLimits (920, 560, 1600, 1000);
    setSize (1080, 640);
}

VaporKeyAudioProcessorEditor::~VaporKeyAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void VaporKeyAudioProcessorEditor::buildOscPanel (int i)
{
    auto& u = oscUI[i];
    u.title.setText ("OSC " + juce::String (i + 1), juce::dontSendNotification);
    u.title.setJustificationType (juce::Justification::centredLeft);
    u.title.setColour (juce::Label::textColourId, Colors::neonPink);
    u.title.setFont (juce::Font (juce::FontOptions (14.0f).withStyle ("Bold")));
    addAndMakeVisible (u.title);

    u.onBtn.setButtonText ("ON");
    addAndMakeVisible (u.onBtn);
    u.onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, "osc" + juce::String (i + 1) + "_on", u.onBtn);

    for (int s = 0; s < WavetableLibrary::NumShapes; ++s)
        u.shapeBox.addItem (WavetableLibrary::shapeName (s), s + 1);
    addAndMakeVisible (u.shapeBox);
    u.shapeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "osc" + juce::String (i + 1) + "_shape", u.shapeBox);

    u.display = std::make_unique<WavetableDisplay> (proc.apvts, i);
    addAndMakeVisible (*u.display);

    auto suffix = "osc" + juce::String (i + 1) + "_";
    u.position = std::make_unique<VaporKnob> (proc.apvts, suffix + "pos",    "Pos");
    u.level    = std::make_unique<VaporKnob> (proc.apvts, suffix + "level",  "Lvl");
    u.pan      = std::make_unique<VaporKnob> (proc.apvts, suffix + "pan",    "Pan");
    u.coarse   = std::make_unique<VaporKnob> (proc.apvts, suffix + "coarse", "Semi");
    u.fine     = std::make_unique<VaporKnob> (proc.apvts, suffix + "fine",   "Fine");
    u.unison   = std::make_unique<VaporKnob> (proc.apvts, suffix + "unison", "Uni");
    u.detune   = std::make_unique<VaporKnob> (proc.apvts, suffix + "detune", "Det");
    addAndMakeVisible (*u.position); addAndMakeVisible (*u.level); addAndMakeVisible (*u.pan);
    addAndMakeVisible (*u.coarse);   addAndMakeVisible (*u.fine);
    addAndMakeVisible (*u.unison);   addAndMakeVisible (*u.detune);
}

void VaporKeyAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // Vaporwave gradient sky
    juce::ColourGradient sky (juce::Colour (0xff1a0640), 0.0f, 0.0f,
                              juce::Colour (0xff05010f), 0.0f, r.getHeight(), false);
    sky.addColour (0.45, juce::Colour (0xffff2ec4).withAlpha (0.18f));
    sky.addColour (0.55, juce::Colour (0xff29f5ff).withAlpha (0.10f));
    g.setGradientFill (sky);
    g.fillRect (r);

    // Sun
    {
        const float cx = r.getCentreX();
        const float cy = r.getY() + 90.0f;
        const float rad = 80.0f;
        juce::ColourGradient sun (juce::Colour (0xffffd166), cx, cy - rad,
                                  juce::Colour (0xffff2ec4), cx, cy + rad, false);
        g.setGradientFill (sun);
        g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);

        // Cut horizontal scan-lines through the sun
        g.setColour (juce::Colour (0xff05010f));
        for (int i = 0; i < 6; ++i)
        {
            const float y = cy + (float) i * 6.0f - 6.0f;
            g.fillRect (cx - rad, y, rad * 2.0f, 2.5f);
        }
    }

    // Horizon perspective grid
    {
        const float horizon = r.getHeight() * 0.42f;
        g.setColour (Colors::neonPink.withAlpha (0.5f));
        g.drawHorizontalLine ((int) horizon, r.getX(), r.getRight());

        const float cx = r.getCentreX();
        for (int i = -10; i <= 10; ++i)
        {
            const float x0 = cx + (float) i * 28.0f;
            juce::Path p;
            p.startNewSubPath (cx, horizon);
            p.lineTo (x0 < cx ? r.getX() - 200.0f : r.getRight() + 200.0f,
                      r.getBottom() + 50.0f);
            // recompute end-x: project i across the bottom
            p.clear();
            p.startNewSubPath (cx + (float) i * 6.0f, horizon);
            p.lineTo (cx + (float) i * 80.0f, r.getBottom());
            g.setColour (Colors::neonPink.withAlpha (0.18f));
            g.strokePath (p, juce::PathStrokeType (1.0f));
        }
        for (int i = 1; i < 14; ++i)
        {
            const float t = (float) i / 14.0f;
            const float y = horizon + std::pow (t, 1.6f) * (r.getBottom() - horizon);
            g.setColour (Colors::neonCyan.withAlpha (0.18f));
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        }
    }

    // Title
    g.setColour (Colors::neonCyan);
    g.setFont (juce::Font (juce::FontOptions (38.0f).withStyle ("Bold")));
    g.drawText ("VAPOR", 24, 14, 200, 44, juce::Justification::left);
    g.setColour (Colors::neonPink);
    g.drawText ("KEY",   140, 14, 200, 44, juce::Justification::left);
    g.setColour (Colors::textDim);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("WAVETABLE  /  SYNTHESIZER", 24, 52, 300, 14, juce::Justification::left);

    // Section backgrounds (filled before children paint).
    for (size_t i = 0; i < sectionRects.size(); ++i)
        drawSectionBg (g, sectionRects[i], sectionColors[i]);

    // Also frame the three oscillator panels as one unified strip
    for (int i = 0; i < 3; ++i)
    {
        if (! oscUI[i].display) continue;
        auto pb = juce::Rectangle<int> (oscUI[i].title.getX() - 4,
                                        oscUI[i].title.getY() - 4,
                                        0, 0);
        // build bounding box of the osc panel using its children
        auto box = oscUI[i].title.getBounds()
                       .getUnion (oscUI[i].shapeBox.getBounds())
                       .getUnion (oscUI[i].display->getBounds())
                       .getUnion (oscUI[i].position->getBounds())
                       .getUnion (oscUI[i].coarse->getBounds())
                       .getUnion (oscUI[i].detune->getBounds())
                       .expanded (6, 6);
        drawSectionBg (g, box, Colors::neonPink);
    }
}

void VaporKeyAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    for (size_t i = 0; i < sectionRects.size(); ++i)
    {
        g.setColour (sectionColors[i]);
        g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
        g.drawText (sectionTitles[i], sectionRects[i].getX() + 10,
                    sectionRects[i].getY() + 4, sectionRects[i].getWidth() - 20, 14,
                    juce::Justification::left);
    }
}

static void drawSectionBg (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour outline)
{
    auto rf = r.toFloat();
    g.setColour (Colors::panel.withAlpha (0.85f));
    g.fillRoundedRectangle (rf, 8.0f);
    g.setColour (outline.withAlpha (0.6f));
    g.drawRoundedRectangle (rf, 8.0f, 1.2f);
}

class SectionPanel : public juce::Component
{
public:
    SectionPanel (juce::String t, juce::Colour c) : title (std::move (t)), col (c) {}
    void paint (juce::Graphics& g) override
    {
        drawSectionBg (g, getLocalBounds(), col);
        g.setColour (col);
        g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
        g.drawText (title, 10, 4, getWidth() - 20, 16, juce::Justification::left);
    }
    juce::String title;
    juce::Colour col;
};

void VaporKeyAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromTop (74); // header

    const int rowH = (r.getHeight() - 12) / 2;
    auto top    = r.removeFromTop (rowH);
    r.removeFromTop (12);
    auto bottom = r;

    // ---------- TOP ROW: 3 oscillator panels ----------
    const int oscW = top.getWidth() / 3 - 8;
    for (int i = 0; i < 3; ++i)
    {
        auto panel = top.removeFromLeft (oscW);
        if (i < 2) top.removeFromLeft (12);

        // header strip
        auto header = panel.removeFromTop (24);
        oscUI[i].title.setBounds (header.removeFromLeft (60).reduced (4, 2));
        oscUI[i].onBtn.setBounds (header.removeFromLeft (44).reduced (2));
        header.removeFromLeft (4);
        oscUI[i].shapeBox.setBounds (header.reduced (2));

        // wavetable display
        auto disp = panel.removeFromTop (90);
        oscUI[i].display->setBounds (disp.reduced (4));

        // knob grid: pos lvl pan / semi fine uni det
        auto knobs = panel.reduced (4);
        const int kW = knobs.getWidth() / 4;
        const int kH = knobs.getHeight() / 2;

        auto row1 = knobs.removeFromTop (kH);
        oscUI[i].position->setBounds (row1.removeFromLeft (kW));
        oscUI[i].level   ->setBounds (row1.removeFromLeft (kW));
        oscUI[i].pan     ->setBounds (row1.removeFromLeft (kW));
        oscUI[i].unison  ->setBounds (row1.removeFromLeft (kW));

        auto row2 = knobs;
        oscUI[i].coarse->setBounds (row2.removeFromLeft (kW));
        oscUI[i].fine  ->setBounds (row2.removeFromLeft (kW));
        oscUI[i].detune->setBounds (row2.removeFromLeft (kW));
        // last cell empty for now
    }

    // ---------- BOTTOM ROW: filter | envs | lfo | warmth | fx | master ----------
    const int totalW = bottom.getWidth();
    const int gap = 8;
    const int sections = 6;
    const int sW = (totalW - gap * (sections - 1)) / sections;

    auto place = [&] (juce::Rectangle<int>& slot)
    {
        auto col = bottom.removeFromLeft (sW);
        if (bottom.getWidth() > 0) bottom.removeFromLeft (gap);
        slot = col;
    };

    juce::Rectangle<int> sFilter, sAmp, sMod, sLfo, sWarm, sFx;
    place (sFilter); place (sAmp); place (sMod); place (sLfo); place (sWarm); place (sFx);

    auto layoutKnobGrid = [] (juce::Rectangle<int> area, std::vector<juce::Component*> cs, int cols, int titleH = 22)
    {
        area.removeFromTop (titleH);
        area.reduce (6, 6);
        const int rows = (int) std::ceil ((double) cs.size() / cols);
        const int kw = area.getWidth() / cols;
        const int kh = area.getHeight() / juce::jmax (1, rows);
        for (size_t i = 0; i < cs.size(); ++i)
        {
            const int r0 = (int) i / cols;
            const int c0 = (int) i % cols;
            cs[i]->setBounds (area.getX() + c0 * kw, area.getY() + r0 * kh, kw, kh);
        }
    };

    // We don't add explicit SectionPanel components — paint() draws section bgs directly.
    // Instead we draw inside resized via overlay paint — but easier: just lay out knobs.
    // Filter
    {
        auto a = sFilter; a.removeFromTop (22);
        auto top2 = a.removeFromTop (22).reduced (4, 0);
        fTypeBox.setBounds (top2);
        layoutKnobGrid (a.reduced (0, 0), { kCut.get(), kRes.get(), kEnv.get() }, 3, 0);
    }
    layoutKnobGrid (sAmp, { kAmpA.get(), kAmpD.get(), kAmpS.get(), kAmpR.get() }, 4);
    layoutKnobGrid (sMod, { kModA.get(), kModD.get(), kModS.get(), kModR.get() }, 4);
    layoutKnobGrid (sLfo, { kLfo1Rate.get(), kLfo1Amt.get(), kLfo2Rate.get(), kLfo2Amt.get() }, 2);
    layoutKnobGrid (sWarm,{ kGrit.get(), kVibe.get(), kDrift.get(), kSat.get() }, 2);
    layoutKnobGrid (sFx,  { kChorus.get(), kDelay.get(), kDelayTime.get(), kDelayFb.get(), kReverb.get(), kGain.get() }, 3);

    // Stash sections to paint backgrounds via member (cheap re-layout call)
    sectionRects = { sFilter, sAmp, sMod, sLfo, sWarm, sFx };
    sectionTitles = { "FILTER", "AMP ENV", "MOD ENV", "LFO", "WARMTH", "FX / MASTER" };
    sectionColors = { Colors::neonCyan, Colors::neonPink, Colors::neonPurple,
                      Colors::neonAmber, Colors::neonAmber, Colors::neonCyan };
}
