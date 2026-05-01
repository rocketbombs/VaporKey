#include "PluginEditor.h"

using namespace VK;

namespace {

void drawSectionBg (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour outline, const juce::String& title)
{
    auto rf = r.toFloat();
    g.setColour (Colors::panel.withAlpha (0.85f));
    g.fillRoundedRectangle (rf, 8.0f);
    g.setColour (outline.withAlpha (0.6f));
    g.drawRoundedRectangle (rf, 8.0f, 1.2f);
    if (title.isNotEmpty())
    {
        g.setColour (outline);
        g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
        g.drawText (title, r.getX() + 10, r.getY() + 4, r.getWidth() - 20, 14, juce::Justification::left);
    }
}

void drawVaporBackdrop (juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::ColourGradient sky (juce::Colour (0xff1a0640), 0.0f, 0.0f,
                              juce::Colour (0xff05010f), 0.0f, r.getHeight(), false);
    sky.addColour (0.45, juce::Colour (0xffff2ec4).withAlpha (0.18f));
    sky.addColour (0.55, juce::Colour (0xff29f5ff).withAlpha (0.10f));
    g.setGradientFill (sky);
    g.fillRect (r);

    const float horizon = r.getHeight() * 0.55f;
    g.setColour (Colors::neonPink.withAlpha (0.5f));
    g.drawHorizontalLine ((int) horizon, r.getX(), r.getRight());

    const float cx = r.getCentreX();
    for (int i = -16; i <= 16; ++i)
    {
        juce::Path p;
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

void layoutKnobRow (juce::Rectangle<int> area, std::vector<juce::Component*> cs, int titleH = 22)
{
    area.removeFromTop (titleH);
    area.reduce (6, 6);
    if (cs.empty()) return;
    const int kw = area.getWidth() / (int) cs.size();
    for (auto* c : cs)
        if (c) c->setBounds (area.removeFromLeft (kw));
}

void layoutKnobGrid (juce::Rectangle<int> area, std::vector<juce::Component*> cs, int cols, int titleH = 22)
{
    area.removeFromTop (titleH);
    area.reduce (6, 6);
    if (cs.empty()) return;
    const int rows = (int) std::ceil ((double) cs.size() / cols);
    const int kw = area.getWidth() / cols;
    const int kh = area.getHeight() / juce::jmax (1, rows);
    for (size_t i = 0; i < cs.size(); ++i)
    {
        if (! cs[i]) continue;
        const int r0 = (int) i / cols;
        const int c0 = (int) i % cols;
        cs[i]->setBounds (area.getX() + c0 * kw, area.getY() + r0 * kh, kw, kh);
    }
}

} // namespace

// ---------- WavetableDisplay ----------

void WavetableDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (Colors::bg);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colors::neonPink.withAlpha (0.35f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    g.setColour (Colors::grid);
    for (int i = 1; i < 4; ++i)
    {
        const float y = r.getY() + r.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
    }

    const int shape = (int) (apvts.getRawParameterValue ("osc" + juce::String (idx + 1) + "_shape")->load() + 0.5f);
    const float pos = apvts.getRawParameterValue ("osc" + juce::String (idx + 1) + "_pos")->load();

    const auto& wt = WavetableLibrary::get().getTable (shape);
    juce::Path p;
    const int N = 256;
    for (int n = 0; n < N; ++n)
    {
        const float ph = (float) n / (float) N;
        const float v = wt.sample (pos, ph, 2);
        const float x = r.getX() + r.getWidth() * (float) n / (float) (N - 1);
        const float y = r.getCentreY() - v * r.getHeight() * 0.42f;
        if (n == 0) p.startNewSubPath (x, y);
        else        p.lineTo (x, y);
    }

    for (int i = 4; i > 0; --i)
    {
        g.setColour (Colors::neonCyan.withAlpha (0.10f * (float) i));
        g.strokePath (p, juce::PathStrokeType ((float) i));
    }
    g.setColour (Colors::neonCyan);
    g.strokePath (p, juce::PathStrokeType (1.4f));
}

// ---------- OscPage ----------

OscPage::OscPage (VaporKeyAudioProcessor& p) : proc (p)
{
    juce::StringArray shapes;
    for (int i = 0; i < WavetableLibrary::NumShapes; ++i) shapes.add (WavetableLibrary::shapeName (i));

    for (int i = 0; i < 3; ++i)
    {
        auto& u = oscUI[i];
        u.title.setText ("OSC " + juce::String (i + 1), juce::dontSendNotification);
        u.title.setJustificationType (juce::Justification::centredLeft);
        u.title.setColour (juce::Label::textColourId, Colors::neonPink);
        u.title.setFont (juce::Font (juce::FontOptions (14.0f).withStyle ("Bold")));
        addAndMakeVisible (u.title);

        const juce::String pf = "osc" + juce::String (i + 1) + "_";
        u.on    = std::make_unique<VaporToggle> (proc.apvts, pf + "on", "ON");      addAndMakeVisible (*u.on);
        u.shape = std::make_unique<VaporCombo>  (proc.apvts, pf + "shape", "Shape", shapes); addAndMakeVisible (*u.shape);
        u.display = std::make_unique<WavetableDisplay> (proc.apvts, i); addAndMakeVisible (*u.display);
        u.position = std::make_unique<VaporKnob> (proc.apvts, pf + "pos",    "Pos");  addAndMakeVisible (*u.position);
        u.level    = std::make_unique<VaporKnob> (proc.apvts, pf + "level",  "Lvl");  addAndMakeVisible (*u.level);
        u.pan      = std::make_unique<VaporKnob> (proc.apvts, pf + "pan",    "Pan");  addAndMakeVisible (*u.pan);
        u.coarse   = std::make_unique<VaporKnob> (proc.apvts, pf + "coarse", "Semi"); addAndMakeVisible (*u.coarse);
        u.fine     = std::make_unique<VaporKnob> (proc.apvts, pf + "fine",   "Fine"); addAndMakeVisible (*u.fine);
        u.unison   = std::make_unique<VaporKnob> (proc.apvts, pf + "unison", "Uni");  addAndMakeVisible (*u.unison);
        u.detune   = std::make_unique<VaporKnob> (proc.apvts, pf + "detune", "Det");  addAndMakeVisible (*u.detune);
        u.phase    = std::make_unique<VaporKnob> (proc.apvts, pf + "phase",  "Phase"); addAndMakeVisible (*u.phase);
    }

    subOn      = std::make_unique<VaporToggle>(proc.apvts, "sub_on", "SUB ON");           addAndMakeVisible (*subOn);
    subShape   = std::make_unique<VaporCombo> (proc.apvts, "sub_shape", "Shape", SubShape::names()); addAndMakeVisible (*subShape);
    subOct     = std::make_unique<VaporKnob>  (proc.apvts, "sub_oct", "Oct");             addAndMakeVisible (*subOct);
    subLevel   = std::make_unique<VaporKnob>  (proc.apvts, "sub_level", "Lvl");           addAndMakeVisible (*subLevel);

    noiseOn    = std::make_unique<VaporToggle>(proc.apvts, "noise_on", "NOISE ON");       addAndMakeVisible (*noiseOn);
    noiseColor = std::make_unique<VaporCombo> (proc.apvts, "noise_color", "Color", NoiseColor::names()); addAndMakeVisible (*noiseColor);
    noiseLevel = std::make_unique<VaporKnob>  (proc.apvts, "noise_level", "Lvl");         addAndMakeVisible (*noiseLevel);

    glide      = std::make_unique<VaporKnob>  (proc.apvts, "glide", "Glide");             addAndMakeVisible (*glide);
    mono       = std::make_unique<VaporToggle>(proc.apvts, "mono", "MONO");               addAndMakeVisible (*mono);
    legato     = std::make_unique<VaporToggle>(proc.apvts, "legato", "LEGATO");           addAndMakeVisible (*legato);
    bendRange  = std::make_unique<VaporKnob>  (proc.apvts, "bend_range", "Bend");         addAndMakeVisible (*bendRange);
}

void OscPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop ((int) (r.getHeight() * 0.62));
    r.removeFromTop (8);
    auto bottom = r;

    const int oscW = (top.getWidth() - 16) / 3;
    for (int i = 0; i < 3; ++i)
    {
        juce::Rectangle<int> panel (top.getX() + i * (oscW + 8), top.getY(), oscW, top.getHeight());
        drawSectionBg (g, panel, Colors::neonPink, "");
    }

    const int bw = bottom.getWidth();
    juce::Rectangle<int> sub  (bottom.getX(),                  bottom.getY(), bw / 3 - 4, bottom.getHeight());
    juce::Rectangle<int> noi  (bottom.getX() + bw / 3 + 2,     bottom.getY(), bw / 3 - 4, bottom.getHeight());
    juce::Rectangle<int> voc  (bottom.getX() + 2 * bw / 3 + 4, bottom.getY(), bw / 3 - 4, bottom.getHeight());
    drawSectionBg (g, sub, Colors::neonAmber, "SUB OSC");
    drawSectionBg (g, noi, Colors::neonAmber, "NOISE");
    drawSectionBg (g, voc, Colors::neonCyan,  "VOICING");
}

void OscPage::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop ((int) (r.getHeight() * 0.62));
    r.removeFromTop (8);

