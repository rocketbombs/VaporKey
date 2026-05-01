#pragma once
#include <JuceHeader.h>

namespace VK
{
    namespace Colors
    {
        const juce::Colour bg        { 0xff0a0420 };
        const juce::Colour panel     { 0xff140833 };
        const juce::Colour panelHi   { 0xff1d0d4a };
        const juce::Colour grid      { 0x40ff2ec4 };
        const juce::Colour neonPink  { 0xffff2ec4 };
        const juce::Colour neonCyan  { 0xff29f5ff };
        const juce::Colour neonPurple{ 0xff8a2bff };
        const juce::Colour neonAmber { 0xffffb347 };
        const juce::Colour text      { 0xfff0e7ff };
        const juce::Colour textDim   { 0x99f0e7ff };
    }
}

class VaporLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VaporLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float rotaryStart, float rotaryEnd,
                           juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool isDown,
                       int btnX, int btnY, int btnW, int btnH, juce::ComboBox&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour&, bool, bool) override;

    void drawLabel (juce::Graphics&, juce::Label&) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};
