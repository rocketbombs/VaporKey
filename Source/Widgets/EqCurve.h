#pragma once
#include <JuceHeader.h>

// Interactive 3-band EQ display: drag the Low / Mid / High nodes to set gain
// (and Mid's frequency horizontally). The composite magnitude curve is drawn
// underneath. Reads/writes EQ params directly via APVTS.
//
// Repaint is driven by APVTS parameter listeners (eq_low / eq_mid /
// eq_mid_freq / eq_high) rather than a polling Timer. Listening to the
// parameters covers user drags, host automation, preset loads, and undo
// without burning a 30 Hz repaint when nothing actually changed - and without
// depending on visibilityChanged/parentHierarchyChanged firing at the right
// moment, which on tab-switch could leave the timer stopped and the curve
// frozen until the next forced repaint.
class EqCurve : public juce::Component,
                private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit EqCurve (juce::AudioProcessorValueTreeState& s);
    ~EqCurve() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    enum Node { NodeNone = -1, NodeLow = 0, NodeMid, NodeHigh };

    void parameterChanged (const juce::String&, float) override;

    juce::Rectangle<float> plotArea() const;
    float xForFreq (float hz, juce::Rectangle<float> r) const;
    float freqForX (float x,  juce::Rectangle<float> r) const;
    float yForDb   (float db, juce::Rectangle<float> r) const;
    float dbForY   (float y,  juce::Rectangle<float> r) const;

    juce::Point<float> nodePos (Node) const;
    Node hitTest (juce::Point<float> p) const;
    void resetNode (Node);

    float currentLowG()  const;
    float currentMidG()  const;
    float currentMidF()  const;
    float currentHighG() const;

    void  setParam (const juce::String& id, float value);
    void  beginGesture (const juce::String& id);
    void  endGesture   (const juce::String& id);

    static const juce::StringArray& watchedParamIds();

    juce::AudioProcessorValueTreeState& apvts;
    Node dragging = NodeNone;
};
