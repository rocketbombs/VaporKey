#include "PresetStore.h"
#include "BinaryData.h"

namespace PresetStore
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
    return idx >= 0 ? idx : (int) Keys;   // Init / unknown -> Keys
}

std::vector<Factory> parseFactoryPresets()
{
    std::vector<Factory> out;

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

        Factory p;
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

const std::vector<Factory>& factoryAll()
{
    static const std::vector<Factory> list = parseFactoryPresets();
    return list;
}

juce::StringArray factoryNames()
{
    juce::StringArray names;
    for (const auto& p : factoryAll()) names.add (p.name);
    return names;
}

void applyFactory (juce::AudioProcessor& proc,
                   juce::AudioProcessorValueTreeState& apvts,
                   int index)
{
    const auto& list = factoryAll();
    if (index < 0 || index >= (int) list.size()) return;

    // Reset to declared defaults first. Doing this from APVTS would skip
    // parameters not in the tree; iterating the AudioProcessor's parameter
    // list catches every RangedAudioParameter regardless.
    for (auto* param : proc.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
            p->setValueNotifyingHost (p->getDefaultValue());

    for (const auto& kv : list[(size_t) index].values)
    {
        if (auto* p = apvts.getParameter (kv.id))
        {
            const auto& range = p->getNormalisableRange();
            const float norm = range.convertTo0to1 (kv.v);
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
        }
    }
}

juce::File userDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("RocketBombs")
                   .getChildFile ("VaporKey")
                   .getChildFile ("Presets");
    if (! dir.exists()) dir.createDirectory();
    return dir;
}

juce::StringArray userList()
{
    juce::StringArray names;
    auto dir = userDir();
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.vkpreset");
    for (auto& f : files) names.add (f.getFileNameWithoutExtension());
    names.sort (false);
    return names;
}

bool userSave (juce::AudioProcessorValueTreeState& apvts, const juce::String& name)
{
    auto safeName = juce::File::createLegalFileName (name).trim();
    if (safeName.isEmpty()) return false;
    auto file = userDir().getChildFile (safeName + ".vkpreset");
    if (auto xml = apvts.copyState().createXml())
        return xml->writeTo (file);
    return false;
}

bool userLoad (juce::AudioProcessorValueTreeState& apvts, const juce::String& name)
{
    auto file = userDir().getChildFile (name + ".vkpreset");
    if (! file.existsAsFile()) return false;
    if (auto xml = juce::XmlDocument::parse (file))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        return true;
    }
    return false;
}

bool userDelete (const juce::String& name)
{
    auto file = userDir().getChildFile (name + ".vkpreset");
    if (! file.existsAsFile()) return false;
    return file.deleteFile();
}

bool userRename (const juce::String& oldName, const juce::String& newName)
{
    auto safeNew = juce::File::createLegalFileName (newName).trim();
    if (safeNew.isEmpty() || safeNew == oldName) return false;
    auto src = userDir().getChildFile (oldName + ".vkpreset");
    auto dst = userDir().getChildFile (safeNew + ".vkpreset");
    if (! src.existsAsFile() || dst.existsAsFile()) return false;
    return src.moveFileTo (dst);
}

} // namespace PresetStore
