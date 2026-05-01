#pragma once
#include <JuceHeader.h>

namespace VK
{
    namespace Colors
    {
        const juce::Colour bg        { 0xff0a0420 };
        const juce::Colour panel     { 0xff140833 };
        const juce::Colour panelHi   { 0xff1d0d4a };
        const juce::Colour panelHi2  { 0xff261066 };
        const juce::Colour grid      { 0x40ff2ec4 };
        const juce::Colour neonPink  { 0xffff2ec4 };
        const juce::Colour neonCyan  { 0xff29f5ff };
        const juce::Colour neonPurple{ 0xff8a2bff };
        const juce::Colour neonAmber { 0xffffb347 };
        const juce::Colour neonGreen { 0xff5fff8e };
        const juce::Colour text      { 0xfff0e7ff };
        const juce::Colour textBright{ 0xffffffff };
        const juce::Colour textDim   { 0xb0d6c8ff };
    }

    namespace Fonts
    {
        // Centralized typography. All explicit; never auto-sized from component height.
        inline juce::Font header   () { return juce::Font (juce::FontOptions (28.0f).withStyle ("Bold")); }
        inline juce::Font subheader() { return juce::Font (juce::FontOptions (14.0f).withStyle ("Bold")); }
        inline juce::Font section  () { return juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")); }
        inline juce::Font label    () { return juce::Font (juce::FontOptions (13.0f).withStyle ("Bold")); }
        inline juce::Font value    () { return juce::Font (juce::FontOptions (12.0f)); }
        inline juce::Font combo    () { return juce::Font (juce::FontOptions (13.0f).withStyle ("Bold")); }
        inline juce::Font tab      () { return juce::Font (juce::FontOptions (13.0f).withStyle ("Bold")); }
        inline juce::Font button   () { return juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")); }
        inline juce::Font preset   () { return juce::Font (juce::FontOptions (15.0f).withStyle ("Bold")); }
        inline juce::Font small    () { return juce::Font (juce::FontOptions (11.0f)); }
    }
}

class VaporLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VaporLookAndFeel();

    // Knobs
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float rotaryStart, float rotaryEnd,
                           juce::Slider&) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;

    // Combo box
    void drawComboBox (juce::Graphics&, int w, int h, bool isDown,
                       int btnX, int btnY, int btnW, int btnH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;

    // Popup menu
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;

    // Buttons / toggles
    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;

    // Labels
    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Font getLabelFont (juce::Label&) override;

    // Tab bar
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool, bool) override;
    void drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&) override;
    int  getTabButtonBestWidth (juce::TabBarButton&, int tabDepth) override;
    juce::Font getTabButtonFont (juce::TabBarButton&, float height) override;
};
