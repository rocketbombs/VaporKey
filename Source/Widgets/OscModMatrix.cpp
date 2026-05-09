#include "OscModMatrix.h"
#include "../LookAndFeel.h"
#include "../Parameters.h"

using namespace VK;

namespace
{
    juce::Colour typeColour (int type)
    {
        switch (type)
        {
            case OscModType::FM:   return Colors::neonPink;
            case OscModType::Ring: return Colors::neonCyan;
            case OscModType::AM:   return Colors::neonAmber;
            default:               return Colors::neonPink;
        }
    }

    juce::String oscParam (int oscIndex, const char* tail)
    {
        return "osc" + juce::String (oscIndex + 1) + "_" + tail;
    }
}

OscModMatrix::OscModMatrix (juce::AudioProcessorValueTreeState& apvts)
    : state (apvts)
{
    // Make sure the diagram repaints when any routing parameter changes -
    // covers preset loads, host automation, undo, and our own controls.
    for (int i = 0; i < 3; ++i)
    {
        state.addParameterListener (oscParam (i, "mod_src"),  this);
        state.addParameterListener (oscParam (i, "mod_type"), this);
        state.addParameterListener (oscParam (i, "mod_amt"),  this);
    }

    for (int i = 0; i < 3; ++i)
    {
        auto& row = rows[i];

        row.header.setText ("OSC " + juce::String (i + 1) + " \xe2\x86\x90",
                            juce::dontSendNotification);
        row.header.setJustificationType (juce::Justification::centredLeft);
        row.header.setFont (Fonts::label());
        row.header.setColour (juce::Label::textColourId, Colors::neonCyan);
        addAndMakeVisible (row.header);

        row.src  = std::make_unique<VaporCombo> (state, oscParam (i, "mod_src"),
                                                 juce::String(), OscModSrc::names());
        row.type = std::make_unique<VaporCombo> (state, oscParam (i, "mod_type"),
                                                 juce::String(), OscModType::names());

        row.amt.setSliderStyle (juce::Slider::LinearHorizontal);
        row.amt.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
        row.amt.setName ("Amount");
        row.amt.setColour (juce::Slider::textBoxTextColourId,  Colors::textBright);
        row.amt.setColour (juce::Slider::backgroundColourId,   Colors::panelHi.withAlpha (0.55f));
        row.amt.setColour (juce::Slider::trackColourId,        Colors::neonPurple);
        row.amt.setColour (juce::Slider::thumbColourId,        Colors::neonCyan);
        row.amt.setWantsKeyboardFocus (false);
        row.amtAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment> (
                state, oscParam (i, "mod_amt"), row.amt);

        addAndMakeVisible (*row.src);
        addAndMakeVisible (*row.type);
        addAndMakeVisible (row.amt);
    }
}

OscModMatrix::~OscModMatrix()
{
    for (int i = 0; i < 3; ++i)
    {
        state.removeParameterListener (oscParam (i, "mod_src"),  this);
        state.removeParameterListener (oscParam (i, "mod_type"), this);
        state.removeParameterListener (oscParam (i, "mod_amt"),  this);
    }
}

void OscModMatrix::parameterChanged (const juce::String&, float)
{
    juce::MessageManager::callAsync (
        [safeThis = juce::Component::SafePointer<OscModMatrix> (this)]
        {
            if (auto* c = safeThis.getComponent()) c->repaint();
        });
}

OscModMatrix::Route OscModMatrix::currentRoute (int dest) const noexcept
{
    auto load = [this] (const juce::String& id) -> float
    {
        if (auto* p = state.getRawParameterValue (id)) return p->load();
        return 0.0f;
    };

    Route r {};
    const int rawSrc = (int) (load (oscParam (dest, "mod_src")) + 0.5f);
    r.src = (rawSrc > 0 && (rawSrc - 1) != dest) ? rawSrc - 1 : -1;
    r.type = (int) (load (oscParam (dest, "mod_type")) + 0.5f);
    r.amt  = juce::jlimit (0.0f, 1.0f, load (oscParam (dest, "mod_amt")));
    return r;
}

