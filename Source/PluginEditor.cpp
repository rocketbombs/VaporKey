#include "PluginEditor.h"

using namespace VK;

// =====================================================================
// Local helpers
// =====================================================================
namespace {

void drawSectionBg (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour outline, const juce::String& title)
{
    auto rf = r.toFloat();
    juce::ColourGradient grad (Colors::panel.withAlpha (0.92f), 0.0f, rf.getY(),
                               Colors::panel.darker (0.2f).withAlpha (0.92f), 0.0f, rf.getBottom(),
                               false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (rf, 10.0f);

    g.setColour (outline.withAlpha (0.65f));
    g.drawRoundedRectangle (rf, 10.0f, 1.4f);

    if (title.isNotEmpty())
    {
        // Title strip (subtle neon underline)
        g.setColour (outline);
        g.setFont (Fonts::section());
        g.drawText (title, r.getX() + 14, r.getY() + 8, r.getWidth() - 28, 16,
                    juce::Justification::left);
        g.setColour (outline.withAlpha (0.4f));
        g.drawHorizontalLine (r.getY() + 27, (float) r.getX() + 14.0f, (float) r.getRight() - 14.0f);
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

constexpr int kSectionTitleH = 36;

void layoutKnobRow (juce::Rectangle<int> area, std::vector<juce::Component*> cs, int titleH = kSectionTitleH)
{
    area.removeFromTop (titleH);
    area.reduce (10, 8);
    if (cs.empty()) return;
    const int kw = area.getWidth() / (int) cs.size();
    for (auto* c : cs)
        if (c) c->setBounds (area.removeFromLeft (kw));
}

void layoutKnobGrid (juce::Rectangle<int> area, std::vector<juce::Component*> cs, int cols, int titleH = kSectionTitleH)
{
    area.removeFromTop (titleH);
    area.reduce (10, 8);
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

// =====================================================================
// VaporKnob
// =====================================================================

VaporKnob::VaporKnob (juce::AudioProcessorValueTreeState& s, const juce::String& paramID, const juce::String& displayName)
    : name (displayName)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 22);
    slider.setName (displayName);
    slider.setColour (juce::Slider::textBoxTextColourId, Colors::textBright);
    addAndMakeVisible (slider);

    label.setText (displayName, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, Colors::neonCyan);
    label.setFont (Fonts::label());
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramID, slider);
}

void VaporKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (18));
    slider.setBounds (r);
}

// =====================================================================
// VaporCombo
// =====================================================================

VaporCombo::VaporCombo (juce::AudioProcessorValueTreeState& s, const juce::String& paramID,
                       const juce::String& displayName, const juce::StringArray& items)
{
    for (int i = 0; i < items.size(); ++i) box.addItem (items[i], i + 1);
    addAndMakeVisible (box);

    label.setText (displayName, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, Colors::neonCyan);
    label.setFont (Fonts::label());
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, paramID, box);
}

void VaporCombo::resized()
{
    auto r = getLocalBounds();
    // Show label only if there's enough vertical room — otherwise give all
    // of the height to the combo box itself so single-row headers don't
    // clip the dropdown text.
    if (r.getHeight() >= 44)
        label.setBounds (r.removeFromTop (18));
    else
        label.setVisible (false);
    box.setBounds (r.reduced (2, 1));
}

// =====================================================================
// VaporToggle
// =====================================================================

VaporToggle::VaporToggle (juce::AudioProcessorValueTreeState& s, const juce::String& paramID, const juce::String& text)
{
    btn.setButtonText (text);
    btn.setClickingTogglesState (true);
    addAndMakeVisible (btn);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, paramID, btn);
}

void VaporToggle::resized() { btn.setBounds (getLocalBounds().reduced (2)); }

// =====================================================================
// WavetableDisplay
// =====================================================================

WavetableDisplay::WavetableDisplay (juce::AudioProcessorValueTreeState& s, int oscIndex)
    : apvts (s), idx (oscIndex)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setOpaque (false);
    startTimerHz (24);
}

void WavetableDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);

    juce::ColourGradient grad (Colors::bg, 0.0f, r.getY(),
                               Colors::bg.darker (0.6f), 0.0f, r.getBottom(),
                               false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 6.0f);

    g.setColour (Colors::neonPink.withAlpha (0.45f));
    g.drawRoundedRectangle (r, 6.0f, 1.2f);

    // grid
    g.setColour (Colors::grid);
    for (int i = 1; i < 4; ++i)
    {
        const float y = r.getY() + r.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine ((int) y, r.getX() + 4.0f, r.getRight() - 4.0f);
    }
    for (int i = 1; i < 8; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / 8.0f;
        g.setColour (Colors::neonPink.withAlpha (0.07f));
        g.drawVerticalLine ((int) x, r.getY() + 4.0f, r.getBottom() - 4.0f);
    }

    const int shape = (int) (apvts.getRawParameterValue ("osc" + juce::String (idx + 1) + "_shape")->load() + 0.5f);
    const float pos = apvts.getRawParameterValue ("osc" + juce::String (idx + 1) + "_pos")->load();

    const auto& wt = WavetableLibrary::get().getTable (shape);

    // Ghost frame: slightly different position to hint at morphing direction.
    const float ghostPos = juce::jlimit (0.0f, 1.0f, pos + 0.07f);

    auto buildPath = [&](float p) {
        juce::Path path;
        const int N = 320;
        for (int n = 0; n < N; ++n)
        {
            const float ph = (float) n / (float) N;
            const float v = wt.sample (p, ph, 2);
            const float x = r.getX() + 4.0f + (r.getWidth() - 8.0f) * (float) n / (float) (N - 1);
            const float y = r.getCentreY() - v * (r.getHeight() - 16.0f) * 0.45f;
            if (n == 0) path.startNewSubPath (x, y);
            else        path.lineTo (x, y);
        }
        return path;
    };

    auto ghost = buildPath (ghostPos);
    g.setColour (Colors::neonCyan.withAlpha (0.15f));
    g.strokePath (ghost, juce::PathStrokeType (1.0f));

    auto wave = buildPath (pos);
    for (int i = 4; i > 0; --i)
    {
        g.setColour (Colors::neonCyan.withAlpha (0.10f * (float) i));
        g.strokePath (wave, juce::PathStrokeType ((float) i + 1.0f));
    }
    g.setColour (Colors::neonCyan);
    g.strokePath (wave, juce::PathStrokeType (1.6f));

    // Overlay text: shape name top-left, position % top-right
    g.setColour (Colors::textBright);
    g.setFont (Fonts::label());
    g.drawText (juce::String (WavetableLibrary::shapeName (shape)).toUpperCase(),
                (int) r.getX() + 10, (int) r.getY() + 6, 200, 16, juce::Justification::left);

    g.setColour (Colors::neonPink);
    g.drawText (juce::String (juce::roundToInt (pos * 100.0f)) + "%",
                (int) r.getRight() - 60, (int) r.getY() + 6, 50, 16, juce::Justification::right);

    // Hint text bottom-right
    g.setColour (Colors::textDim);
    g.setFont (Fonts::small());
    g.drawText ("DRAG TO SCRUB",
                (int) r.getRight() - 110, (int) r.getBottom() - 18, 100, 14,
                juce::Justification::right);

    // Position scrub indicator (vertical line)
    const float ix = r.getX() + 4.0f + (r.getWidth() - 8.0f) * pos;
    g.setColour (Colors::neonPink.withAlpha (0.7f));
    g.drawVerticalLine ((int) ix, r.getY() + 24.0f, r.getBottom() - 6.0f);
}

