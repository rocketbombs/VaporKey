#include "PluginEditor.h"
#include "Presets.h"

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
// LevelMeter
// =====================================================================

LevelMeter::LevelMeter (VaporKeyAudioProcessor& p) : processor (p)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (45);
}

void LevelMeter::timerCallback()
{
    // Pull the latest peak snapshot, decay the visible level smoothly, and
    // hold the falling peak marker for a moment so transients are visible.
    const float pL = juce::jmin (1.0f, processor.vis.peakL.load (std::memory_order_relaxed));
    const float pR = juce::jmin (1.0f, processor.vis.peakR.load (std::memory_order_relaxed));

    constexpr float decay = 0.78f;
    lvlL = juce::jmax (pL, lvlL * decay);
    lvlR = juce::jmax (pR, lvlR * decay);

    if (pL >= peakL) { peakL = pL; peakHoldL = 28; }
    else if (--peakHoldL <= 0) { peakL = juce::jmax (0.0f, peakL - 0.012f); peakHoldL = 0; }

    if (pR >= peakR) { peakR = pR; peakHoldR = 28; }
    else if (--peakHoldR <= 0) { peakR = juce::jmax (0.0f, peakR - 0.012f); peakHoldR = 0; }

    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    if (r.getWidth() < 6.0f || r.getHeight() < 6.0f) return;

    const bool vertical = r.getHeight() > r.getWidth();
    const float gap = 2.0f;
    auto barL = vertical ? r.removeFromLeft (r.getWidth() * 0.5f - gap * 0.5f)
                         : r.removeFromTop  (r.getHeight() * 0.5f - gap * 0.5f);
    if (vertical) r.removeFromLeft (gap); else r.removeFromTop (gap);
    auto barR = r;

    auto drawBar = [&] (juce::Rectangle<float> br, float lvl, float peak)
    {
        // Backing
        g.setColour (Colors::panel.darker (0.4f).withAlpha (0.85f));
        g.fillRoundedRectangle (br, 2.5f);
        g.setColour (Colors::neonCyan.withAlpha (0.25f));
        g.drawRoundedRectangle (br, 2.5f, 1.0f);

        // Logarithmic-ish shaping so 0.5 looks meaningful.
        auto curveLvl  = std::pow (juce::jlimit (0.0f, 1.0f, lvl),  0.55f);
        auto curvePeak = std::pow (juce::jlimit (0.0f, 1.0f, peak), 0.55f);

        if (vertical)
        {
            const float h = br.getHeight() * curveLvl;
            auto fillR = br.withTop (br.getBottom() - h);
            juce::ColourGradient grad (Colors::neonGreen, fillR.getX(), fillR.getBottom(),
                                       Colors::neonPink,  fillR.getX(), br.getY(), false);
            grad.addColour (0.55, Colors::neonAmber);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fillR.reduced (1.0f), 2.0f);

            if (peak > 0.005f)
            {
                const float py = br.getBottom() - br.getHeight() * curvePeak;
                g.setColour (peak > 0.99f ? Colors::neonPink : Colors::textBright);
                g.fillRect (br.getX() + 1.0f, py - 1.5f, br.getWidth() - 2.0f, 2.0f);
            }
        }
        else
        {
            const float w = br.getWidth() * curveLvl;
            auto fillR = br.withWidth (w);
            juce::ColourGradient grad (Colors::neonGreen, br.getX(),    br.getY(),
                                       Colors::neonPink,  br.getRight(), br.getY(), false);
            grad.addColour (0.55, Colors::neonAmber);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fillR.reduced (1.0f), 2.0f);

            if (peak > 0.005f)
            {
                const float px = br.getX() + br.getWidth() * curvePeak;
                g.setColour (peak > 0.99f ? Colors::neonPink : Colors::textBright);
                g.fillRect (px - 1.0f, br.getY() + 1.0f, 2.0f, br.getHeight() - 2.0f);
            }
        }
    };

    drawBar (barL, lvlL, peakL);
    drawBar (barR, lvlR, peakR);
}

// =====================================================================
// Scope
// =====================================================================