void OscModMatrix::resized()
{
    auto r = getLocalBounds();

    // Diagram on the left; 3 control rows stacked on the right. The diagram
    // gets a square-ish slice so the triangle doesn't get squashed at typical
    // widths.
    const int diagramW = juce::jlimit (140, 220, r.getWidth() / 3);
    diagramArea = r.removeFromLeft (diagramW).reduced (12);

    // Triangle layout: dest 1 (top-centre), dest 2 (bottom-left), dest 3
    // (bottom-right). The triangle reads left-to-right for routing because
    // the right-hand rows are also OSC 1 / 2 / 3 top-to-bottom.
    const auto df = diagramArea.toFloat();
    const float cx = df.getCentreX();
    const float cy = df.getCentreY();
    const float h  = df.getHeight() * 0.42f;
    const float w  = df.getWidth()  * 0.42f;
    nodeCentre[0] = { cx,         cy - h };
    nodeCentre[1] = { cx - w,     cy + h * 0.55f };
    nodeCentre[2] = { cx + w,     cy + h * 0.55f };

    r.removeFromLeft (8);

    const int rowH = r.getHeight() / 3;
    for (int i = 0; i < 3; ++i)
    {
        auto rowR = r.removeFromTop (rowH).reduced (4, 3);
        rows[i].header.setBounds (rowR.removeFromLeft (74));
        // Source picker (~28%), type picker (~22%), amount slider (~remainder).
        const int srcW  = juce::jmax (78, rowR.getWidth() / 3);
        const int typeW = juce::jmax (66, rowR.getWidth() / 4);
        rows[i].src ->setBounds (rowR.removeFromLeft (srcW).reduced (2, 2));
        rows[i].type->setBounds (rowR.removeFromLeft (typeW).reduced (2, 2));
        rows[i].amt .setBounds (rowR.reduced (4, 4));
    }
}

void OscModMatrix::paint (juce::Graphics& g)
{
    paintDiagram (g, diagramArea.toFloat());

    // Row separators on the right - subtle dashed lines so the three slots
    // read as distinct items even before they're populated.
    auto fullBounds = getLocalBounds();
    const int rightX = diagramArea.getRight() + 8;
    const int rowH   = (fullBounds.getHeight()) / 3;
    g.setColour (Colors::neonCyan.withAlpha (0.12f));
    for (int i = 1; i < 3; ++i)
    {
        const float y = (float) (i * rowH);
        const float dashes[] { 4.0f, 4.0f };
        juce::Line<float> line ((float) rightX, y, (float) fullBounds.getRight() - 4.0f, y);
        g.drawDashedLine (line, dashes, 2, 1.0f);
    }
}

