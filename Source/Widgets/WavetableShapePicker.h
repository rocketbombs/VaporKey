#pragma once
#include <JuceHeader.h>
#include "../Wavetable.h"

// A combo-style shape selector for the OSC page that opens a multi-column
// popup grouped by WavetableLibrary::categoriesAndShapes(). With 23+
// shapes a flat dropdown becomes unwieldy; the categorised popup keeps
// the related timbres adjacent and the menu compact (one click, one
// scroll-free view).
//
// Behaves like VaporCombo from the outside: takes an APVTS + the shape
// parameter id, draws a button, and binds the parameter through a
// ParameterAttachment so external automation / preset loads still update
// the widget.
class WavetableShapePicker : public juce::Component,
                             private juce::AudioProcessorValueTreeState::Listener
{
public:
    WavetableShapePicker (juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& paramID);
    ~WavetableShapePicker() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    void parameterChanged (const juce::String& id, float newValue) override;
    void showMenu();

    int   currentShape() const noexcept;
    void  setShape (int shape);

    juce::AudioProcessorValueTreeState& state;
    juce::String paramId;

    // Latest parameter value (mirrored from the listener callback). Read on
    // the message thread by paint() / showMenu(); written by the parameter
    // listener which fires on the message thread under the APVTS lock.
    std::atomic<int> shapeMirror { 0 };
};
