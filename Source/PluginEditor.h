#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"
#include "Widgets/Meters.h"

// Top-level editor with TabbedComponent. Drives a low-rate animation timer
// that powers the starfield twinkle and the audio-reactive sun pulse.
//
// Multi-instance perf notes:
//   * The static parts of the backdrop (sky gradient, retrowave grid, sun
//     body, header text) are baked into `backdrop` once per resize so each
//     frame only re-renders the animated overlays (stars, sun halo, sun rim).
//   * The animation timer is paused when the editor isn't on screen so a
//     hidden plugin window doesn't burn the host's UI thread.
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
    void visibilityChanged() override        { syncTimerToVisibility(); }
    void parentHierarchyChanged() override   { syncTimerToVisibility(); }

    void initStars();
    void rebuildBackdrop();
    void syncTimerToVisibility();
    void drawStars (juce::Graphics&, juce::Rectangle<float> r);
    void drawSunOverlay (juce::Graphics&, juce::Rectangle<float> r);

    VaporKeyAudioProcessor& proc;
    VaporLookAndFeel lnf;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    std::unique_ptr<LevelMeter> headerMeter;

    struct Star { float x01, y01, baseAlpha, twinkleHz, phase, radius; };
    std::vector<Star> stars;
    float animPhase = 0.0f;
    float sunPulse  = 0.0f;

    juce::Image backdrop;       // cached sky + grid + sun body + header text
    bool        timerActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VaporKeyAudioProcessorEditor)
};