void OscModMatrix::paintDiagram (juce::Graphics& g, juce::Rectangle<float> r)
{
    if (r.isEmpty()) return;

    // 1) Faint dotted backdrop so the diagram has its own visual frame even
    // when no routes are active.
    g.setColour (Colors::neonCyan.withAlpha (0.08f));
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    // 2) Connection arrows (drawn first, behind the nodes). For each
    // destination osc, look up its current routing and draw a curved
    // bezier from source -> dest, colour by type, opacity / thickness by
    // amount. Self-routes are filtered out by currentRoute.
    for (int dest = 0; dest < 3; ++dest)
    {
        const Route route = currentRoute (dest);
        if (route.src < 0 || route.amt <= 0.001f) continue;

        const auto a = nodeCentre[route.src];
        const auto b = nodeCentre[dest];

        // Bezier control point: perpendicular offset from the midpoint, so
        // bidirectional routes (e.g. 1<->2) curve in opposite directions
        // and remain visually distinct.
        const auto mid = (a + b) * 0.5f;
        const auto delta = b - a;
        const auto perp  = juce::Point<float> (-delta.y, delta.x);
        const float perpLen = juce::jmax (1.0e-3f, perp.getDistanceFromOrigin());
        const auto unitPerp = perp / perpLen;
        // Sign: route.src < dest curves one way, the reverse curves the
        // other - so a 1->2 and 2->1 pair don't overlap.
        const float sign = (route.src < dest) ? 1.0f : -1.0f;
        const float bend = juce::jmin (28.0f, delta.getDistanceFromOrigin() * 0.35f);
        const auto ctrl = mid + unitPerp * (bend * sign);

        // Pull the arrow endpoints in to the node edges so the arrowhead
        // doesn't bury itself inside the destination node.
        const auto ab = (b - a);
        const float abLen = juce::jmax (1.0e-3f, ab.getDistanceFromOrigin());
        const auto abUnit = ab / abLen;
        const auto start = a + abUnit * (nodeRadius + 1.0f);
        const auto end   = b - abUnit * (nodeRadius + 4.0f);

        juce::Path path;
        path.startNewSubPath (start);
        path.quadraticTo (ctrl, end);

        const auto col   = typeColour (route.type);
        const float alpha = 0.30f + 0.65f * route.amt;
        const float thick = 1.4f + 2.6f * route.amt;

        // Soft glow underneath the main stroke for the synthwave look.
        g.setColour (col.withAlpha (alpha * 0.35f));
        g.strokePath (path, juce::PathStrokeType (thick + 3.5f,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        g.setColour (col.withAlpha (alpha));
        g.strokePath (path, juce::PathStrokeType (thick,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        // Arrowhead at the destination.
        const auto headDir = end - ctrl;
        const float headLen = juce::jmax (1.0e-3f, headDir.getDistanceFromOrigin());
        const auto headUnit = headDir / headLen;
        const auto headPerp = juce::Point<float> (-headUnit.y, headUnit.x);
        const float headSize = 6.0f + 4.0f * route.amt;
        const auto p1 = end - headUnit * headSize + headPerp * (headSize * 0.55f);
        const auto p2 = end - headUnit * headSize - headPerp * (headSize * 0.55f);
        juce::Path head;
        head.addTriangle (end, p1, p2);
        g.setColour (col.withAlpha (juce::jmin (1.0f, alpha + 0.15f)));
        g.fillPath (head);

        // Type abbreviation floating near the bend - cheap visual cue
        // ("FM" / "RM" / "AM") that doesn't require reading the controls.
        const auto labelPos = ctrl;
        const juce::String tag = (route.type == OscModType::FM)   ? "FM"
                               : (route.type == OscModType::Ring) ? "RM"
                                                                  : "AM";
        g.setFont (Fonts::small());
        g.setColour (col.withAlpha (juce::jmin (1.0f, alpha + 0.2f)));
        g.drawText (tag,
                    juce::Rectangle<float> (labelPos.x - 14.0f, labelPos.y - 8.0f, 28.0f, 14.0f),
                    juce::Justification::centred);
    }

    // 3) Nodes on top. Each node gets a subtle outer halo, an inner filled
    // disc, and a centred number. The destination node (where arrows
    // terminate) gets a brighter ring so it reads as "the carrier".
    for (int i = 0; i < 3; ++i)
    {
        const auto c = nodeCentre[i];
        const float rad = nodeRadius;

        // Halo
        g.setColour (Colors::neonPink.withAlpha (0.10f));
        g.fillEllipse (c.x - rad - 4.0f, c.y - rad - 4.0f,
                       (rad + 4.0f) * 2.0f, (rad + 4.0f) * 2.0f);

        // Body
        juce::ColourGradient body (Colors::panelHi2,  c.x, c.y - rad,
                                   Colors::panel,     c.x, c.y + rad, false);
        g.setGradientFill (body);
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f);

        // Outline coloured by whether this osc is acting as a source somewhere.
        bool isSource = false;
        for (int dest = 0; dest < 3; ++dest)
        {
            const Route route = currentRoute (dest);
            if (route.src == i && route.amt > 0.001f) { isSource = true; break; }
        }
        const auto outline = isSource ? Colors::neonCyan : Colors::neonPink;
        g.setColour (outline.withAlpha (0.85f));
        g.drawEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f, 1.4f);

        g.setColour (Colors::textBright);
        g.setFont (Fonts::label());
        g.drawText (juce::String (i + 1),
                    juce::Rectangle<float> (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f),
                    juce::Justification::centred);
    }
}
