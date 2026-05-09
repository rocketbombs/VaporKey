#include "WavetableShapePicker.h"
#include "../LookAndFeel.h"

using namespace VK;

WavetableShapePicker::WavetableShapePicker (juce::AudioProcessorValueTreeState& apvts,
                                            const juce::String& paramID)
    : state (apvts), paramId (paramID)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setInterceptsMouseClicks (true, false);

    state.addParameterListener (paramId, this);
    if (auto* p = state.getRawParameterValue (paramId))
        shapeMirror.store ((int) (p->load() + 0.5f));
}

WavetableShapePicker::~WavetableShapePicker()
{
    state.removeParameterListener (paramId, this);
}

int WavetableShapePicker::currentShape() const noexcept
{
    return juce::jlimit (0, (int) WavetableLibrary::NumShapes - 1,
                         shapeMirror.load());
}

void WavetableShapePicker::setShape (int shape)
{
    if (auto* p = state.getParameter (paramId))
    {
        const float norm = p->getNormalisableRange().convertTo0to1 ((float) shape);
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
    }
}

void WavetableShapePicker::parameterChanged (const juce::String&, float newValue)
{
    shapeMirror.store ((int) (newValue + 0.5f));
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<WavetableShapePicker> (this)]
    {
        if (auto* c = safeThis.getComponent()) c->repaint();
    });
}

void WavetableShapePicker::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);

    // Background gradient matching the combo box style in the L&F so this
    // visually slots in next to the rest of the OSC controls.
    juce::ColourGradient grad (Colors::panelHi2, 0.0f, 0.0f,
                               Colors::panelHi,  0.0f, r.getHeight(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 4.0f);

    g.setColour (Colors::neonCyan.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    // Shape name (left), category hint (right of name).
    const int shape = currentShape();
    const auto name = juce::String (WavetableLibrary::shapeName (shape));

    juce::String catName;
    for (const auto& c : WavetableLibrary::categoriesAndShapes())
        if (c.shapes.contains (shape)) { catName = c.name.toUpperCase(); break; }

    auto textArea = r.reduced (10.0f, 0.0f).withTrimmedRight (24.0f);

    g.setFont (Fonts::combo());
    g.setColour (Colors::textBright);
    g.drawText (name, textArea.toNearestInt(), juce::Justification::centredLeft);

    if (catName.isNotEmpty())
    {
        g.setFont (Fonts::small());
        g.setColour (Colors::neonCyan.withAlpha (0.55f));
        g.drawText (catName, textArea.toNearestInt(), juce::Justification::centredRight);
    }

    // Dropdown chevron (matches drawComboBox).
    const float cx = r.getRight() - 14.0f;
    const float cy = r.getCentreY();
    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo          (cx,        cy + 3.0f);
    chevron.lineTo          (cx + 4.0f, cy - 2.0f);
    g.setColour (Colors::neonPink);
    g.strokePath (chevron, juce::PathStrokeType (1.6f));
}

void WavetableShapePicker::mouseDown (const juce::MouseEvent& /*e*/)
{
    showMenu();
}

void WavetableShapePicker::showMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());

    const int currentS = currentShape();
    const auto& cats = WavetableLibrary::categoriesAndShapes();

    for (size_t ci = 0; ci < cats.size(); ++ci)
    {
        const auto& cat = cats[ci];

        menu.addSectionHeader (cat.name.toUpperCase());
        for (int s : cat.shapes)
        {
            // Item id = shape + 1 (PopupMenu reserves id 0 for "no selection").
            menu.addItem (s + 1, juce::String (WavetableLibrary::shapeName (s)),
                          /*isActive*/ true, /*isTicked*/ s == currentS);
        }
        if (ci + 1 < cats.size())
            menu.addColumnBreak();
    }

    auto opts = juce::PopupMenu::Options()
                    .withTargetComponent (this)
                    .withMinimumWidth (juce::jmax (160, getWidth()))
                    .withStandardItemHeight (26);

    menu.showMenuAsync (opts,
        [safeThis = juce::Component::SafePointer<WavetableShapePicker> (this)] (int r)
    {
        if (r <= 0) return;
        if (auto* c = safeThis.getComponent())
            c->setShape (r - 1);
    });
}
