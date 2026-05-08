#pragma once
#include <JuceHeader.h>
#include <vector>

// Factory + user preset machinery.
//
// Factory presets are authored in Source/Presets.json (embedded into the
// binary at build time via juce_add_binary_data) and parsed lazily on first
// access. User presets live as XML state files under
// userApplicationDataDirectory/RocketBombs/VaporKey/Presets/*.vkpreset.
//
// Everything is namespace-scope free functions; the processor wraps them to
// expose its preset API to the editor.
namespace PresetStore
{
    // -- Categories --
    enum Category {
        Bass = 0,
        Lead,
        Pad,
        Pluck,
        Keys,
        Bell,
        FX,
        Arp,
        NumCategories
    };

    juce::StringArray categoryNames();
    const char*       categoryShortName (int c) noexcept;

    // -- Factory --
    struct PV      { juce::String id; float v; };
    struct Factory { juce::String name; int category; std::vector<PV> values; };

    const std::vector<Factory>& factoryAll();
    juce::StringArray           factoryNames();

    // Resets all parameters to their declared defaults, then applies the named
    // preset's key/value overrides. The caller is responsible for muting voices
    // and clearing FX state first - this only touches the parameter tree.
    void applyFactory (juce::AudioProcessor& proc,
                       juce::AudioProcessorValueTreeState& apvts,
                       int index);

    // -- User preset I/O --
    juce::File        userDir();
    juce::StringArray userList();

    bool userSave   (juce::AudioProcessorValueTreeState& apvts, const juce::String& name);
    bool userLoad   (juce::AudioProcessorValueTreeState& apvts, const juce::String& name);
    bool userDelete (const juce::String& name);
    bool userRename (const juce::String& oldName, const juce::String& newName);
}