void WavetableDisplay::setPositionFromMouse (const juce::MouseEvent& e)
{
    auto r = getLocalBounds().reduced (2);
    const float x01 = juce::jlimit (0.0f, 1.0f, (float) (e.x - r.getX()) / (float) juce::jmax (1, r.getWidth()));
    if (auto* p = apvts.getParameter ("osc" + juce::String (idx + 1) + "_pos"))
    {
        // The position parameter range is 0..1, so the normalized value is just x01.
        p->setValueNotifyingHost (x01);
    }
}

void WavetableDisplay::mouseDown (const juce::MouseEvent& e)        { setPositionFromMouse (e); }
void WavetableDisplay::mouseDrag (const juce::MouseEvent& e)        { setPositionFromMouse (e); }
void WavetableDisplay::mouseDoubleClick (const juce::MouseEvent&)
{
    if (auto* p = apvts.getParameter ("osc" + juce::String (idx + 1) + "_pos"))
        p->setValueNotifyingHost (p->getDefaultValue());
}

// =====================================================================
// OscPage
// =====================================================================

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
        u.title.setFont (Fonts::subheader());
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
}

void OscPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop ((int) (r.getHeight() * 0.62));
    r.removeFromTop (10);
    auto bottom = r;

    const int oscW = (top.getWidth() - 20) / 3;
    for (int i = 0; i < 3; ++i)
    {
        juce::Rectangle<int> panel (top.getX() + i * (oscW + 10), top.getY(), oscW, top.getHeight());
        drawSectionBg (g, panel, Colors::neonPink, "");
    }

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
    auto top = r.removeFromTop ((int) (r.getHeight() * 0.62));
    r.removeFromTop (10);
    auto bottom = r;

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

        // Wavetable display
        auto disp = osc.removeFromTop ((int) (osc.getHeight() * 0.34));
        oscUI[i].display->setBounds (disp.reduced (4));

        // Knobs grid 4 cols × 2 rows
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
    layoutSubLikeBox (sub, subOn.get(),   subShape.get(),   subOct.get(),   subLevel.get());

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

// =====================================================================
// FilterEnvPage
// =====================================================================

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

// =====================================================================
// ModPage
// =====================================================================

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
}

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

    const int mw = bot.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        juce::Rectangle<int> mr (bot.getX() + i * mw + (i > 0 ? 5 : 0), bot.getY(), mw - 5, bot.getHeight());
        drawSectionBg (g, mr, Colors::neonAmber, "MACRO " + juce::String (i + 1));
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

    const int mw = bot.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        juce::Rectangle<int> mr (bot.getX() + i * mw + (i > 0 ? 5 : 0), bot.getY(), mw - 5, bot.getHeight());
        mr.removeFromTop (kSectionTitleH); mr.reduce (10, 8);
        macros[i].dest->setBounds (mr.removeFromTop (50));
        layoutKnobRow (mr, { macros[i].val.get(), macros[i].amt.get() }, 0);
    }
}

// =====================================================================
// FxPage
// =====================================================================

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
    layoutKnobRow (sEq,  { eqLow.get(), eqMid.get(), eqMidF.get(), eqHigh.get() });
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

// =====================================================================
// MasterPage
// =====================================================================

int MasterPage::PresetListModel::getNumRows()
{
    return (int) owner.entries.size();
}

void MasterPage::PresetListModel::paintListBoxItem (int row, juce::Graphics& g,
                                                    int width, int height, bool selected)
{
    if (row < 0 || row >= (int) owner.entries.size()) return;
    const auto& entry = owner.entries[(size_t) row];

    if (selected)
    {
        g.setColour (Colors::neonPink.withAlpha (0.30f));
        g.fillRect (0, 0, width, height);
        g.setColour (Colors::neonPink);
        g.fillRect (0, 0, 4, height);
    }
    else
    {
        g.setColour (Colors::panelHi.withAlpha (0.4f));
        g.fillRect (0, height - 1, width, 1);
    }

    // Type tag (F / U) on left
    g.setFont (Fonts::small());
    g.setColour (entry.isFactory ? Colors::neonCyan.withAlpha (0.85f)
                                 : Colors::neonAmber.withAlpha (0.85f));
    g.drawText (entry.isFactory ? "F" : "U", 12, 0, 18, height, juce::Justification::centred);

    // Name
    g.setColour (selected ? Colors::textBright
                          : (entry.isFactory ? Colors::text : Colors::neonAmber.brighter (0.4f)));
    g.setFont (Fonts::preset());
    g.drawText (entry.name, 36, 0, width - 56, height, juce::Justification::centredLeft);

    if (selected)
    {
        g.setColour (Colors::neonPink);
        g.setFont (Fonts::small());
        g.drawText ("PLAYING", width - 80, 0, 70, height, juce::Justification::centredRight);
    }
}

