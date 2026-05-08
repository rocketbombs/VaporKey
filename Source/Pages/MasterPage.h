#pragma once
#include <JuceHeader.h>
#include "../Widgets/VaporWidgets.h"
#include "../Widgets/Meters.h"

class VaporKeyAudioProcessor;

class MasterPage : public juce::Component
{
public:
    explicit MasterPage (VaporKeyAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;

    // Combined factory + user preset listing.
    struct PresetEntry { juce::String name; bool isFactory; int category; };
    std::vector<PresetEntry> entries;
    std::vector<int>         visibleRows;
    void rebuildEntries();
    void rebuildVisible();
    int  findEntryIndex (const juce::String& name, bool isFactory) const;
    int  visibleRowFromEntryIndex (int entryIndex) const;
    void loadEntry (int index);
    void refreshNowPlaying();

private:
    VaporKeyAudioProcessor& proc;
    std::unique_ptr<VaporKnob> gain, width;
    std::unique_ptr<LevelMeter> meter;
    std::unique_ptr<Scope>      scope;
    juce::ListBox presetList;
    juce::TextButton prevBtn { "<  PREV" }, nextBtn { "NEXT  >" };
    juce::TextButton saveBtn { "SAVE" }, renameBtn { "RENAME" }, deleteBtn { "DELETE" };
    juce::TextEditor nameField;
    juce::Label presetLabel, presetNowLabel, nameLabel, brand, tagline, copy;
    juce::ComboBox categoryFilter;
    juce::Label    categoryLabel;

    class PresetListModel : public juce::ListBoxModel
    {
    public:
        explicit PresetListModel (MasterPage& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    private:
        MasterPage& owner;
    };
    std::unique_ptr<PresetListModel> presetModel;

    void onSave();
    void onRename();
    void onDelete();
    void stepPreset (int dir);
    void showStatus (const juce::String& msg, juce::Colour col);
};
