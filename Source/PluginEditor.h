#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"
#include "Widgets/Meters.h"

// Top-level editor with TabbedComponent. Drives a low-rate animation timer
// that powers the starfield twinkle and the audio-reactive sun pulse.
class VaporKeyAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit VaporKeyAudioProcessorEditor (VaporKeyAudioProcessor&);
    ~VaporKeyAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void initStars();
    void drawStars (juce::Graphics&, juce::Rectangle<float> r);
    void drawSun   (juce::Graphics&, juce::Rectangle<float> r);

    VaporKeyAudioProcessor& proc;
    VaporLookAndFeel lnf;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    std::unique_ptr<LevelMeter> headerMeter;

    struct Star { float x01, y01, baseAlpha, twinkleHz, phase, radius; };
    std::vector<Star> stars;
    float animPhase = 0.0f;
    float sunPulse  = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VaporKeyAudioProcessorEditor)
};
