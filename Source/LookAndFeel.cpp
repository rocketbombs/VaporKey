#include "LookAndFeel.h"

using namespace VK;

VaporLookAndFeel::VaporLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Colors::bg);

    // Slider
    setColour (juce::Slider::textBoxTextColourId,         Colors::textBright);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,    Colors::neonPink.withAlpha (0.4f));

    // Label
    setColour (juce::Label::textColourId,                 Colors::text);
    setColour (juce::Label::backgroundColourId,           juce::Colours::transparentBlack);

    // Combo box
    setColour (juce::ComboBox::backgroundColourId,        Colors::panelHi2);
    setColour (juce::ComboBox::textColourId,              Colors::textBright);
    setColour (juce::ComboBox::outlineColourId,           Colors::neonCyan.withAlpha (0.7f));
    setColour (juce::ComboBox::arrowColourId,             Colors::neonPink);
    setColour (juce::ComboBox::buttonColourId,            Colors::panelHi2);
    setColour (juce::ComboBox::focusedOutlineColourId,    Colors::neonPink);

    // Popup menu
    setColour (juce::PopupMenu::backgroundColourId,       Colors::panel);
    setColour (juce::PopupMenu::textColourId,             Colors::text);
    setColour (juce::PopupMenu::headerTextColourId,       Colors::neonPink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colors::neonPink.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId,  Colors::textBright);

    // Buttons
    setColour (juce::TextButton::buttonColourId,          Colors::panelHi2);
    setColour (juce::TextButton::buttonOnColourId,        Colors::neonPink.withAlpha (0.55f));
    setColour (juce::TextButton::textColourOnId,          Colors::textBright);
    setColour (juce::TextButton::textColourOffId,         Colors::textDim);

    // Toggle
    setColour (juce::ToggleButton::textColourId,          Colors::textBright);
    setColour (juce::ToggleButton::tickColourId,          Colors::neonPink);
    setColour (juce::ToggleButton::tickDisabledColourId,  Colors::textDim);

    // Tab
    setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedButtonBar::frontOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedComponent::outlineColourId,    juce::Colours::transparentBlack);
}

// ----- Knobs -----

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

    // Outer disc
    g.setColour (Colors::panelHi);
    g.fillEllipse (cx - radius, cy - radius, radius * 2, radius * 2);

    // Track arc (background)
    juce::Path track;
    track.addCentredArc (cx, cy, radius - 3, radius - 3, 0.0f, rotaryStart, rotaryEnd, true);
    g.setColour (Colors::grid);
    g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Pick neon color based on knob name (warmth knobs use amber/purple)
    const juce::String n = s.getName();
    juce::Colour neon = Colors::neonPink;
    if (n.containsIgnoreCase ("Grit") || n.containsIgnoreCase ("Sat") || n.containsIgnoreCase ("Drive"))      neon = Colors::neonAmber;
    else if (n.containsIgnoreCase ("Vibe"))                                                                  neon = Colors::neonPurple;
    else if (n.containsIgnoreCase ("Drift") || n.containsIgnoreCase ("Cutoff") || n.containsIgnoreCase ("Reso")) neon = Colors::neonCyan;
    else if (n.containsIgnoreCase ("Mix")  || n.containsIgnoreCase ("Lvl") || n.containsIgnoreCase ("Master")) neon = Colors::neonGreen;

    juce::Path val;
    val.addCentredArc (cx, cy, radius - 3, radius - 3, 0.0f, rotaryStart, angle, true);
    for (int i = 4; i > 0; --i)
    {
        g.setColour (neon.withAlpha (0.10f * (float) i));
        g.strokePath (val, juce::PathStrokeType ((float) i * 2.0f + 2.0f,
                       juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (neon);
    g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Inner disc
    const float inner = radius * 0.66f;
    juce::ColourGradient grad (Colors::panelHi2, cx, cy - inner,
                               Colors::panel,    cx, cy + inner, false);
    g.setGradientFill (grad);
    g.fillEllipse (cx - inner, cy - inner, inner * 2, inner * 2);

    // Inner highlight ring
    g.setColour (neon.withAlpha (0.4f));
    g.drawEllipse (cx - inner, cy - inner, inner * 2, inner * 2, 1.0f);

    // Pointer
    juce::Path pointer;
    const float pl = inner * 0.85f;
    pointer.addRoundedRectangle (-1.8f, -pl, 3.6f, pl * 0.55f, 1.8f);
    g.setColour (neon);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (cx, cy));

    glowEllipse (g, juce::Rectangle<float> (cx - radius, cy - radius, radius * 2, radius * 2), neon, 1.0f);
}

juce::Label* VaporLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (Fonts::value());
    l->setColour (juce::Label::textColourId, Colors::textBright);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::TextEditor::backgroundColourId, Colors::panelHi);
    l->setColour (juce::TextEditor::textColourId, Colors::textBright);
    l->setColour (juce::TextEditor::highlightColourId, Colors::neonPink.withAlpha (0.4f));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

