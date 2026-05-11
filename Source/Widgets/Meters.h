#pragma once
#include <JuceHeader.h>

class VaporKeyAudioProcessor;

// Stereo peak meter with falling peak hold, fed from VisData::peakL/R.
// Timer is paused while the component isn't actually on screen (hidden tab,
// closed editor) so multiple plugin instances don't all burn the host's UI
// thread refreshing meters that aren't visible.
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter (VaporKeyAudioProcessor& p);
    ~LevelMeter() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void visibilityChanged() override      { syncTimerToVisibility(); }
    void parentHierarchyChanged() override { syncTimerToVisibility(); }
private:
    void timerCallback() override;
    void syncTimerToVisibility();
    VaporKeyAudioProcessor& processor;
    float lvlL = 0.0f, lvlR = 0.0f;
    float peakL = 0.0f, peakR = 0.0f;
    int   peakHoldL = 0, peakHoldR = 0;
    bool  timerActive = false;
};

// Output oscilloscope reading the post-FX scope ring buffer from the processor.
class Scope : public juce::Component, private juce::Timer
{
public:
    explicit Scope (VaporKeyAudioProcessor& p);
    ~Scope() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void visibilityChanged() override      { syncTimerToVisibility(); }
    void parentHierarchyChanged() override { syncTimerToVisibility(); }
private:
    void timerCallback() override { repaint(); }
    void syncTimerToVisibility();
    VaporKeyAudioProcessor& processor;
    bool timerActive = false;
};