void MasterPage::PresetListModel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    owner.loadEntry (row);
}

void MasterPage::rebuildEntries()
{
    entries.clear();
    for (const auto& n : VaporKeyAudioProcessor::factoryPresetNames())
        entries.push_back ({ n, true });
    for (const auto& n : proc.getUserPresetNames())
        entries.push_back ({ n, false });
    presetList.updateContent();
}

int MasterPage::findEntryIndex (const juce::String& name, bool isFactory) const
{
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].isFactory == isFactory && entries[i].name == name)
            return (int) i;
    return -1;
}

void MasterPage::loadEntry (int idx)
{
    if (idx < 0 || idx >= (int) entries.size()) return;
    const auto& e = entries[(size_t) idx];
    if (e.isFactory)
    {
        // Find factory index
        const auto names = VaporKeyAudioProcessor::factoryPresetNames();
        const int fi = names.indexOf (e.name);
        if (fi >= 0) proc.loadFactoryPreset (fi);
    }
    else
    {
        proc.loadUserPresetByName (e.name);
    }
    presetList.selectRow (idx);
    refreshNowPlaying();
    nameField.setText (e.isFactory ? juce::String() : e.name, juce::dontSendNotification);
}

void MasterPage::refreshNowPlaying()
{
    const juce::String name = proc.currentPresetName.isNotEmpty()
                                ? proc.currentPresetName
                                : juce::String ("(unnamed)");
    presetNowLabel.setText (name, juce::dontSendNotification);
    const int idx = findEntryIndex (proc.currentPresetName, proc.currentPresetIsFactory);
    if (idx >= 0) presetList.selectRow (idx, false, true);
}

void MasterPage::stepPreset (int dir)
{
    const int n = (int) entries.size();
    if (n == 0) return;
    int cur = juce::jmax (0, presetList.getSelectedRow());
    cur = (cur + dir + n) % n;
    loadEntry (cur);
}

void MasterPage::showStatus (const juce::String& msg, juce::Colour col)
{
    presetLabel.setText (msg, juce::dontSendNotification);
    presetLabel.setColour (juce::Label::textColourId, col);
    juce::Component::SafePointer<MasterPage> sp (this);
    juce::Timer::callAfterDelay (1800, [sp]
    {
        if (auto* p = sp.getComponent())
        {
            p->presetLabel.setText ("PRESETS", juce::dontSendNotification);
            p->presetLabel.setColour (juce::Label::textColourId, Colors::neonPink);
        }
    });
}

void MasterPage::onSave()
{
    auto name = nameField.getText().trim();
    if (name.isEmpty())
    {
        // Auto-name with a number suffix
        int n = 1;
        for (;; ++n)
        {
            auto candidate = "User Preset " + juce::String (n);
            if (! proc.getUserPresetNames().contains (candidate))
            {
                name = candidate;
                break;
            }
        }
    }
    if (proc.saveUserPreset (name))
    {
        rebuildEntries();
        const int idx = findEntryIndex (proc.currentPresetName, false);
        if (idx >= 0) presetList.selectRow (idx);
        refreshNowPlaying();
        nameField.setText (name, juce::dontSendNotification);
        showStatus ("SAVED  -  " + name.toUpperCase(), Colors::neonGreen);
    }
    else
    {
        showStatus ("SAVE FAILED", Colors::neonAmber);
    }
}