// ----- Combo box -----

void VaporLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool /*isDown*/,
                                     int /*btnX*/, int /*btnY*/, int /*btnW*/, int /*btnH*/, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (1.0f);

    // Soft gradient bg
    juce::ColourGradient grad (Colors::panelHi2, 0.0f, 0.0f,
                               Colors::panelHi,  0.0f, (float) h, false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 5.0f);

    // Outline
    const auto outlineCol = box.hasKeyboardFocus (true) ? Colors::neonPink : Colors::neonCyan.withAlpha (0.7f);
    g.setColour (outlineCol);
    g.drawRoundedRectangle (r, 5.0f, 1.4f);

    // Arrow (large, neon)
    const float ax = (float) w - 14.0f;
    const float ay = (float) h * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (ax - 5, ay - 3, ax + 5, ay - 3, ax, ay + 4);
    g.setColour (Colors::neonPink);
    g.fillPath (arrow);

    // Subtle separator before arrow
    g.setColour (Colors::neonCyan.withAlpha (0.25f));
    g.drawVerticalLine ((int) (ax - 9), 4.0f, (float) h - 4.0f);
}

juce::Font VaporLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return Fonts::combo();
}

void VaporLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (Fonts::combo());
    label.setColour (juce::Label::textColourId, Colors::textBright);
    label.setJustificationType (juce::Justification::centredLeft);
}

// ----- Popup menu -----

juce::Font VaporLookAndFeel::getPopupMenuFont() { return Fonts::combo(); }

void VaporLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h);
    g.setColour (Colors::panel);
    g.fillRect (r);
    g.setColour (Colors::neonPink.withAlpha (0.6f));
    g.drawRect (r, 1.2f);
}

void VaporLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                          bool isSeparator, bool isActive, bool isHighlighted,
                                          bool isTicked, bool /*hasSubMenu*/, const juce::String& text,
                                          const juce::String& /*shortcut*/, const juce::Drawable* /*icon*/,
                                          const juce::Colour* /*textColour*/)
{
    if (isSeparator)
    {
        g.setColour (Colors::neonCyan.withAlpha (0.25f));
        g.drawLine ((float) area.getX() + 4, (float) area.getCentreY(),
                    (float) area.getRight() - 4, (float) area.getCentreY());
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour (Colors::neonPink.withAlpha (0.35f));
        g.fillRect (area);
    }

    g.setColour (isActive ? (isTicked ? Colors::neonCyan : Colors::textBright) : Colors::textDim);
    g.setFont (Fonts::combo());
    auto textArea = area.reduced (10, 0);
    g.drawFittedText (text, textArea, juce::Justification::centredLeft, 1);

    if (isTicked)
    {
        const float ts = (float) area.getHeight() * 0.4f;
        const float cx = (float) area.getRight() - 16.0f;
        const float cy = (float) area.getCentreY();
        juce::Path tick;
        tick.startNewSubPath (cx - ts * 0.5f, cy);
        tick.lineTo (cx - ts * 0.15f, cy + ts * 0.4f);
        tick.lineTo (cx + ts * 0.5f, cy - ts * 0.4f);
        g.setColour (Colors::neonPink);
        g.strokePath (tick, juce::PathStrokeType (1.6f));
    }
}

