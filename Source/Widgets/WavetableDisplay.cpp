#include "WavetableDisplay.h"
#include "../PluginProcessor.h"
#include "../LookAndFeel.h"

using namespace VK;

WavetableDisplay::WavetableDisplay (VaporKeyAudioProcessor& proc, int oscIndex)
    : processor (proc), apvts (proc.apvts), idx (oscIndex)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setOpaque (false);
    startTimerHz (24);
}

bool WavetableDisplay::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".wav")) return true;
    return false;
}

void WavetableDisplay::fileDragEnter (const juce::StringArray&, int, int)
{
    dragHover = true;
    repaint();
}

void WavetableDisplay::fileDragExit (const juce::StringArray&)
{
    dragHover = false;
    repaint();
}

void WavetableDisplay::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    for (const auto& f : files)
    {
        if (! f.endsWithIgnoreCase (".wav")) continue;
        if (processor.loadCustomWavetable (idx, juce::File (f)))
        {
            if (auto* p = apvts.getParameter ("osc" + juce::String (idx + 1) + "_shape"))
            {
                const float norm = p->getNormalisableRange().convertTo0to1 ((float) WavetableLibrary::Custom);
                p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
            }
            break;
        }
    }
    repaint();
}

void WavetableDisplay::chooseWavFile()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Choose a .wav file for OSC " + juce::String (idx + 1),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav");

    const int chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File()) return;
        if (processor.loadCustomWavetable (idx, file))
        {
            if (auto* p = apvts.getParameter ("osc" + juce::String (idx + 1) + "_shape"))
            {
                const float norm = p->getNormalisableRange().convertTo0to1 ((float) WavetableLibrary::Custom);
                p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
            }
        }
    });
}

void WavetableDisplay::showLoadMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Load .wav file...");
    m.addItem (2, "Clear custom wavetable",
               processor.getCustomWavetableName (idx).isNotEmpty());

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [this] (int r)
                     {
                         if (r == 1) chooseWavFile();
                         else if (r == 2) processor.clearCustomWavetable (idx);
                     });
}

void WavetableDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);

    juce::ColourGradient grad (Colors::bg, 0.0f, r.getY(),
                               Colors::bg.darker (0.6f), 0.0f, r.getBottom(),
                               false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 6.0f);

    g.setColour (Colors::neonPink.withAlpha (0.45f));
    g.drawRoundedRectangle (r, 6.0f, 1.2f);

    g.setColour (Colors::grid);
    for (int i = 1; i < 4; ++i)
    {
        const float y = r.getY() + r.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine ((int) y, r.getX() + 4.0f, r.getRight() - 4.0f);
    }
    for (int i = 1; i < 8; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / 8.0f;
        g.setColour (Colors::neonPink.withAlpha (0.07f));
        g.drawVerticalLine ((int) x, r.getY() + 4.0f, r.getBottom() - 4.0f);
    }

    const int shape = (int) (apvts.getRawParameterValue ("osc" + juce::String (idx + 1) + "_shape")->load() + 0.5f);
    const float pos = apvts.getRawParameterValue ("osc" + juce::String (idx + 1) + "_pos")->load();

    std::shared_ptr<Wavetable> customSnap;
    const Wavetable* wtPtr = nullptr;
    if (shape == WavetableLibrary::Custom)
    {
        customSnap = std::atomic_load (&processor.synthParams.customTables[idx]);
        wtPtr = customSnap ? customSnap.get()
                           : &WavetableLibrary::get().getTable (WavetableLibrary::Basic);
    }
    else
    {
        wtPtr = &WavetableLibrary::get().getTable (shape);
    }
    const auto& wt = *wtPtr;

    const float ghostPos = juce::jlimit (0.0f, 1.0f, pos + 0.07f);

    auto buildPath = [&](float p) {
        juce::Path path;
        const int N = 320;
        for (int n = 0; n < N; ++n)
        {
            const float ph = (float) n / (float) N;
            const float v = wt.sample (p, ph, 2);
            const float x = r.getX() + 4.0f + (r.getWidth() - 8.0f) * (float) n / (float) (N - 1);
            const float y = r.getCentreY() - v * (r.getHeight() - 16.0f) * 0.45f;
            if (n == 0) path.startNewSubPath (x, y);
            else        path.lineTo (x, y);
        }
        return path;
    };

    auto ghost = buildPath (ghostPos);
    g.setColour (Colors::neonCyan.withAlpha (0.15f));
    g.strokePath (ghost, juce::PathStrokeType (1.0f));

    auto wave = buildPath (pos);
    for (int i = 4; i > 0; --i)
    {
        g.setColour (Colors::neonCyan.withAlpha (0.10f * (float) i));
        g.strokePath (wave, juce::PathStrokeType ((float) i + 1.0f));
    }
    g.setColour (Colors::neonCyan);
    g.strokePath (wave, juce::PathStrokeType (1.6f));

    g.setColour (Colors::textBright);
    g.setFont (Fonts::label());
    juce::String label = juce::String (WavetableLibrary::shapeName (shape)).toUpperCase();
    if (shape == WavetableLibrary::Custom)
    {
        const auto custom = processor.getCustomWavetableName (idx);
        label = custom.isNotEmpty() ? ("CUSTOM  /  " + custom.toUpperCase()) : "CUSTOM  /  (DROP .WAV)";
    }
    g.drawText (label,
                (int) r.getX() + 10, (int) r.getY() + 6, (int) r.getWidth() - 80, 16, juce::Justification::left);

    g.setColour (Colors::neonPink);
    g.drawText (juce::String (juce::roundToInt (pos * 100.0f)) + "%",
                (int) r.getRight() - 60, (int) r.getY() + 6, 50, 16, juce::Justification::right);

    g.setColour (Colors::textDim);
    g.setFont (Fonts::small());
    g.drawText ("DRAG TO SCRUB  /  DROP .WAV  /  RIGHT-CLICK",
                (int) r.getX() + 10, (int) r.getBottom() - 18, (int) r.getWidth() - 20, 14,
                juce::Justification::right);

    if (dragHover)
    {
        g.setColour (Colors::neonGreen.withAlpha (0.20f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Colors::neonGreen);
        g.drawRoundedRectangle (r, 6.0f, 2.5f);
        g.setFont (Fonts::subheader());
        g.drawText ("DROP TO LOAD WAVETABLE", r.toNearestInt(), juce::Justification::centred);
    }

    const float ix = r.getX() + 4.0f + (r.getWidth() - 8.0f) * pos;
    g.setColour (Colors::neonPink.withAlpha (0.7f));
    g.drawVerticalLine ((int) ix, r.getY() + 24.0f, r.getBottom() - 6.0f);
}

void WavetableDisplay::setPositionFromMouse (const juce::MouseEvent& e)
{
    auto r = getLocalBounds().reduced (2);
    const float x01 = juce::jlimit (0.0f, 1.0f, (float) (e.x - r.getX()) / (float) juce::jmax (1, r.getWidth()));
    if (auto* p = apvts.getParameter ("osc" + juce::String (idx + 1) + "_pos"))
        p->setValueNotifyingHost (x01);
}

void WavetableDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showLoadMenu(); return; }
    setPositionFromMouse (e);
}
void WavetableDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    setPositionFromMouse (e);
}
void WavetableDisplay::mouseDoubleClick (const juce::MouseEvent&)
{
    if (auto* p = apvts.getParameter ("osc" + juce::String (idx + 1) + "_pos"))
        p->setValueNotifyingHost (p->getDefaultValue());
}
