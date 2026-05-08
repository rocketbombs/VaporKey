#pragma once
#include <JuceHeader.h>
#include "../LookAndFeel.h"

// Shared editor layout primitives. Kept header-only and inline so each page
// translation unit can inline these tight per-frame paint/layout calls
// without relying on cross-TU LTO.

namespace VKEditorLayout
{
    inline constexpr int kSectionTitleH = 36;

    inline void drawSectionBg (juce::Graphics& g, juce::Rectangle<int> r,
                               juce::Colour outline, const juce::String& title)
    {
        using namespace VK;
        auto rf = r.toFloat();
        juce::ColourGradient grad (Colors::panel.withAlpha (0.92f), 0.0f, rf.getY(),
                                   Colors::panel.darker (0.2f).withAlpha (0.92f), 0.0f, rf.getBottom(),
                                   false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (rf, 10.0f);

        g.setColour (outline.withAlpha (0.65f));
        g.drawRoundedRectangle (rf, 10.0f, 1.4f);

        if (title.isNotEmpty())
        {
            g.setColour (outline);
            g.setFont (Fonts::section());
            g.drawText (title, r.getX() + 14, r.getY() + 8, r.getWidth() - 28, 16,
                        juce::Justification::left);
            g.setColour (outline.withAlpha (0.4f));
            g.drawHorizontalLine (r.getY() + 27, (float) r.getX() + 14.0f, (float) r.getRight() - 14.0f);
        }
    }

    inline void layoutKnobRow (juce::Rectangle<int> area, std::vector<juce::Component*> cs,
                               int titleH = kSectionTitleH)
    {
        area.removeFromTop (titleH);
        area.reduce (10, 8);
        if (cs.empty()) return;
        const int kw = area.getWidth() / (int) cs.size();
        for (auto* c : cs)
            if (c) c->setBounds (area.removeFromLeft (kw));
    }

    inline void layoutKnobGrid (juce::Rectangle<int> area, std::vector<juce::Component*> cs,
                                int cols, int titleH = kSectionTitleH)
    {
        area.removeFromTop (titleH);
        area.reduce (10, 8);
        if (cs.empty()) return;
        const int rows = (int) std::ceil ((double) cs.size() / cols);
        const int kw = area.getWidth() / cols;
        const int kh = area.getHeight() / juce::jmax (1, rows);
        for (size_t i = 0; i < cs.size(); ++i)
        {
            if (! cs[i]) continue;
            const int r0 = (int) i / cols;
            const int c0 = (int) i % cols;
            cs[i]->setBounds (area.getX() + c0 * kw, area.getY() + r0 * kh, kw, kh);
        }
    }
}
