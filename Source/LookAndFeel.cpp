#include "LookAndFeel.h"

using namespace VK;

VaporLookAndFeel::VaporLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Colors::bg);
    setColour (juce::Slider::textBoxTextColourId,         Colors::text);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,                 Colors::text);
    setColour (juce::ComboBox::backgroundColourId,        Colors::panelHi);
    setColour (juce::ComboBox::textColourId,              Colors::neonCyan);
    setColour (juce::ComboBox::outlineColourId,           Colors::neonPink.withAlpha (0.6f));
    setColour (juce::ComboBox::arrowColourId,             Colors::neonPink);
    setColour (juce::PopupMenu::backgroundColourId,       Colors::panel);
    setColour (juce::PopupMenu::textColourId,             Colors::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colors::neonPink.withAlpha (0.4f));
    setColour (juce::PopupMenu::highlightedTextColourId,  juce::Colours::white);
    setColour (juce::TextButton::buttonColourId,          Colors::panelHi);
    setColour (juce::TextButton::buttonOnColourId,        Colors::neonPink.withAlpha (0.55f));
    setColour (juce::TextButton::textColourOnId,          juce::Colours::white);
    setColour (juce::TextButton::textColourOffId,         Colors::textDim);
}

static void glowEllipse (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float strength)
{
    for (int i = 4; i >= 1; --i)
    {
        const float exp = (float) i * 1.6f;
        g.setColour (c.withAlpha (0.08f * strength));
        g.drawEllipse (r.expanded (exp), 1.0f);
    }
}

void VaporLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                         float sliderPos, float rotaryStart, float rotaryEnd,
                                         juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float angle = rotaryStart + sliderPos * (rotaryEnd - rotaryStart);

    // Outer ring (track)
    g.setColour (Colors::panelHi);
    g.fillEllipse (cx - radius, cy - radius, radius * 2, radius * 2);

    // Track arc
    juce::Path track;
    track.addCentredArc (cx, cy, radius - 2, radius - 2, 0.0f, rotaryStart, rotaryEnd, true);
    g.setColour (Colors::grid);
    g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc with neon glow
    const auto neon = (s.getName().containsIgnoreCase ("Grit")  ? Colors::neonAmber
                    :  s.getName().containsIgnoreCase ("Vibe")  ? Colors::neonPurple
                    :  s.getName().containsIgnoreCase ("Drift") ? Colors::neonCyan
                    :  s.getName().containsIgnoreCase ("Sat")   ? Colors::neonAmber
                    :  Colors::neonPink);

    juce::Path val;
    val.addCentredArc (cx, cy, radius - 2, radius - 2, 0.0f, rotaryStart, angle, true);
    for (int i = 4; i > 0; --i)
    {
        g.setColour (neon.withAlpha (0.10f * (float) i));
        g.strokePath (val, juce::PathStrokeType ((float) i * 2.0f + 2.0f,
                       juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (neon);
    g.strokePath (val, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Inner disc
    const float inner = radius * 0.62f;
    juce::ColourGradient grad (Colors::panelHi, cx, cy - inner,
                               Colors::panel,   cx, cy + inner, false);
    g.setGradientFill (grad);
    g.fillEllipse (cx - inner, cy - inner, inner * 2, inner * 2);

    // Pointer
    juce::Path pointer;
    const float pl = inner * 0.85f;
    pointer.addRoundedRectangle (-1.5f, -pl, 3.0f, pl * 0.55f, 1.5f);
    g.setColour (neon);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (cx, cy));

    glowEllipse (g, juce::Rectangle<float> (cx - radius, cy - radius, radius * 2, radius * 2), neon, 1.0f);
}

void VaporLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool /*isDown*/,
                                     int /*btnX*/, int /*btnY*/, int /*btnW*/, int /*btnH*/, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 4.0f, 1.2f);

    juce::Path arrow;
    const float ax = (float) w - 12.0f;
    const float ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void VaporLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                             const juce::Colour&, bool /*highlighted*/, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto base = b.getToggleState() ? Colors::neonPink.withAlpha (0.35f)
                                         : Colors::panelHi;
    g.setColour (base);
    g.fillRoundedRectangle (r, 4.0f);

    const auto outline = b.getToggleState() ? Colors::neonPink : Colors::neonCyan.withAlpha (0.5f);
    g.setColour (outline);
    g.drawRoundedRectangle (r, 4.0f, b.getToggleState() ? 1.6f : 1.0f);

    if (down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (r, 4.0f);
    }
}

void VaporLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& l)
{
    g.setColour (l.findColour (juce::Label::textColourId));
    g.setFont (getLabelFont (l));
    g.drawFittedText (l.getText(), l.getLocalBounds(), l.getJustificationType(), 1);
}

juce::Font VaporLookAndFeel::getLabelFont (juce::Label& l)
{
    return juce::Font (juce::FontOptions ((float) l.getHeight() * 0.55f).withStyle ("Bold"));
}

juce::Font VaporLookAndFeel::getComboBoxFont (juce::ComboBox& b)
{
    return juce::Font (juce::FontOptions ((float) b.getHeight() * 0.55f));
}

juce::Font VaporLookAndFeel::getPopupMenuFont()
{
    return juce::Font (juce::FontOptions (14.0f));
}