Scope::Scope (VaporKeyAudioProcessor& p) : processor (p)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void Scope::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    if (r.getWidth() < 4.0f || r.getHeight() < 4.0f) return;

    juce::ColourGradient bg (Colors::bg.darker (0.4f),  0.0f, r.getY(),
                             Colors::panel.darker (0.2f), 0.0f, r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 5.0f);

    // Subtle grid
    g.setColour (Colors::neonCyan.withAlpha (0.08f));
    for (int i = 1; i < 4; ++i)
    {
        const float y = r.getY() + r.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine ((int) y, r.getX() + 4.0f, r.getRight() - 4.0f);
    }
    for (int i = 1; i < 8; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / 8.0f;
        g.drawVerticalLine ((int) x, r.getY() + 4.0f, r.getBottom() - 4.0f);
    }

    // Snapshot the scope buffer ending at the most recent write index.
    constexpr int kView = 768;
    const auto write = processor.vis.scopeWrite.load (std::memory_order_acquire);
    const auto& buf  = processor.vis.scope;

    juce::Path wave;
    const float midY = r.getCentreY();
    const float scaleY = r.getHeight() * 0.42f;
    const float pad = 4.0f;
    const float w = r.getWidth() - pad * 2.0f;

    for (int i = 0; i < kView; ++i)
    {
        const auto idx = (write - (uint32_t) (kView - i)) & VisData::kScopeMask;
        const float v = juce::jlimit (-1.5f, 1.5f, buf[idx]);
        const float x = r.getX() + pad + w * (float) i / (float) (kView - 1);
        const float y = midY - v * scaleY;
        if (i == 0) wave.startNewSubPath (x, y);
        else        wave.lineTo (x, y);
    }

    // Outer glow
    for (int i = 4; i > 0; --i)
    {
        g.setColour (Colors::neonPink.withAlpha (0.10f * (float) i));
        g.strokePath (wave, juce::PathStrokeType ((float) i + 0.6f, juce::PathStrokeType::curved));
    }
    g.setColour (Colors::neonCyan);
    g.strokePath (wave, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved));

    g.setColour (Colors::neonPink.withAlpha (0.45f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

// =====================================================================
// EqCurve
// =====================================================================
// Frequency axis is log10 over 20..20000 Hz. dB axis is linear over -18..+18.
// Hit-detection radius around each node is 18 px.

namespace {
    constexpr float kEqMinHz = 20.0f;
    constexpr float kEqMaxHz = 20000.0f;
    constexpr float kEqDbRange = 18.0f;
    constexpr float kEqLowFc  = 200.0f;
    constexpr float kEqHighFc = 5000.0f;
    constexpr double kEqVisSr = 48000.0;
    constexpr float kNodeRadius = 9.0f;
    constexpr float kHitRadius  = 18.0f;
}

EqCurve::EqCurve (juce::AudioProcessorValueTreeState& s) : apvts (s)
{
    startTimerHz (30);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> EqCurve::plotArea() const
{
    return getLocalBounds().toFloat().reduced (12.0f, 14.0f);
}

float EqCurve::xForFreq (float hz, juce::Rectangle<float> r) const
{
    const float t = (std::log10 (juce::jlimit (kEqMinHz, kEqMaxHz, hz)) - std::log10 (kEqMinHz))
                    / (std::log10 (kEqMaxHz) - std::log10 (kEqMinHz));
    return r.getX() + r.getWidth() * t;
}
float EqCurve::freqForX (float x, juce::Rectangle<float> r) const
{
    const float t = juce::jlimit (0.0f, 1.0f, (x - r.getX()) / juce::jmax (1.0f, r.getWidth()));
    const float lg = std::log10 (kEqMinHz) + t * (std::log10 (kEqMaxHz) - std::log10 (kEqMinHz));
    return std::pow (10.0f, lg);
}
float EqCurve::yForDb (float db, juce::Rectangle<float> r) const
{
    const float t = juce::jlimit (-1.0f, 1.0f, db / kEqDbRange);
    return r.getCentreY() - t * r.getHeight() * 0.5f;
}
float EqCurve::dbForY (float y, juce::Rectangle<float> r) const
{
    const float t = juce::jlimit (-1.0f, 1.0f, (r.getCentreY() - y) / (r.getHeight() * 0.5f));
    return t * kEqDbRange;
}

float EqCurve::currentLowG()  const { auto* p = apvts.getRawParameterValue ("eq_low");      return p ? p->load() : 0.0f; }
float EqCurve::currentMidG()  const { auto* p = apvts.getRawParameterValue ("eq_mid");      return p ? p->load() : 0.0f; }
float EqCurve::currentMidF()  const { auto* p = apvts.getRawParameterValue ("eq_mid_freq"); return p ? p->load() : 1000.0f; }
float EqCurve::currentHighG() const { auto* p = apvts.getRawParameterValue ("eq_high");     return p ? p->load() : 0.0f; }

juce::Point<float> EqCurve::nodePos (Node n) const
{
    auto r = plotArea();
    switch (n)
    {
        case NodeLow:  return { xForFreq (kEqLowFc,        r), yForDb (currentLowG(),  r) };
        case NodeMid:  return { xForFreq (currentMidF(),   r), yForDb (currentMidG(),  r) };
        case NodeHigh: return { xForFreq (kEqHighFc,       r), yForDb (currentHighG(), r) };
        case NodeNone: break;
    }
    return {};
}

EqCurve::Node EqCurve::hitTest (juce::Point<float> p) const
{
    Node best = NodeNone;
    float bestD = kHitRadius;
    for (Node n : { NodeLow, NodeMid, NodeHigh })
    {
        const float d = nodePos (n).getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = n; }
    }
    return best;
}

void EqCurve::beginGesture (const juce::String& id)
{
    if (auto* p = apvts.getParameter (id))
        p->beginChangeGesture();
}
void EqCurve::endGesture (const juce::String& id)
{
    if (auto* p = apvts.getParameter (id))
        p->endChangeGesture();
}
void EqCurve::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        const auto& range = p->getNormalisableRange();
        const float norm = range.convertTo0to1 (juce::jlimit (range.start, range.end, value));
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
    }
}

