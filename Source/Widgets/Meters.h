#pragma once
#include <JuceHeader.h>

class VaporKeyAudioProcessor;

// Stereo peak meter with falling peak hold, fed from VisData::peakL/R.
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter (VaporKeyAudioProcessor& p);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    VaporKeyAudioProcessor& processor;
    float lvlL = 0.0f, lvlR = 0.0f;
    float peakL = 0.0f, peakR = 0.0f;
    int   peakHoldL = 0, peakHoldR = 0;
};

// Output oscilloscope reading the post-FX scope ring buffer from the processor.
class Scope : public juce::Component, private juce::Timer
{
public:
    explicit Scope (VaporKeyAudioProcessor& p);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    VaporKeyAudioProcessor& processor;
};
