#include "PluginEditor.h"
#include "Pages/OscPage.h"
#include "Pages/FilterEnvPage.h"
#include "Pages/ModPage.h"
#include "Pages/ArpPage.h"
#include "Pages/FxPage.h"
#include "Pages/MasterPage.h"

using namespace VK;

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

    juce::ColourGradient sun (juce::Colour (0xffffd166), cx, cy - baseRad,
                              juce::Colour (0xffff2ec4), cx, cy + baseRad, false);
    g.setGradientFill (sun);
    g.fillEllipse (cx - baseRad, cy - baseRad, baseRad * 2.0f, baseRad * 2.0f);

    // Horizontal slats (synthwave sun)
    g.setColour (juce::Colour (0xff05010f));
    for (int i = 0; i < 6; ++i)
        g.fillRect (cx - baseRad, cy + (float) i * 6.0f - 6.0f, baseRad * 2.0f, 2.5f);

    g.setColour (juce::Colour (0xffffd166).withAlpha (0.6f + 0.4f * sunPulse));
    g.drawEllipse (cx - baseRad, cy - baseRad, baseRad * 2.0f, baseRad * 2.0f, 1.4f);
}

namespace {
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
}

void VaporKeyAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawVaporBackdrop (g, r);
    drawStars (g, r);
    drawSun   (g, r);

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

    auto meterR = juce::Rectangle<int> (header.getWidth() - 156, 30,
                                        128, 28);
    if (headerMeter) headerMeter->setBounds (meterR);

    tabs.setBounds (r.reduced (8));
}