// ----- Buttons / toggles -----

void VaporLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                             const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto isOn = b.getToggleState();
    juce::ColourGradient base (isOn ? Colors::neonPink.withAlpha (0.45f) : Colors::panelHi2,
                               0.0f, 0.0f,
                               isOn ? Colors::neonPink.withAlpha (0.25f) : Colors::panelHi,
                               0.0f, r.getHeight(), false);
    g.setGradientFill (base);
    g.fillRoundedRectangle (r, 4.0f);

    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (r, 4.0f);
    }
    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (r, 4.0f);
    }

    const auto outline = isOn ? Colors::neonPink : Colors::neonCyan.withAlpha (0.5f);
    g.setColour (outline);
    g.drawRoundedRectangle (r, 4.0f, isOn ? 1.6f : 1.0f);
}

juce::Font VaporLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return Fonts::button();
}

void VaporLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                         bool isMouseOver, bool isMouseDown)
{
    drawButtonBackground (g, b, juce::Colours::transparentBlack, isMouseOver, isMouseDown);
    g.setColour (b.getToggleState() ? Colors::textBright : Colors::textDim);
    g.setFont (Fonts::button());
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

// ----- Labels -----

void VaporLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& l)
{
    g.setColour (l.findColour (juce::Label::textColourId));
    g.setFont (l.getFont());  // Honor whatever font the label has been given.
    g.drawFittedText (l.getText(), l.getLocalBounds(), l.getJustificationType(), 1);
}

juce::Font VaporLookAndFeel::getLabelFont (juce::Label& l)
{
    // Used by JUCE for sizing/popup; just return the label's own font.
    return l.getFont();
}

// ----- Tabs -----

void VaporLookAndFeel::drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics& g)
{
    auto r = g.getClipBounds().toFloat();
    juce::ColourGradient grad (Colors::panel.darker (0.4f), 0.0f, 0.0f,
                               Colors::panel,                0.0f, r.getHeight(), false);
    g.setGradientFill (grad);
    g.fillRect (r);

    g.setColour (Colors::neonPink.withAlpha (0.55f));
    g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX(), r.getRight());
}

void VaporLookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                                      bool isMouseOver, bool /*isMouseDown*/)
{
    auto r = button.getActiveArea().toFloat();
    const bool isFront = button.isFrontTab();

    if (isFront)
    {
        juce::ColourGradient grad (Colors::neonPink.withAlpha (0.30f), r.getX(), r.getY(),
                                   Colors::panelHi2,                   r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (r);
        g.setColour (Colors::neonPink);
        g.drawHorizontalLine ((int) r.getBottom() - 2, r.getX() + 2, r.getRight() - 2);
    }
    else if (isMouseOver)
    {
        g.setColour (Colors::panelHi.withAlpha (0.6f));
        g.fillRect (r);
    }

    g.setColour (isFront ? Colors::textBright : Colors::textDim);
    g.setFont (Fonts::tab());
    g.drawFittedText (button.getButtonText(), r.toNearestInt(), juce::Justification::centred, 1);

    if (isFront)
    {
        // Subtle neon underline glow
        for (int i = 3; i > 0; --i)
        {
            g.setColour (Colors::neonPink.withAlpha (0.10f * (float) i));
            g.drawHorizontalLine ((int) r.getBottom() - 2 - i, r.getX() + 2, r.getRight() - 2);
        }
    }
}

int VaporLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& b, int /*tabDepth*/)
{
    // Avoid Font::getStringWidth (deprecation churn across JUCE versions).
    return (int) std::ceil ((float) b.getButtonText().length() * 10.5f) + 32;
}

juce::Font VaporLookAndFeel::getTabButtonFont (juce::TabBarButton&, float /*height*/)
{
    return Fonts::tab();
}
