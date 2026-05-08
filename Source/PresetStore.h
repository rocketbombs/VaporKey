#pragma once
#include <JuceHeader.h>

// Preset I/O - both factory presets (compiled-in JSON via Presets.cpp) and
// user presets (XML files under userApplicationDataDirectory). Functions are
// stateless: callers own the APVTS and the "is currently this name?" tracking.
//
// Loading a preset typically requires silencing the synth + flushing FX so
// envelope releases and FX tails do not ride the new patch values. That
// silencing happens in the processor; this module only manipulates the APVTS.
namespace PresetStore
{
    // Factory presets ----------------------------------------------------------

    juce::StringArray factoryPresetNames();

    // Apply factory preset `index` into `apvts`, using `proc` to walk every
    // parameter and reset to defaults first. Returns false if `index` is out of
    // range.
    bool applyFactoryPreset (juce::AudioProcessorValueTreeState& apvts,
                             juce::AudioProcessor& proc,
                             int index);

    int factoryPresetCount();

    // User presets -------------------------------------------------------------

    juce::File        getUserPresetsDir();
    juce::StringArray getUserPresetNames();

    bool saveUserPreset (juce::AudioProcessorValueTreeState& apvts, const juce::String& name);
    bool loadUserPresetByName (juce::AudioProcessorValueTreeState& apvts, const juce::String& name);
    bool deleteUserPreset (const juce::String& name);
    bool renameUserPreset (const juce::String& oldName, const juce::String& newName);
}
