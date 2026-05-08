#include "PresetStore.h"
#include "Presets.h"

juce::StringArray PresetStore::factoryPresetNames()
{
    juce::StringArray names;
    for (const auto& p : VKPresets::all()) names.add (p.name);
    return names;
}

int PresetStore::factoryPresetCount()
{
    return (int) VKPresets::all().size();
}

bool PresetStore::applyFactoryPreset (juce::AudioProcessorValueTreeState& apvts,
                                      juce::AudioProcessor& proc,
                                      int index)
{
    const auto& list = VKPresets::all();
    if (index < 0 || index >= (int) list.size()) return false;

    // Reset to defaults first by re-creating defaults from parameter ranges.
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
    return true;
}

juce::File PresetStore::getUserPresetsDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("RocketBombs")
                   .getChildFile ("VaporKey")
                   .getChildFile ("Presets");
    if (! dir.exists()) dir.createDirectory();
    return dir;
}

juce::StringArray PresetStore::getUserPresetNames()
{
    juce::StringArray names;
    auto dir = getUserPresetsDir();
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.vkpreset");
    for (auto& f : files) names.add (f.getFileNameWithoutExtension());
    names.sort (false);
    return names;
}

bool PresetStore::saveUserPreset (juce::AudioProcessorValueTreeState& apvts, const juce::String& name)
{
    auto safeName = juce::File::createLegalFileName (name).trim();
    if (safeName.isEmpty()) return false;
    auto file = getUserPresetsDir().getChildFile (safeName + ".vkpreset");
    if (auto xml = apvts.copyState().createXml())
        return xml->writeTo (file);
    return false;
}

std::unique_ptr<juce::XmlElement> PresetStore::readUserPresetXml (const juce::String& name)
{
    auto file = getUserPresetsDir().getChildFile (name + ".vkpreset");
    if (! file.existsAsFile()) return nullptr;
    return juce::XmlDocument::parse (file);
}

bool PresetStore::deleteUserPreset (const juce::String& name)
{
    auto file = getUserPresetsDir().getChildFile (name + ".vkpreset");
    if (! file.existsAsFile()) return false;
    return file.deleteFile();
}

bool PresetStore::renameUserPreset (const juce::String& oldName, const juce::String& newName)
{
    auto safeNew = juce::File::createLegalFileName (newName).trim();
    if (safeNew.isEmpty() || safeNew == oldName) return false;
    auto src = getUserPresetsDir().getChildFile (oldName + ".vkpreset");
    auto dst = getUserPresetsDir().getChildFile (safeNew + ".vkpreset");
    if (! src.existsAsFile() || dst.existsAsFile()) return false;
    return src.moveFileTo (dst);
}