    // 3 osc panels
    const int oscW = (top.getWidth() - 16) / 3;
    for (int i = 0; i < 3; ++i)
    {
        auto panel = top.removeFromLeft (oscW);
        if (i < 2) top.removeFromLeft (8);

        auto header = panel.removeFromTop (24);
        oscUI[i].title.setBounds (header.removeFromLeft (60).reduced (4, 2));
        oscUI[i].on->setBounds (header.removeFromLeft (54).reduced (2));
        header.removeFromLeft (4);
        oscUI[i].shape->setBounds (header.reduced (2));

        auto disp = panel.removeFromTop (90);
        oscUI[i].display->setBounds (disp.reduced (4));

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
        oscUI[i].phase ->setBounds (row2.removeFromLeft (kW));
    }

    // Bottom row: sub | noise | voicing (glide+mono+legato+bend)
    auto bottom = r;
    const int bw = bottom.getWidth();
    juce::Rectangle<int> sub  (bottom.getX() + 0,                  bottom.getY(), bw / 3 - 4, bottom.getHeight());
    juce::Rectangle<int> noi  (bottom.getX() + bw / 3 + 2,         bottom.getY(), bw / 3 - 4, bottom.getHeight());
    juce::Rectangle<int> voc  (bottom.getX() + 2 * bw / 3 + 4,     bottom.getY(), bw / 3 - 4, bottom.getHeight());

