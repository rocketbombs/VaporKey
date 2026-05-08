#pragma once
#include <JuceHeader.h>

class VaporKnob : public juce::Component
{
public:
    VaporKnob (juce::AudioProcessorValueTreeState& s, const juce::String& paramID, const juce::String& displayName);
    void resized() override;

    juce::Slider slider;
    juce::Label  label;
    juce::String name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class VaporCombo : public juce::Component
{
public:
    VaporCombo (juce::AudioProcessorValueTreeState& s, const juce::String& paramID,
                const juce::String& displayName, const juce::StringArray& items);
    void resized() override;

    juce::ComboBox box;
    juce::Label    label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class VaporToggle : public juce::Component
{
public:
    VaporToggle (juce::AudioProcessorValueTreeState& s, const juce::String& paramID, const juce::String& text);
    void resized() override;

    juce::TextButton btn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};