void EqCurve::resetNode (Node n)
{
    auto reset = [&] (const juce::String& id) {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
    };
    if (n == NodeLow)  reset ("eq_low");
    if (n == NodeHigh) reset ("eq_high");
    if (n == NodeMid)  { reset ("eq_mid"); reset ("eq_mid_freq"); }
}

void EqCurve::mouseDown (const juce::MouseEvent& e)
{
    auto p = e.position;
    auto n = hitTest (p);
    if (e.mods.isPopupMenu()) { if (n != NodeNone) resetNode (n); return; }

    if (n == NodeNone)
    {
        // Click in empty space picks the closest band by frequency.
        const float fx = freqForX (p.x, plotArea());
        n = (fx < 700.0f)   ? NodeLow
          : (fx < 2500.0f)  ? NodeMid
          : NodeHigh;
    }

    dragging = n;
    if (n == NodeLow)  beginGesture ("eq_low");
    if (n == NodeHigh) beginGesture ("eq_high");
    if (n == NodeMid)
    {
        if (auto* a = apvts.getParameter ("eq_mid"))      a->beginChangeGesture();
        if (auto* a = apvts.getParameter ("eq_mid_freq")) a->beginChangeGesture();
    }
    mouseDrag (e);
}

void EqCurve::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == NodeNone) return;
    auto r = plotArea();
    const float db = dbForY (e.position.y, r);
    if (dragging == NodeLow)  setParam ("eq_low",  db);
    if (dragging == NodeHigh) setParam ("eq_high", db);
    if (dragging == NodeMid)
    {
        setParam ("eq_mid",      db);
        setParam ("eq_mid_freq", freqForX (e.position.x, r));
    }
}

void EqCurve::mouseUp (const juce::MouseEvent&)
{
    if (dragging == NodeLow)  endGesture ("eq_low");
    if (dragging == NodeHigh) endGesture ("eq_high");
    if (dragging == NodeMid)
    {
        if (auto* a = apvts.getParameter ("eq_mid"))      a->endChangeGesture();
        if (auto* a = apvts.getParameter ("eq_mid_freq")) a->endChangeGesture();
    }
    dragging = NodeNone;
}

void EqCurve::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto n = hitTest (e.position);
    if (n != NodeNone) { resetNode (n); return; }
    resetNode (NodeLow); resetNode (NodeMid); resetNode (NodeHigh);
}

