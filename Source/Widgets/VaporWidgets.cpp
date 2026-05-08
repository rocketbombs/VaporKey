#include "VaporWidgets.h"
#include "../LookAndFeel.h"

using namespace VK;

VaporKnob::VaporKnob (juce::AudioProcessorValueTreeState& s, const juce::String& paramID, const juce::String& displayName)
    : name (displayName)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 22);
    slider.setName (displayName);
    slider.setColour (juce::Slider::textBoxTextColourId, Colors::textBright);
    // Keep keyboard focus on the editor itself so the host (and any computer-
    // keyboard MIDI input it routes through us) keeps receiving key events
    // after the user tweaks a knob, instead of having to click the header to
    // hand focus back.
    slider.setWantsKeyboardFocus (false);
    addAndMakeVisible (slider);

    label.setText (displayName, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, Colors::neonCyan);
    label.setFont (Fonts::label());
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramID, slider);
}

void VaporKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (18));
    slider.setBounds (r);
}

VaporCombo::VaporCombo (juce::AudioProcessorValueTreeState& s, const juce::String& paramID,
                       const juce::String& displayName, const juce::StringArray& items)
{
    for (int i = 0; i < items.size(); ++i) box.addItem (items[i], i + 1);
    addAndMakeVisible (box);

    label.setText (displayName, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, Colors::neonCyan);
    label.setFont (Fonts::label());
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, paramID, box);
}

void VaporCombo::resized()
{
    auto r = getLocalBounds();
    if (r.getHeight() >= 44)
        label.setBounds (r.removeFromTop (18));
    else
        label.setVisible (false);
    box.setBounds (r.reduced (2, 1));
}

VaporToggle::VaporToggle (juce::AudioProcessorValueTreeState& s, const juce::String& paramID, const juce::String& text)
{
    btn.setButtonText (text);
    btn.setClickingTogglesState (true);
    addAndMakeVisible (btn);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, paramID, btn);
}

void VaporToggle::resized() { btn.setBounds (getLocalBounds().reduced (2)); }