    {
        auto a = sub; a.removeFromTop (22); a.reduce (6, 6);
        auto top1 = a.removeFromTop (28);
        subOn->setBounds (top1.removeFromLeft (a.getWidth() / 2).reduced (2));
        subShape->setBounds (top1.reduced (2));
        const int kw = a.getWidth() / 2;
        subOct->setBounds (a.removeFromLeft (kw));
        subLevel->setBounds (a);
    }
    {
        auto a = noi; a.removeFromTop (22); a.reduce (6, 6);
        auto top1 = a.removeFromTop (28);
        noiseOn->setBounds (top1.removeFromLeft (a.getWidth() / 2).reduced (2));
        noiseColor->setBounds (top1.reduced (2));
        noiseLevel->setBounds (a);
    }
    {
        auto a = voc; a.removeFromTop (22); a.reduce (6, 6);
        auto top1 = a.removeFromTop (28);
        mono->setBounds (top1.removeFromLeft (a.getWidth() / 2).reduced (2));
        legato->setBounds (top1.reduced (2));
        const int kw = a.getWidth() / 2;
        glide->setBounds (a.removeFromLeft (kw));
        bendRange->setBounds (a);
    }
}

// ---------- FilterEnvPage ----------

FilterEnvPage::FilterEnvPage (VaporKeyAudioProcessor& p)
{
    cut    = std::make_unique<VaporKnob>  (p.apvts, "f_cut",   "Cutoff");  addAndMakeVisible (*cut);
    res    = std::make_unique<VaporKnob>  (p.apvts, "f_res",   "Reso");    addAndMakeVisible (*res);
    env    = std::make_unique<VaporKnob>  (p.apvts, "f_env",   "Env");     addAndMakeVisible (*env);
    drive  = std::make_unique<VaporKnob>  (p.apvts, "f_drive", "Drive");   addAndMakeVisible (*drive);
    key    = std::make_unique<VaporKnob>  (p.apvts, "f_key",   "Key");     addAndMakeVisible (*key);
    type   = std::make_unique<VaporCombo> (p.apvts, "f_type",  "Type", { "LP", "BP", "HP" }); addAndMakeVisible (*type);

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
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop (r.getHeight() / 2 - 4);
    r.removeFromTop (8);
    auto bot = r;

    const int tW = top.getWidth();
    juce::Rectangle<int> sFilter (top.getX(),                top.getY(), tW * 6 / 10 - 4, top.getHeight());
    juce::Rectangle<int> sPitch  (top.getX() + tW * 6 / 10 + 4, top.getY(), tW * 4 / 10 - 4, top.getHeight());

    const int bW = bot.getWidth();
    juce::Rectangle<int> sAmp   (bot.getX(),                  bot.getY(), bW / 3 - 4, bot.getHeight());
    juce::Rectangle<int> sMod   (bot.getX() + bW / 3 + 2,     bot.getY(), bW / 3 - 4, bot.getHeight());
    juce::Rectangle<int> sWarm  (bot.getX() + 2 * bW / 3 + 4, bot.getY(), bW / 3 - 4, bot.getHeight());

    drawSectionBg (g, sFilter, Colors::neonCyan,   "FILTER");
    drawSectionBg (g, sPitch,  Colors::neonAmber,  "PITCH ENV");
    drawSectionBg (g, sAmp,    Colors::neonPink,   "AMP ENVELOPE");
    drawSectionBg (g, sMod,    Colors::neonPurple, "MOD ENVELOPE");
    drawSectionBg (g, sWarm,   Colors::neonAmber,  "WARMTH");
}

