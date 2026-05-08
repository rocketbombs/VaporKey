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

    // Canonical path to the user-presets directory. Pure - this does NOT
    // create the directory on disk. Read-only callers (list / read / delete /
    // rename) use this so simply opening the editor does not leave behind an
    // empty RocketBombs/VaporKey/Presets tree on every host.
    juce::File        getUserPresetsDir();

    // Same path as above, but creates the directory if it doesn't already
    // exist. Use this immediately before writing - currently saveUserPreset
    // and any UI that wants to reveal the folder to the user.
    juce::File        ensureUserPresetsDir();

    juce::StringArray getUserPresetNames();

    bool saveUserPreset (juce::AudioProcessorValueTreeState& apvts, const juce::String& name);

    // Read and parse a user preset file into an XML tree. Returns nullptr if
    // the file is missing or unparseable. Stateless - no APVTS changes happen
    // here, the caller decides when to silence and apply.
    std::unique_ptr<juce::XmlElement> readUserPresetXml (const juce::String& name);

    bool deleteUserPreset (const juce::String& name);
    bool renameUserPreset (const juce::String& oldName, const juce::String& newName);
}