void EqCurve::paint (juce::Graphics& g)
{
    auto outer = getLocalBounds().toFloat();
    auto r = plotArea();

    // Background
    juce::ColourGradient bg (Colors::bg.darker (0.3f), 0.0f, r.getY(),
                             Colors::panel.darker (0.1f), 0.0f, r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 6.0f);

    // dB grid
    g.setColour (Colors::neonCyan.withAlpha (0.10f));
    for (int db : { -12, -6, 6, 12 })
    {
        const float y = yForDb ((float) db, r);
        g.drawHorizontalLine ((int) y, r.getX() + 4.0f, r.getRight() - 4.0f);
    }
    g.setColour (Colors::neonCyan.withAlpha (0.22f));
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 4.0f, r.getRight() - 4.0f);

    // Frequency grid (octaves)
    const float labelHzs[] = { 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2500.0f, 5000.0f, 10000.0f };
    g.setColour (Colors::neonCyan.withAlpha (0.10f));
    for (float hz : labelHzs)
    {
        const float x = xForFreq (hz, r);
        g.drawVerticalLine ((int) x, r.getY() + 4.0f, r.getBottom() - 4.0f);
    }

    // Frequency labels
    g.setColour (Colors::textDim);
    g.setFont (Fonts::small());
    for (float hz : { 100.0f, 1000.0f, 10000.0f })
    {
        const auto label = hz >= 1000.0f
            ? juce::String (juce::roundToInt (hz / 1000.0f)) + "k"
            : juce::String (juce::roundToInt (hz));
        const float x = xForFreq (hz, r);
        g.drawText (label, (int) (x - 18.0f), (int) outer.getBottom() - 14, 36, 12,
                    juce::Justification::centred);
    }

    // Compute composite magnitude curve via real IIR coefficients.
    using Coef = juce::dsp::IIR::Coefficients<float>;
    const float lowG  = currentLowG();
    const float midG  = currentMidG();
    const float midF  = currentMidF();
    const float highG = currentHighG();

    auto cLow  = Coef::makeLowShelf  (kEqVisSr, kEqLowFc,  0.707f, juce::Decibels::decibelsToGain (lowG));
    auto cMid  = Coef::makePeakFilter (kEqVisSr, midF,      0.8f,   juce::Decibels::decibelsToGain (midG));
    auto cHigh = Coef::makeHighShelf (kEqVisSr, kEqHighFc, 0.707f, juce::Decibels::decibelsToGain (highG));

    juce::Path curve;
    juce::Path fill;
    const int N = juce::jmax (32, (int) r.getWidth());
    for (int i = 0; i < N; ++i)
    {
        const float t = (float) i / (float) (N - 1);
        const float x = r.getX() + r.getWidth() * t;
        const float hz = freqForX (x, r);
        const double mag = cLow ->getMagnitudeForFrequency ((double) hz, kEqVisSr)
                         * cMid ->getMagnitudeForFrequency ((double) hz, kEqVisSr)
                         * cHigh->getMagnitudeForFrequency ((double) hz, kEqVisSr);
        const float db = juce::Decibels::gainToDecibels ((float) mag, -60.0f);
        const float y = yForDb (db, r);
        if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, r.getCentreY()); fill.lineTo (x, y); }
        else        { curve.lineTo (x, y); fill.lineTo (x, y); }
    }
    fill.lineTo (r.getRight(), r.getCentreY());
    fill.closeSubPath();

    // Filled glow under curve
    juce::ColourGradient cg (Colors::neonCyan.withAlpha (0.30f), r.getX(), r.getCentreY(),
                             Colors::neonPink.withAlpha (0.08f), r.getX(), r.getY(), false);
    g.setGradientFill (cg);
    g.fillPath (fill);

    // Outer glow stroke
    for (int i = 4; i > 0; --i)
    {
        g.setColour (Colors::neonCyan.withAlpha (0.10f * (float) i));
        g.strokePath (curve, juce::PathStrokeType ((float) i + 0.5f, juce::PathStrokeType::curved));
    }
    g.setColour (Colors::neonCyan);
    g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));

    // Outline
    g.setColour (Colors::neonPink.withAlpha (0.45f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    // Nodes
    auto drawNode = [&] (Node n, juce::Colour col, const juce::String& letter)
    {
        const auto pt = nodePos (n);
        const bool isDrag = (dragging == n);
        const float rad = isDrag ? kNodeRadius + 2.0f : kNodeRadius;

        // Glow
        for (int i = 4; i > 0; --i)
            g.setColour (col.withAlpha ((isDrag ? 0.16f : 0.08f) * (float) i)),
            g.fillEllipse (pt.x - rad - (float) i, pt.y - rad - (float) i,
                           (rad + (float) i) * 2.0f, (rad + (float) i) * 2.0f);

        // Body
        juce::ColourGradient grad (col.brighter (0.4f), pt.x, pt.y - rad,
                                   col.darker (0.3f),   pt.x, pt.y + rad, false);
        g.setGradientFill (grad);
        g.fillEllipse (pt.x - rad, pt.y - rad, rad * 2.0f, rad * 2.0f);

        g.setColour (Colors::textBright);
        g.setFont (Fonts::small());
        g.drawText (letter, (int) (pt.x - 8.0f), (int) (pt.y - 7.0f), 16, 14,
                    juce::Justification::centred);
    };
    drawNode (NodeLow,  Colors::neonGreen,  "L");
    drawNode (NodeMid,  Colors::neonAmber,  "M");
    drawNode (NodeHigh, Colors::neonPink,   "H");

    // Mini readout in the top-right corner
    g.setColour (Colors::textDim);
    g.setFont (Fonts::small());
    auto fmtDb = [] (float v) { return (v >= 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
    auto fmtHz = [] (float v) { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz"
                                                    : juce::String (juce::roundToInt (v)) + " Hz"; };
    juce::String head = "L " + fmtDb (lowG) + "   M " + fmtDb (midG) + " @ " + fmtHz (midF) + "   H " + fmtDb (highG);
    g.drawText (head, (int) r.getX() + 6, (int) r.getY() + 4, (int) r.getWidth() - 12, 14,
                juce::Justification::right);
}

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
    // Keep keyboard focus on the editor itself so the host (and any computer-
    // keyboard MIDI input it routes through us) keeps receiving key events
    // after the user tweaks a knob, instead of having to click the header to
    // hand focus back.
    slider.setWantsKeyboardFocus (false);
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

WavetableDisplay::WavetableDisplay (VaporKeyAudioProcessor& proc, int oscIndex)
    : processor (proc), apvts (proc.apvts), idx (oscIndex)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setOpaque (false);
    startTimerHz (24);
}

bool WavetableDisplay::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".wav")) return true;
    return false;
}