void MasterPage::onRename()
{
    const int row = presetList.getSelectedRow();
    if (row < 0 || row >= (int) entries.size()) { showStatus ("SELECT A PRESET", Colors::neonAmber); return; }
    const auto& e = entries[(size_t) row];
    if (e.isFactory) { showStatus ("CANNOT RENAME FACTORY", Colors::neonAmber); return; }

    const auto newName = nameField.getText().trim();
    if (newName.isEmpty()) { showStatus ("ENTER A NEW NAME", Colors::neonAmber); return; }

    if (proc.renameUserPreset (e.name, newName))
    {
        rebuildEntries();
        const int idx = findEntryIndex (newName, false);
        if (idx >= 0) presetList.selectRow (idx);
        refreshNowPlaying();
        showStatus ("RENAMED", Colors::neonGreen);
    }
    else
    {
        showStatus ("RENAME FAILED", Colors::neonAmber);
    }
}

void MasterPage::onDelete()
{
    const int row = presetList.getSelectedRow();
    if (row < 0 || row >= (int) entries.size()) { showStatus ("SELECT A PRESET", Colors::neonAmber); return; }
    const auto& e = entries[(size_t) row];
    if (e.isFactory) { showStatus ("CANNOT DELETE FACTORY", Colors::neonAmber); return; }

    const auto deletedName = e.name;
    if (proc.deleteUserPreset (deletedName))
    {
        rebuildEntries();
        if (proc.currentPresetName == deletedName)
            presetNowLabel.setText ("(unnamed)", juce::dontSendNotification);
        nameField.setText ({}, juce::dontSendNotification);
        showStatus ("DELETED", Colors::neonGreen);
    }
    else
    {
        showStatus ("DELETE FAILED", Colors::neonAmber);
    }
}

