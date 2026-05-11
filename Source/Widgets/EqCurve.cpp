#include "EqCurve.h"
#include "../LookAndFeel.h"

using namespace VK;

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
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    // Timer is started lazily by visibilityChanged so a hidden FX tab doesn't
    // keep recomputing the magnitude curve.
}

void EqCurve::syncTimerToVisibility()
{
    const bool shouldRun = isShowing();
    if (shouldRun && ! isTimerRunning())  startTimerHz (30);
    else if (! shouldRun && isTimerRunning()) stopTimer();
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

    juce::ColourGradient bg (Colors::bg.darker (0.3f), 0.0f, r.getY(),
                             Colors::panel.darker (0.1f), 0.0f, r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 6.0f);

    g.setColour (Colors::neonCyan.withAlpha (0.10f));
    for (int db : { -12, -6, 6, 12 })
    {
        const float y = yForDb ((float) db, r);
        g.drawHorizontalLine ((int) y, r.getX() + 4.0f, r.getRight() - 4.0f);
    }
    g.setColour (Colors::neonCyan.withAlpha (0.22f));
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 4.0f, r.getRight() - 4.0f);

    const float labelHzs[] = { 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2500.0f, 5000.0f, 10000.0f };
    g.setColour (Colors::neonCyan.withAlpha (0.10f));
    for (float hz : labelHzs)
    {
        const float x = xForFreq (hz, r);
        g.drawVerticalLine ((int) x, r.getY() + 4.0f, r.getBottom() - 4.0f);
    }

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

    juce::ColourGradient cg (Colors::neonCyan.withAlpha (0.30f), r.getX(), r.getCentreY(),
                             Colors::neonPink.withAlpha (0.08f), r.getX(), r.getY(), false);
    g.setGradientFill (cg);
    g.fillPath (fill);

    for (int i = 4; i > 0; --i)
    {
        g.setColour (Colors::neonCyan.withAlpha (0.10f * (float) i));
        g.strokePath (curve, juce::PathStrokeType ((float) i + 0.5f, juce::PathStrokeType::curved));
    }
    g.setColour (Colors::neonCyan);
    g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));

    g.setColour (Colors::neonPink.withAlpha (0.45f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    auto drawNode = [&] (Node n, juce::Colour col, const juce::String& letter)
    {
        const auto pt = nodePos (n);
        const bool isDrag = (dragging == n);
        const float rad = isDrag ? kNodeRadius + 2.0f : kNodeRadius;

        for (int i = 4; i > 0; --i)
            g.setColour (col.withAlpha ((isDrag ? 0.16f : 0.08f) * (float) i)),
            g.fillEllipse (pt.x - rad - (float) i, pt.y - rad - (float) i,
                           (rad + (float) i) * 2.0f, (rad + (float) i) * 2.0f);

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

    g.setColour (Colors::textDim);
    g.setFont (Fonts::small());
    auto fmtDb = [] (float v) { return (v >= 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
    auto fmtHz = [] (float v) { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz"
                                                    : juce::String (juce::roundToInt (v)) + " Hz"; };
    juce::String head = "L " + fmtDb (lowG) + "   M " + fmtDb (midG) + " @ " + fmtHz (midF) + "   H " + fmtDb (highG);
    g.drawText (head, (int) r.getX() + 6, (int) r.getY() + 4, (int) r.getWidth() - 12, 14,
                juce::Justification::right);
}
