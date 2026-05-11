#pragma once
#include <JuceHeader.h>

class VaporKeyAudioProcessor;

// Click-drag-to-scrub wavetable display (drives the position parameter).
// Also accepts .wav files via drag-and-drop or right-click "Load .wav..." to
// fill the per-oscillator Custom wavetable slot.
class WavetableDisplay : public juce::Component,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
{
public:
    WavetableDisplay (VaporKeyAudioProcessor& proc, int oscIndex);
    ~WavetableDisplay() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit  (const juce::StringArray&) override;
    void filesDropped  (const juce::StringArray& files, int x, int y) override;

    void visibilityChanged() override      { syncTimerToVisibility(); }
    void parentHierarchyChanged() override { syncTimerToVisibility(); }

private:
    void timerCallback() override { repaint(); }
    void syncTimerToVisibility();
    void setPositionFromMouse (const juce::MouseEvent& e);
    void showLoadMenu();
    void chooseWavFile();

    VaporKeyAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    int idx;
    bool dragHover = false;
    std::unique_ptr<juce::FileChooser> chooser;
};