MasterPage::MasterPage (VaporKeyAudioProcessor& p) : proc (p)
{
    gain  = std::make_unique<VaporKnob> (p.apvts, "gain",  "Master"); addAndMakeVisible (*gain);
    width = std::make_unique<VaporKnob> (p.apvts, "width", "Width");  addAndMakeVisible (*width);

    presetLabel.setText ("PRESETS", juce::dontSendNotification);
    presetLabel.setFont (Fonts::section());
    presetLabel.setColour (juce::Label::textColourId, Colors::neonPink);
    presetLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (presetLabel);

    // Initialize current name from program
    if (proc.currentPresetName.isEmpty())
    {
        proc.currentPresetName = VaporKeyAudioProcessor::factoryPresetNames()[proc.getCurrentProgram()];
        proc.currentPresetIsFactory = true;
    }
    presetNowLabel.setText (proc.currentPresetName, juce::dontSendNotification);
    presetNowLabel.setFont (Fonts::header());
    presetNowLabel.setColour (juce::Label::textColourId, Colors::neonCyan);
    presetNowLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (presetNowLabel);

    presetModel = std::make_unique<PresetListModel> (*this);
    presetList.setModel (presetModel.get());
    presetList.setRowHeight (30);
    presetList.setColour (juce::ListBox::backgroundColourId, Colors::bg.darker (0.2f));
    presetList.setColour (juce::ListBox::outlineColourId,    Colors::neonCyan.withAlpha (0.45f));
    presetList.setOutlineThickness (1);
    addAndMakeVisible (presetList);

    rebuildEntries();
    {
        const int idx = findEntryIndex (proc.currentPresetName, proc.currentPresetIsFactory);
        if (idx >= 0) presetList.selectRow (idx);
    }

    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (+1); };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);

    nameLabel.setText ("NAME", juce::dontSendNotification);
    nameLabel.setFont (Fonts::section());
    nameLabel.setColour (juce::Label::textColourId, Colors::neonCyan);
    nameLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (nameLabel);

    nameField.setFont (Fonts::preset());
    nameField.setColour (juce::TextEditor::backgroundColourId, Colors::panelHi2);
    nameField.setColour (juce::TextEditor::textColourId, Colors::textBright);
    nameField.setColour (juce::TextEditor::outlineColourId, Colors::neonCyan.withAlpha (0.5f));
    nameField.setColour (juce::TextEditor::focusedOutlineColourId, Colors::neonPink);
    nameField.setColour (juce::TextEditor::highlightColourId, Colors::neonPink.withAlpha (0.35f));
    nameField.setTextToShowWhenEmpty ("Type a name and SAVE", Colors::textDim);
    nameField.setIndents (8, 4);
    nameField.onReturnKey = [this] { onSave(); };
    addAndMakeVisible (nameField);

    saveBtn.onClick   = [this] { onSave(); };
    renameBtn.onClick = [this] { onRename(); };
    deleteBtn.onClick = [this] { onDelete(); };
    addAndMakeVisible (saveBtn);
    addAndMakeVisible (renameBtn);
    addAndMakeVisible (deleteBtn);

    brand.setText ("VAPORKEY", juce::dontSendNotification);
    brand.setFont (Fonts::header());
    brand.setColour (juce::Label::textColourId, Colors::neonPink);
    brand.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (brand);

    tagline.setText ("Wavetable synthesizer with analog warmth.", juce::dontSendNotification);
    tagline.setFont (Fonts::label());
    tagline.setColour (juce::Label::textColourId, Colors::neonCyan);
    tagline.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (tagline);

    copy.setText ("v0.4   /   3 wavetable osc + sub + noise   /   16-voice poly\n"
                  "ADSR amp/mod, pitch env, 2 LFOs, 4 macros\n"
                  "Distortion, Chorus, Phaser, EQ, Delay, Reverb, Comp\n"
                  "Grit / Vibe / Drift / Sat   -   user presets supported\n"
                  "Built with JUCE.   /   RocketBombs",
                  juce::dontSendNotification);
    copy.setFont (Fonts::value());
    copy.setColour (juce::Label::textColourId, Colors::textDim);
    copy.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (copy);
}

void MasterPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (10);
    const int leftW = (int) (r.getWidth() * 0.42);
    juce::Rectangle<int> sLeft  (r.getX(),                  r.getY(), leftW - 5,             r.getHeight());
    juce::Rectangle<int> sRight (r.getX() + leftW + 5,      r.getY(), r.getWidth() - leftW - 5, r.getHeight());
    drawSectionBg (g, sLeft,  Colors::neonPink, "PRESETS");
    drawSectionBg (g, sRight, Colors::neonCyan, "MASTER  /  ABOUT");
}

void MasterPage::resized()
{
    auto r = getLocalBounds().reduced (10);
    const int leftW = (int) (r.getWidth() * 0.42);
    juce::Rectangle<int> sLeft  (r.getX(),                  r.getY(), leftW - 5,             r.getHeight());
    juce::Rectangle<int> sRight (r.getX() + leftW + 5,      r.getY(), r.getWidth() - leftW - 5, r.getHeight());

    // Left: presets + name field + save/rename/delete
    {
        auto a = sLeft; a.removeFromTop (kSectionTitleH); a.reduce (16, 12);

        presetLabel.setBounds (a.removeFromTop (18));
        a.removeFromTop (4);
        presetNowLabel.setBounds (a.removeFromTop (40));
        a.removeFromTop (6);

        auto btnRow = a.removeFromTop (32);
        prevBtn.setBounds (btnRow.removeFromLeft (110));
        btnRow.removeFromLeft (8);
        nextBtn.setBounds (btnRow.removeFromLeft (110));
        a.removeFromTop (8);

        // Reserve space for name + save/rename/delete row at the bottom (so list grows to fill).
        const int controlsH = 24 /*name label*/ + 32 /*field*/ + 8 + 32 /*buttons*/;
        auto bottom = a.removeFromBottom (controlsH);

        presetList.setBounds (a);

        nameLabel.setBounds (bottom.removeFromTop (24));
        nameField.setBounds (bottom.removeFromTop (32));
        bottom.removeFromTop (8);
        auto br = bottom.removeFromTop (32);
        const int bw = (br.getWidth() - 16) / 3;
        saveBtn  .setBounds (br.removeFromLeft (bw)); br.removeFromLeft (8);
        renameBtn.setBounds (br.removeFromLeft (bw)); br.removeFromLeft (8);
        deleteBtn.setBounds (br.removeFromLeft (bw));
    }

    // Right: master + about
    {
        auto a = sRight; a.removeFromTop (kSectionTitleH); a.reduce (16, 12);
        brand.setBounds (a.removeFromTop (40));
        tagline.setBounds (a.removeFromTop (22));
        a.removeFromTop (10);

        auto knobRow = a.removeFromTop (140);
        const int kw = juce::jmin (170, knobRow.getWidth() / 2);
        gain ->setBounds (knobRow.removeFromLeft (kw));
        knobRow.removeFromLeft (8);
        width->setBounds (knobRow.removeFromLeft (kw));

        a.removeFromTop (16);
        copy.setBounds (a);
    }
}