void WavetableDisplay::fileDragEnter (const juce::StringArray&, int, int)
{
    dragHover = true;
    repaint();
}

void WavetableDisplay::fileDragExit (const juce::StringArray&)
{
    dragHover = false;
    repaint();
}

void WavetableDisplay::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    for (const auto& f : files)
    {
        if (! f.endsWithIgnoreCase (".wav")) continue;
        if (processor.loadCustomWavetable (idx, juce::File (f)))
        {
            // Switch the oscillator's shape to Custom so the user hears the file.
            if (auto* p = apvts.getParameter ("osc" + juce::String (idx + 1) + "_shape"))
            {
                const float norm = p->getNormalisableRange().convertTo0to1 ((float) WavetableLibrary::Custom);
                p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
            }
            break;
        }
    }
    repaint();
}

void WavetableDisplay::chooseWavFile()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Choose a .wav file for OSC " + juce::String (idx + 1),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav");

    const int chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File()) return;
        if (processor.loadCustomWavetable (idx, file))
        {
            if (auto* p = apvts.getParameter ("osc" + juce::String (idx + 1) + "_shape"))
            {
                const float norm = p->getNormalisableRange().convertTo0to1 ((float) WavetableLibrary::Custom);
                p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
            }
        }
    });
}

void WavetableDisplay::showLoadMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Load .wav file...");
    m.addItem (2, "Clear custom wavetable",
               processor.getCustomWavetableName (idx).isNotEmpty());

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [this] (int r)
                     {
                         if (r == 1) chooseWavFile();
                         else if (r == 2) processor.clearCustomWavetable (idx);
                     });
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

    // For Custom shape, paint from the user-loaded table (atomic snapshot) and
    // fall back to Basic if no file has been loaded yet.
    std::shared_ptr<Wavetable> customSnap;
    const Wavetable* wtPtr = nullptr;
    if (shape == WavetableLibrary::Custom)
    {
        customSnap = std::atomic_load (&processor.synthParams.customTables[idx]);
        wtPtr = customSnap ? customSnap.get()
                           : &WavetableLibrary::get().getTable (WavetableLibrary::Basic);
    }
    else
    {
        wtPtr = &WavetableLibrary::get().getTable (shape);
    }
    const auto& wt = *wtPtr;

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
    juce::String label = juce::String (WavetableLibrary::shapeName (shape)).toUpperCase();
    if (shape == WavetableLibrary::Custom)
    {
        const auto custom = processor.getCustomWavetableName (idx);
        label = custom.isNotEmpty() ? ("CUSTOM  /  " + custom.toUpperCase()) : "CUSTOM  /  (DROP .WAV)";
    }
    g.drawText (label,
                (int) r.getX() + 10, (int) r.getY() + 6, (int) r.getWidth() - 80, 16, juce::Justification::left);

    g.setColour (Colors::neonPink);
    g.drawText (juce::String (juce::roundToInt (pos * 100.0f)) + "%",
                (int) r.getRight() - 60, (int) r.getY() + 6, 50, 16, juce::Justification::right);

    // Hint text bottom-right
    g.setColour (Colors::textDim);
    g.setFont (Fonts::small());
    g.drawText ("DRAG TO SCRUB  /  DROP .WAV  /  RIGHT-CLICK",
                (int) r.getX() + 10, (int) r.getBottom() - 18, (int) r.getWidth() - 20, 14,
                juce::Justification::right);

    // Drag-hover highlight overlay
    if (dragHover)
    {
        g.setColour (Colors::neonGreen.withAlpha (0.20f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Colors::neonGreen);
        g.drawRoundedRectangle (r, 6.0f, 2.5f);
        g.setFont (Fonts::subheader());
        g.drawText ("DROP TO LOAD WAVETABLE", r.toNearestInt(), juce::Justification::centred);
    }

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

void WavetableDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showLoadMenu(); return; }
    setPositionFromMouse (e);
}
void WavetableDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    setPositionFromMouse (e);
}
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
// ArpPage
// =====================================================================

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