void FilterEnvPage::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop (r.getHeight() / 2 - 4);
    r.removeFromTop (8);
    auto bot = r;

    const int tW = top.getWidth();
    juce::Rectangle<int> sFilter (top.getX(),                top.getY(), tW * 6 / 10 - 4, top.getHeight());
    juce::Rectangle<int> sPitch  (top.getX() + tW * 6 / 10 + 4, top.getY(), tW * 4 / 10 - 4, top.getHeight());

    {
        auto a = sFilter; a.removeFromTop (22); a.reduce (6, 6);
        auto topRow = a.removeFromTop (28);
        type->setBounds (topRow.reduced (4, 2).withWidth (130));
        layoutKnobRow (a, { cut.get(), res.get(), env.get(), drive.get(), key.get() }, 0);
    }
    {
        auto a = sPitch; a.removeFromTop (22); a.reduce (6, 6);
        layoutKnobRow (a, { pAmt.get(), pDecay.get() }, 0);
    }

    const int bW = bot.getWidth();
    juce::Rectangle<int> sAmp   (bot.getX(),                  bot.getY(), bW / 3 - 4, bot.getHeight());
    juce::Rectangle<int> sMod   (bot.getX() + bW / 3 + 2,     bot.getY(), bW / 3 - 4, bot.getHeight());
    juce::Rectangle<int> sWarm  (bot.getX() + 2 * bW / 3 + 4, bot.getY(), bW / 3 - 4, bot.getHeight());

    layoutKnobRow (sAmp,  { aA.get(), aD.get(), aS.get(), aR.get(), aVel.get() });
    layoutKnobRow (sMod,  { mA.get(), mD.get(), mS.get(), mR.get(), fVel.get() });
    layoutKnobGrid (sWarm,{ grit.get(), vibe.get(), drift.get(), sat.get() }, 2);
}

// ---------- ModPage ----------

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
        macros[m].val  = std::make_unique<VaporKnob>  (p.apvts, pf + "val",  "M" + juce::String (m + 1));
        macros[m].dest = std::make_unique<VaporCombo> (p.apvts, pf + "dest", "Dest", destNames);
        macros[m].amt  = std::make_unique<VaporKnob>  (p.apvts, pf + "amt",  "Amt");
        addAndMakeVisible (*macros[m].val);
        addAndMakeVisible (*macros[m].dest);
        addAndMakeVisible (*macros[m].amt);
    }
}

void ModPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop (r.getHeight() / 2 - 4);
    r.removeFromTop (8);
    auto bot = r;

    const int tw = top.getWidth();
    juce::Rectangle<int> s1 (top.getX(),              top.getY(), tw / 2 - 4, top.getHeight());
    juce::Rectangle<int> s2 (top.getX() + tw / 2 + 4, top.getY(), tw / 2 - 4, top.getHeight());
    drawSectionBg (g, s1, Colors::neonCyan, "LFO 1  (-> CUTOFF)");
    drawSectionBg (g, s2, Colors::neonPink, "LFO 2  (-> POSITION)");

    const int mw = bot.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        juce::Rectangle<int> mr (bot.getX() + i * mw + (i > 0 ? 4 : 0), bot.getY(), mw - 4, bot.getHeight());
        drawSectionBg (g, mr, Colors::neonAmber, "MACRO " + juce::String (i + 1));
    }
}