// =====================================================================
// Editor
// =====================================================================

VaporKeyAudioProcessorEditor::VaporKeyAudioProcessorEditor (VaporKeyAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    tabs.setTabBarDepth (40);
    tabs.setOutline (0);
    tabs.addTab ("OSCILLATORS",   Colors::panel, new OscPage (proc),       true);
    tabs.addTab ("FILTER & ENV",  Colors::panel, new FilterEnvPage (proc), true);
    tabs.addTab ("MOD",           Colors::panel, new ModPage (proc),       true);
    tabs.addTab ("FX",            Colors::panel, new FxPage (proc),        true);
    tabs.addTab ("MASTER",        Colors::panel, new MasterPage (proc),    true);

    tabs.setColour (juce::TabbedComponent::backgroundColourId, juce::Colours::transparentBlack);
    tabs.setColour (juce::TabbedComponent::outlineColourId,    juce::Colours::transparentBlack);
    addAndMakeVisible (tabs);

    setResizable (true, true);
    setResizeLimits (1100, 680, 1920, 1200);
    setSize (1280, 800);
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
        const float cy = r.getY() + 64.0f;
        const float rad = 64.0f;
        juce::ColourGradient sun (juce::Colour (0xffffd166), cx, cy - rad,
                                  juce::Colour (0xffff2ec4), cx, cy + rad, false);
        g.setGradientFill (sun);
        g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
        g.setColour (juce::Colour (0xff05010f));
        for (int i = 0; i < 6; ++i)
            g.fillRect (cx - rad, cy + (float) i * 6.0f - 6.0f, rad * 2.0f, 2.5f);
    }

    // Title
    g.setColour (Colors::neonCyan);
    g.setFont (juce::Font (juce::FontOptions (38.0f).withStyle ("Bold")));
    g.drawText ("VAPOR", 28, 14, 220, 46, juce::Justification::left);
    g.setColour (Colors::neonPink);
    g.drawText ("KEY",   158, 14, 220, 46, juce::Justification::left);

    g.setColour (Colors::textDim);
    g.setFont (Fonts::small());
    g.drawText ("WAVETABLE  /  SYNTHESIZER  /  v0.3", 28, 56, 360, 14, juce::Justification::left);

    // Top-right neon line
    g.setColour (Colors::neonPink.withAlpha (0.6f));
    g.drawHorizontalLine (72, (float) getWidth() - 220.0f, (float) getWidth() - 28.0f);
    g.setColour (Colors::neonCyan.withAlpha (0.6f));
    g.drawHorizontalLine (76, (float) getWidth() - 200.0f, (float) getWidth() - 28.0f);
}

void VaporKeyAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (76); // header
    tabs.setBounds (r.reduced (8));
}
