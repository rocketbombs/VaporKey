#include "Presets.h"
#include "BinaryData.h"

namespace VKPresets
{

juce::StringArray categoryNames()
{
    return { "Bass", "Lead", "Pad", "Pluck", "Keys", "Bell", "FX", "Arp" };
}

const char* categoryShortName (int c) noexcept
{
    switch (c)
    {
        case Bass:  return "Bass";
        case Lead:  return "Lead";
        case Pad:   return "Pad";
        case Pluck: return "Pluck";
        case Keys:  return "Keys";
        case Bell:  return "Bell";
        case FX:    return "FX";
        case Arp:   return "Arp";
        default:    return "?";
    }
}

namespace {

int categoryFromString (const juce::String& s)
{
    const auto names = categoryNames();
    const int idx = names.indexOf (s, true);
    return idx >= 0 ? idx : (int) Keys;   // Init and unrecognised strings default to Keys
}

std::vector<Preset> parsePresets()
{
    std::vector<Preset> out;

    const auto raw = juce::String::fromUTF8 (BinaryData::Presets_json,
                                             BinaryData::Presets_jsonSize);
    juce::var root = juce::JSON::parse (raw);
    if (! root.isObject()) { jassertfalse; return out; }

    const auto presetsVar = root.getProperty ("presets", juce::var());
    auto* presets = presetsVar.getArray();
    if (presets == nullptr) { jassertfalse; return out; }

    out.reserve ((size_t) presets->size());
    for (const auto& item : *presets)
    {
        if (! item.isObject()) continue;

        Preset p;
        p.name     = item.getProperty ("name", "").toString();
        p.category = categoryFromString (item.getProperty ("category", "").toString());

        auto valuesVar = item.getProperty ("values", juce::var());
        if (auto* obj = valuesVar.getDynamicObject())
        {
            const auto& props = obj->getProperties();
            p.values.reserve ((size_t) props.size());
            for (int i = 0; i < props.size(); ++i)
            {
                const auto& name = props.getName (i);
                const auto& val  = props.getValueAt (i);
                p.values.push_back ({ name.toString(), (float) (double) val });
            }
        }

        out.push_back (std::move (p));
    }

    return out;
}

} // namespace

const std::vector<Preset>& all()
{
    static const std::vector<Preset> list = parsePresets();
    return list;
}

} // namespace VKPresets
