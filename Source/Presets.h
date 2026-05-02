#pragma once
#include <JuceHeader.h>
#include <vector>

// Factory presets are authored in Source/Presets.json (embedded into the
// binary via juce_add_binary_data) and parsed lazily on first access.
namespace VKPresets
{
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

    struct PV {
        juce::String id;
        float        v;
    };

    struct Preset {
        juce::String    name;
        int             category;
        std::vector<PV> values;
    };

    const std::vector<Preset>& all();
}