// =====================================================================
// MasterPage
// =====================================================================

int MasterPage::PresetListModel::getNumRows()
{
    return (int) owner.visibleRows.size();
}

void MasterPage::PresetListModel::paintListBoxItem (int row, juce::Graphics& g,
                                                    int width, int height, bool selected)
{
    if (row < 0 || row >= (int) owner.visibleRows.size()) return;
    const int entryIdx = owner.visibleRows[(size_t) row];
    if (entryIdx < 0 || entryIdx >= (int) owner.entries.size()) return;
    const auto& entry = owner.entries[(size_t) entryIdx];

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
    const int nameRight = entry.isFactory ? (width - 130) : (width - 56);
    g.drawText (entry.name, 36, 0, juce::jmax (40, nameRight - 36), height, juce::Justification::centredLeft);

    // Category tag on the right (factory only)
    if (entry.isFactory && entry.category >= 0 && entry.category < VKPresets::NumCategories)
    {
        g.setFont (Fonts::small());
        g.setColour (Colors::neonCyan.withAlpha (0.6f));
        g.drawText (juce::String (VKPresets::categoryShortName (entry.category)).toUpperCase(),
                    width - 120, 0, 50, height, juce::Justification::centredRight);
    }

    if (selected)
    {
        g.setColour (Colors::neonPink);
        g.setFont (Fonts::small());
        g.drawText ("PLAYING", width - 80, 0, 70, height, juce::Justification::centredRight);
    }
}

void MasterPage::PresetListModel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int) owner.visibleRows.size()) return;
    owner.loadEntry (owner.visibleRows[(size_t) row]);
}

void MasterPage::rebuildEntries()
{
    entries.clear();

    const auto& factory = VKPresets::all();
    for (const auto& p : factory)
        entries.push_back ({ juce::String (p.name), true, p.category });

    for (const auto& n : proc.getUserPresetNames())
        entries.push_back ({ n, false, -1 });

    rebuildVisible();
}

void MasterPage::rebuildVisible()
{
    visibleRows.clear();
    // categoryFilter item IDs:
    //   1            = All
    //   2..1+N       = factory category (Bass, Lead, Pad, Pluck, Keys, Bell, FX, Arp)
    //   2+N          = User
    const int sel = categoryFilter.getSelectedId();
    const int N   = (int) VKPresets::NumCategories;

    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries[i];
        bool match = false;
        if (sel <= 0 || sel == 1) match = true;
        else if (sel == 2 + N)    match = ! e.isFactory;
        else                      match = e.isFactory && e.category == (sel - 2);

        if (match) visibleRows.push_back ((int) i);
    }

    presetList.updateContent();
    presetList.repaint();
}

int MasterPage::findEntryIndex (const juce::String& name, bool isFactory) const
{
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].isFactory == isFactory && entries[i].name == name)
            return (int) i;
    return -1;
}

int MasterPage::visibleRowFromEntryIndex (int entryIndex) const
{
    for (size_t i = 0; i < visibleRows.size(); ++i)
        if (visibleRows[i] == entryIndex) return (int) i;
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
    const int row = visibleRowFromEntryIndex (idx);
    if (row >= 0) presetList.selectRow (row);
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
    if (idx >= 0)
    {
        const int row = visibleRowFromEntryIndex (idx);
        if (row >= 0) presetList.selectRow (row, false, true);
    }
}