void ModPage::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop (r.getHeight() / 2 - 4);
    r.removeFromTop (8);
    auto bot = r;

    const int tw = top.getWidth();
    juce::Rectangle<int> s1 (top.getX(),              top.getY(), tw / 2 - 4, top.getHeight());
    juce::Rectangle<int> s2 (top.getX() + tw / 2 + 4, top.getY(), tw / 2 - 4, top.getHeight());

    auto layoutLfo = [] (juce::Rectangle<int> a, VaporCombo* shape, VaporKnob* rate, VaporKnob* amt,
                         VaporToggle* sync, VaporCombo* div)
    {
        a.removeFromTop (22); a.reduce (6, 6);
        auto top1 = a.removeFromTop (28);
        shape->setBounds (top1.removeFromLeft (a.getWidth() / 3));
        sync->setBounds  (top1.removeFromLeft (60).reduced (2));
        div->setBounds   (top1);
        layoutKnobRow (a, { rate, amt }, 0);
    };
    layoutLfo (s1, l1Shape.get(), l1Rate.get(), l1Amt.get(), l1Sync.get(), l1Div.get());
    layoutLfo (s2, l2Shape.get(), l2Rate.get(), l2Amt.get(), l2Sync.get(), l2Div.get());

    const int mw = bot.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        juce::Rectangle<int> mr (bot.getX() + i * mw + (i > 0 ? 4 : 0), bot.getY(), mw - 4, bot.getHeight());
        mr.removeFromTop (22); mr.reduce (6, 6);
        macros[i].dest->setBounds (mr.removeFromTop (40));
        layoutKnobRow (mr, { macros[i].val.get(), macros[i].amt.get() }, 0);
    }
}

// ---------- FxPage ----------

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

    eqLow   = std::make_unique<VaporKnob> (p.apvts, "eq_low",      "Low");  addAndMakeVisible (*eqLow);
    eqMid   = std::make_unique<VaporKnob> (p.apvts, "eq_mid",      "Mid");  addAndMakeVisible (*eqMid);
    eqMidF  = std::make_unique<VaporKnob> (p.apvts, "eq_mid_freq", "Freq"); addAndMakeVisible (*eqMidF);
    eqHigh  = std::make_unique<VaporKnob> (p.apvts, "eq_high",     "High"); addAndMakeVisible (*eqHigh);

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
    auto r = getLocalBounds().reduced (8);
    const int rowH = (r.getHeight() - 16) / 3;
    auto row1 = r.removeFromTop (rowH); r.removeFromTop (8);
    auto row2 = r.removeFromTop (rowH); r.removeFromTop (8);
    auto row3 = r;

    const int w1 = row1.getWidth();
    juce::Rectangle<int> sDist  (row1.getX(),                    row1.getY(), w1 / 3 - 4, row1.getHeight());
    juce::Rectangle<int> sChor  (row1.getX() + w1 / 3 + 2,       row1.getY(), w1 / 3 - 4, row1.getHeight());
    juce::Rectangle<int> sPhas  (row1.getX() + 2 * w1 / 3 + 4,   row1.getY(), w1 / 3 - 4, row1.getHeight());
    drawSectionBg (g, sDist, Colors::neonAmber, "DISTORTION");
    drawSectionBg (g, sChor, Colors::neonPink,  "CHORUS");
    drawSectionBg (g, sPhas, Colors::neonPurple,"PHASER");

    const int w2 = row2.getWidth();
    juce::Rectangle<int> sEq    (row2.getX(),                    row2.getY(), w2 / 2 - 4, row2.getHeight());
    juce::Rectangle<int> sDly   (row2.getX() + w2 / 2 + 4,       row2.getY(), w2 / 2 - 4, row2.getHeight());
    drawSectionBg (g, sEq,  Colors::neonCyan, "EQ");
    drawSectionBg (g, sDly, Colors::neonPink, "DELAY");

    const int w3 = row3.getWidth();
    juce::Rectangle<int> sRv    (row3.getX(),                    row3.getY(), w3 * 2 / 5 - 4, row3.getHeight());
    juce::Rectangle<int> sCmp   (row3.getX() + w3 * 2 / 5 + 4,   row3.getY(), w3 * 3 / 5 - 4, row3.getHeight());
    drawSectionBg (g, sRv,  Colors::neonAmber, "REVERB");
    drawSectionBg (g, sCmp, Colors::neonCyan,  "COMPRESSOR");
}

