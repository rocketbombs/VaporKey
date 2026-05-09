#pragma once
#include <JuceHeader.h>
#include "VaporWidgets.h"

// X-MOD matrix for the OSC page: lets each oscillator pick another oscillator
// as a phase / ring / amplitude modulator. The widget owns the JUCE controls
// (one source combo, one type combo, one amount knob per destination) and
// renders a small node-and-arrow diagram on the left visualising the active
// routing. Hooks into the APVTS so the diagram repaints in lock-step with
// preset loads, host automation and direct knob tweaks.
class OscModMatrix : public juce::Component,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit OscModMatrix (juce::AudioProcessorValueTreeState& apvts);
    ~OscModMatrix() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // One destination row: source-osc combo, mod-type combo, amount slider.
    // The amount control is a horizontal slider rather than a rotary so the
    // band can stay compact (rotaries need ~60 px of vertical to read well;
    // a horizontal slider is fine at ~22 px and visually doubles as a
    // depth meter).
    struct Row
    {
        juce::Label   header;
        std::unique_ptr<VaporCombo> src;
        std::unique_ptr<VaporCombo> type;
        juce::Slider  amt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAttachment;
    };

    void parameterChanged (const juce::String&, float) override;

    // Resolve current routing for diagram drawing. Reads the APVTS atomics.
    struct Route { int src; int type; float amt; };
    Route currentRoute (int dest) const noexcept;

    // Paints the three osc nodes plus any active connection arrows into r.
    void paintDiagram (juce::Graphics& g, juce::Rectangle<float> r);

    juce::AudioProcessorValueTreeState& state;
    Row rows[3];

    // Cached layout: kept in members so paint() can draw arrows aligned
    // exactly with each row's amount knob without recomputing geometry.
    juce::Rectangle<int> diagramArea;
    juce::Point<float>   nodeCentre[3];
    float                nodeRadius = 14.0f;
};
