#include "Meters.h"
#include "../PluginProcessor.h"
#include "../LookAndFeel.h"

using namespace VK;

LevelMeter::LevelMeter (VaporKeyAudioProcessor& p) : processor (p)
{
    setInterceptsMouseClicks (false, false);
    // Timer is started by visibilityChanged once the meter is actually on
    // screen; that avoids 45 Hz callbacks on every hidden-tab / closed-editor
    // instance running in the host process.
}

void LevelMeter::syncTimerToVisibility()
{
    const bool shouldRun = isShowing();
    if (shouldRun && ! timerActive)      { startTimerHz (45); timerActive = true; }
    else if (! shouldRun && timerActive) { stopTimer();       timerActive = false; }
}

void LevelMeter::timerCallback()
{
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
        g.setColour (Colors::panel.darker (0.4f).withAlpha (0.85f));
        g.fillRoundedRectangle (br, 2.5f);
        g.setColour (Colors::neonCyan.withAlpha (0.25f));
        g.drawRoundedRectangle (br, 2.5f, 1.0f);

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

Scope::Scope (VaporKeyAudioProcessor& p) : processor (p)
{
    setInterceptsMouseClicks (false, false);
    // Started lazily by visibilityChanged - see LevelMeter for rationale.
}

void Scope::syncTimerToVisibility()
{
    const bool shouldRun = isShowing();
    if (shouldRun && ! timerActive)      { startTimerHz (30); timerActive = true; }
    else if (! shouldRun && timerActive) { stopTimer();       timerActive = false; }
}

void Scope::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    if (r.getWidth() < 4.0f || r.getHeight() < 4.0f) return;

    juce::ColourGradient bg (Colors::bg.darker (0.4f),  0.0f, r.getY(),
                             Colors::panel.darker (0.2f), 0.0f, r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 5.0f);

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