void FxPage::resized()
{
    auto r = getLocalBounds().reduced (8);
    const int rowH = (r.getHeight() - 16) / 3;
    auto row1 = r.removeFromTop (rowH); r.removeFromTop (8);
    auto row2 = r.removeFromTop (rowH); r.removeFromTop (8);
    auto row3 = r;

    const int w1 = row1.getWidth();
    juce::Rectangle<int> sDist  (row1.getX(),                    row1.getY(), w1 / 3 - 4, row1.getHeight());
    juce::Rectangle<int> sChor  (row1.getX() + w1 / 3 + 2,       row1.getY(), w1 / 3 - 4, row1.getHeight());
    juce::Rectangle<int> sPhas  (row1.getX() + 2 * w1 / 3 + 4,   row1.getY(), w1 / 3 - 4, row1.getHeight());

    {
        auto a = sDist; a.removeFromTop (22); a.reduce (6, 6);
        distType->setBounds (a.removeFromTop (28).reduced (4, 2));
        layoutKnobRow (a, { distDrive.get(), distMix.get() }, 0);
    }
    layoutKnobRow (sChor, { chMix.get(), chRate.get(), chDepth.get() });
    layoutKnobRow (sPhas, { phMix.get(), phRate.get(), phDepth.get(), phFb.get() });

    const int w2 = row2.getWidth();
    juce::Rectangle<int> sEq    (row2.getX(),                    row2.getY(), w2 / 2 - 4, row2.getHeight());
    juce::Rectangle<int> sDly   (row2.getX() + w2 / 2 + 4,       row2.getY(), w2 / 2 - 4, row2.getHeight());
    layoutKnobRow (sEq,  { eqLow.get(), eqMid.get(), eqMidF.get(), eqHigh.get() });
    {
        auto a = sDly; a.removeFromTop (22); a.reduce (6, 6);
        auto top1 = a.removeFromTop (28);
        dlSync->setBounds (top1.removeFromLeft (60).reduced (2));
        dlDiv->setBounds  (top1.removeFromLeft (130));
        layoutKnobRow (a, { dlMix.get(), dlTime.get(), dlFb.get() }, 0);
    }

    const int w3 = row3.getWidth();
    juce::Rectangle<int> sRv    (row3.getX(),                    row3.getY(), w3 * 2 / 5 - 4, row3.getHeight());
    juce::Rectangle<int> sCmp   (row3.getX() + w3 * 2 / 5 + 4,   row3.getY(), w3 * 3 / 5 - 4, row3.getHeight());
    layoutKnobRow (sRv,  { rvMix.get(), rvSize.get(), rvDamp.get() });
    {
        auto a = sCmp; a.removeFromTop (22); a.reduce (6, 6);
        compOn->setBounds (a.removeFromTop (28).reduced (4, 2).withWidth (80));
        layoutKnobRow (a, { compThr.get(), compRatio.get(), compAtk.get(), compRel.get(), compMakeup.get() }, 0);
    }
}

// ---------- MasterPage ----------

MasterPage::MasterPage (VaporKeyAudioProcessor& p) : proc (p)
{
    gain  = std::make_unique<VaporKnob> (p.apvts, "gain",  "Master"); addAndMakeVisible (*gain);
    width = std::make_unique<VaporKnob> (p.apvts, "width", "Width");  addAndMakeVisible (*width);

    auto names = VaporKeyAudioProcessor::factoryPresetNames();
    for (int i = 0; i < names.size(); ++i) presetBox.addItem (names[i], i + 1);
    presetBox.setTextWhenNothingSelected ("(Select preset)");
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0) proc.loadFactoryPreset (idx);
    };
    addAndMakeVisible (presetBox);

    prevBtn.onClick = [this] {
        const int n = VaporKeyAudioProcessor::factoryPresetNames().size();
        int idx = juce::jmax (0, presetBox.getSelectedItemIndex());
        idx = (idx - 1 + n) % n;
        presetBox.setSelectedItemIndex (idx, juce::sendNotificationSync);
    };
    nextBtn.onClick = [this] {
        const int n = VaporKeyAudioProcessor::factoryPresetNames().size();
        int idx = juce::jmax (0, presetBox.getSelectedItemIndex());
        idx = (idx + 1) % n;
        presetBox.setSelectedItemIndex (idx, juce::sendNotificationSync);
    };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);

    about.setText ("VAPORKEY  /  v0.2\n"
                   "Wavetable synthesizer with analog warmth.\n"
                   "10 wavetables  -  3 oscs + sub + noise  -  16-voice poly\n"
                   "ADSR amp/mod, pitch env, 2 LFOs, 4 macros\n"
                   "Distortion, Chorus, Phaser, EQ, Delay, Reverb, Comp\n"
                   "Grit / Vibe / Drift / Sat for that retro-analog vibe.\n"
                   "Built with JUCE.  -  RocketBombs", juce::dontSendNotification);
    about.setJustificationType (juce::Justification::topLeft);
    about.setColour (juce::Label::textColourId, Colors::textDim);
    about.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (about);
}

void MasterPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (8);
    const int leftW = r.getWidth() / 3;
    juce::Rectangle<int> sMaster (r.getX(), r.getY(), leftW - 4, r.getHeight());
    juce::Rectangle<int> sPreset (r.getX() + leftW + 4, r.getY(), r.getWidth() - leftW - 4, r.getHeight());
    drawSectionBg (g, sMaster, Colors::neonCyan, "MASTER");
    drawSectionBg (g, sPreset, Colors::neonPink, "PRESETS / ABOUT");
}

void MasterPage::resized()
{
    auto r = getLocalBounds().reduced (8);
    const int leftW = r.getWidth() / 3;
    juce::Rectangle<int> sMaster (r.getX(), r.getY(), leftW - 4, r.getHeight());
    juce::Rectangle<int> sPreset (r.getX() + leftW + 4, r.getY(), r.getWidth() - leftW - 4, r.getHeight());

    layoutKnobRow (sMaster, { gain.get(), width.get() });

    {
        auto a = sPreset; a.removeFromTop (22); a.reduce (10, 10);
        auto top1 = a.removeFromTop (32);
        prevBtn.setBounds (top1.removeFromLeft (32));
        top1.removeFromLeft (4);
        nextBtn.setBounds (top1.removeFromLeft (32));
        top1.removeFromLeft (8);
        presetBox.setBounds (top1);
        a.removeFromTop (12);
        about.setBounds (a);
    }
}

// ---------- Editor ----------

VaporKeyAudioProcessorEditor::VaporKeyAudioProcessorEditor (VaporKeyAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    tabs.setTabBarDepth (28);
    tabs.setOutline (0);
    tabs.addTab ("OSCILLATORS", Colors::panel,    new OscPage (proc),       true);
    tabs.addTab ("FILTER & ENV", Colors::panel,   new FilterEnvPage (proc), true);
    tabs.addTab ("MOD",         Colors::panel,    new ModPage (proc),       true);
    tabs.addTab ("FX",          Colors::panel,    new FxPage (proc),        true);
    tabs.addTab ("MASTER",      Colors::panel,    new MasterPage (proc),    true);

    tabs.setColour (juce::TabbedComponent::backgroundColourId, juce::Colours::transparentBlack);
    tabs.setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);

    addAndMakeVisible (tabs);

    setResizable (true, true);
    setResizeLimits (1000, 620, 1800, 1100);
    setSize (1180, 720);
}

VaporKeyAudioProcessorEditor::~VaporKeyAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void VaporKeyAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawVaporBackdrop (g, r);

    // Sun
    {
        const float cx = r.getCentreX();
        const float cy = r.getY() + 60.0f;
        const float rad = 60.0f;
        juce::ColourGradient sun (juce::Colour (0xffffd166), cx, cy - rad,
                                  juce::Colour (0xffff2ec4), cx, cy + rad, false);
        g.setGradientFill (sun);
        g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
        g.setColour (juce::Colour (0xff05010f));
        for (int i = 0; i < 5; ++i)
            g.fillRect (cx - rad, cy + (float) i * 5.0f - 5.0f, rad * 2.0f, 2.0f);
    }

    // Title
    g.setColour (Colors::neonCyan);
    g.setFont (juce::Font (juce::FontOptions (32.0f).withStyle ("Bold")));
    g.drawText ("VAPOR", 24, 12, 200, 40, juce::Justification::left);
    g.setColour (Colors::neonPink);
    g.drawText ("KEY",   132, 12, 200, 40, juce::Justification::left);
    g.setColour (Colors::textDim);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("WAVETABLE  /  SYNTHESIZER", 24, 46, 300, 14, juce::Justification::left);
}

void VaporKeyAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (60); // header
    tabs.setBounds (r.reduced (6));
}
