#pragma once
#include <JuceHeader.h>

// Interactive 3-band EQ display: drag the Low / Mid / High nodes to set gain
// (and Mid's frequency horizontally). The composite magnitude curve is drawn
// underneath. Reads/writes EQ params directly via APVTS.
class EqCurve : public juce::Component, private juce::Timer
{
public:
    explicit EqCurve (juce::AudioProcessorValueTreeState& s);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    enum Node { NodeNone = -1, NodeLow = 0, NodeMid, NodeHigh };

    void timerCallback() override { repaint(); }

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

    juce::AudioProcessorValueTreeState& apvts;
    Node dragging = NodeNone;
};