void MasterPage::stepPreset (int dir)
{
    // Step within whatever the user is currently viewing — wraps around the
    // filtered list so navigation feels predictable per category.
    const int n = (int) visibleRows.size();
    if (n == 0) return;
    int curRow = juce::jmax (0, presetList.getSelectedRow());
    curRow = (curRow + dir + n) % n;
    loadEntry (visibleRows[(size_t) curRow]);
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
        if (idx >= 0)
        {
            const int row = visibleRowFromEntryIndex (idx);
            if (row >= 0) presetList.selectRow (row);
        }
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
    if (row < 0 || row >= (int) visibleRows.size()) { showStatus ("SELECT A PRESET", Colors::neonAmber); return; }
    const int entryIdx = visibleRows[(size_t) row];
    const auto& e = entries[(size_t) entryIdx];
    if (e.isFactory) { showStatus ("CANNOT RENAME FACTORY", Colors::neonAmber); return; }

    const auto newName = nameField.getText().trim();
    if (newName.isEmpty()) { showStatus ("ENTER A NEW NAME", Colors::neonAmber); return; }

    if (proc.renameUserPreset (e.name, newName))
    {
        rebuildEntries();
        const int idx = findEntryIndex (newName, false);
        if (idx >= 0)
        {
            const int newRow = visibleRowFromEntryIndex (idx);
            if (newRow >= 0) presetList.selectRow (newRow);
        }
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
    if (row < 0 || row >= (int) visibleRows.size()) { showStatus ("SELECT A PRESET", Colors::neonAmber); return; }
    const int entryIdx = visibleRows[(size_t) row];
    const auto& e = entries[(size_t) entryIdx];
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
    meter = std::make_unique<LevelMeter> (p);                          addAndMakeVisible (*meter);
    scope = std::make_unique<Scope>      (p);                          addAndMakeVisible (*scope);

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

    categoryLabel.setText ("CATEGORY", juce::dontSendNotification);
    categoryLabel.setFont (Fonts::section());
    categoryLabel.setColour (juce::Label::textColourId, Colors::neonCyan);
    categoryLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (categoryLabel);

    categoryFilter.addItem ("All", 1);
    {
        const auto cats = VKPresets::categoryNames();
        for (int i = 0; i < cats.size(); ++i)
            categoryFilter.addItem (cats[i], 2 + i);
        categoryFilter.addItem ("User", 2 + cats.size());
    }
    categoryFilter.setSelectedId (1, juce::dontSendNotification);
    categoryFilter.onChange = [this] { rebuildVisible(); refreshNowPlaying(); };
    addAndMakeVisible (categoryFilter);

    rebuildEntries();
    {
        const int idx = findEntryIndex (proc.currentPresetName, proc.currentPresetIsFactory);
        if (idx >= 0)
        {
            const int row = visibleRowFromEntryIndex (idx);
            if (row >= 0) presetList.selectRow (row);
        }
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

    copy.setText ("v" + juce::String (JucePlugin_VersionString) + "   /   3 wavetable osc + sub + noise   /   16-voice poly\n"
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

        auto catRow = a.removeFromTop (28);
        categoryLabel.setBounds (catRow.removeFromLeft (90));
        categoryFilter.setBounds (catRow.reduced (2, 1));
        a.removeFromTop (8);

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

        // Output scope across the top of the right column.
        scope->setBounds (a.removeFromTop (110));
        a.removeFromTop (10);

        // Master and width knobs on the left, vertical stereo meter on the right.
        auto knobRow = a.removeFromTop (140);
        auto meterArea = knobRow.removeFromRight (52);
        meter->setBounds (meterArea.reduced (4, 6));
        knobRow.removeFromRight (8);
        const int kw = juce::jmin (170, knobRow.getWidth() / 2);
        gain ->setBounds (knobRow.removeFromLeft (kw));
        knobRow.removeFromLeft (8);
        width->setBounds (knobRow.removeFromLeft (kw));

        a.removeFromTop (12);
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
    tabs.addTab ("ARP",           Colors::panel, new ArpPage (proc),       true);
    tabs.addTab ("FX",            Colors::panel, new FxPage (proc),        true);
    tabs.addTab ("MASTER",        Colors::panel, new MasterPage (proc),    true);

    tabs.setColour (juce::TabbedComponent::backgroundColourId, juce::Colours::transparentBlack);
    tabs.setColour (juce::TabbedComponent::outlineColourId,    juce::Colours::transparentBlack);
    addAndMakeVisible (tabs);

    headerMeter = std::make_unique<LevelMeter> (proc);
    addAndMakeVisible (*headerMeter);

    initStars();
    startTimerHz (45);   // backdrop animation + sun pulse

    setResizable (true, true);
    setResizeLimits (1100, 680, 1920, 1200);
    setSize (1280, 800);
}

VaporKeyAudioProcessorEditor::~VaporKeyAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void VaporKeyAudioProcessorEditor::initStars()
{
    juce::Random rng (0xCAFEF00D);
    stars.clear();
    stars.reserve (90);
    for (int i = 0; i < 90; ++i)
    {
        Star s;
        s.x01 = rng.nextFloat();
        // Concentrate in the upper "sky" portion of the editor.
        s.y01 = std::pow (rng.nextFloat(), 1.6f) * 0.55f;
        s.baseAlpha = 0.25f + 0.6f * rng.nextFloat();
        s.twinkleHz = 0.4f + 1.6f * rng.nextFloat();
        s.phase     = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        s.radius    = 0.6f + 1.4f * rng.nextFloat();
        stars.push_back (s);
    }
}

void VaporKeyAudioProcessorEditor::timerCallback()
{
    animPhase += 1.0f / 45.0f;
    if (animPhase > 1.0e6f) animPhase = 0.0f;

    // Smooth a small sun pulse from the live RMS so the sun gently breathes
    // with the audio.
    const float rms = juce::jlimit (0.0f, 1.0f, proc.vis.rms.load (std::memory_order_relaxed));
    const float target = std::pow (rms, 0.6f);
    sunPulse += (target - sunPulse) * 0.12f;

    repaint();
}

void VaporKeyAudioProcessorEditor::drawStars (juce::Graphics& g, juce::Rectangle<float> r)
{
    for (const auto& s : stars)
    {
        const float a = juce::jlimit (0.0f, 1.0f,
            s.baseAlpha * (0.6f + 0.4f * std::sin (animPhase * s.twinkleHz * juce::MathConstants<float>::twoPi + s.phase)));
        g.setColour (juce::Colours::white.withAlpha (a));
        const float x = r.getX() + r.getWidth() * s.x01;
        const float y = r.getY() + r.getHeight() * s.y01;
        g.fillEllipse (x - s.radius, y - s.radius, s.radius * 2.0f, s.radius * 2.0f);
    }
}

void VaporKeyAudioProcessorEditor::drawSun (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float cx = r.getCentreX();
    const float cy = r.getY() + 64.0f;
    const float baseRad = 64.0f;
    const float pulse = sunPulse * 0.5f;

    // Outer atmospheric halo - reacts to RMS.
    for (int i = 8; i > 0; --i)
    {
        const float k = (float) i;
        const float rr = baseRad + k * 6.0f + pulse * 26.0f;
        g.setColour (juce::Colour (0xffff2ec4).withAlpha (0.025f * k * (0.6f + 0.7f * sunPulse)));
        g.fillEllipse (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f);
    }

    // Sun body: warm-to-pink radial gradient
    juce::ColourGradient sun (juce::Colour (0xffffd166), cx, cy - baseRad,
                              juce::Colour (0xffff2ec4), cx, cy + baseRad, false);
    g.setGradientFill (sun);
    g.fillEllipse (cx - baseRad, cy - baseRad, baseRad * 2.0f, baseRad * 2.0f);

    // Horizontal slats (synthwave sun)
    g.setColour (juce::Colour (0xff05010f));
    for (int i = 0; i < 6; ++i)
        g.fillRect (cx - baseRad, cy + (float) i * 6.0f - 6.0f, baseRad * 2.0f, 2.5f);

    // Reactive bright rim
    g.setColour (juce::Colour (0xffffd166).withAlpha (0.6f + 0.4f * sunPulse));
    g.drawEllipse (cx - baseRad, cy - baseRad, baseRad * 2.0f, baseRad * 2.0f, 1.4f);
}

void VaporKeyAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawVaporBackdrop (g, r);
    drawStars (g, r);
    drawSun   (g, r);

    // Title with subtle outer glow
    auto drawWithGlow = [&] (const juce::String& s, int x, int y, int w, int h, juce::Colour col)
    {
        for (int i = 3; i > 0; --i)
        {
            g.setColour (col.withAlpha (0.10f * (float) i));
            g.drawText (s, x - i, y - i, w, h, juce::Justification::left);
            g.drawText (s, x + i, y + i, w, h, juce::Justification::left);
        }
        g.setColour (col);
        g.drawText (s, x, y, w, h, juce::Justification::left);
    };

    g.setFont (juce::Font (juce::FontOptions (38.0f).withStyle ("Bold")));
    drawWithGlow ("VAPOR", 28, 14, 220, 46, Colors::neonCyan);
    drawWithGlow ("KEY",   158, 14, 220, 46, Colors::neonPink);

    g.setColour (Colors::textDim);
    g.setFont (Fonts::small());
    g.drawText ("WAVETABLE  /  SYNTHESIZER  /  v" + juce::String (JucePlugin_VersionString),
                28, 56, 360, 14, juce::Justification::left);

    // Top-right neon lines + "OUTPUT" tag above the meter
    g.setColour (Colors::neonPink.withAlpha (0.6f));
    g.drawHorizontalLine (72, (float) getWidth() - 220.0f, (float) getWidth() - 28.0f);
    g.setColour (Colors::neonCyan.withAlpha (0.6f));
    g.drawHorizontalLine (76, (float) getWidth() - 200.0f, (float) getWidth() - 28.0f);

    g.setColour (Colors::textDim);
    g.setFont (Fonts::small());
    g.drawText ("OUTPUT", getWidth() - 220, 16, 60, 12, juce::Justification::left);
}

void VaporKeyAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (76);

    // Stereo meter strip in the top-right of the header.
    auto meterR = juce::Rectangle<int> (header.getWidth() - 156, 30,
                                        128, 28);
    if (headerMeter) headerMeter->setBounds (meterR);

    tabs.setBounds (r.reduced (8));
}
