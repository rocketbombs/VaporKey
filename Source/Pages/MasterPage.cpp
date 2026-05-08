#include "MasterPage.h"
#include "EditorLayout.h"
#include "../PluginProcessor.h"
#include "../LookAndFeel.h"
#include "../Presets.h"

using namespace VK;
using namespace VKEditorLayout;

int MasterPage::PresetListModel::getNumRows()
{
    return (int) owner.visibleRows.size();
}

void MasterPage::PresetListModel::paintListBoxItem (int row, juce::Graphics& g,
                                                    int width, int height, bool selected)
{
    if (row < 0 || row >= (int) owner.visibleRows.size()) return;
    const int entryIdx = owner.visibleRows[(size_t) row];
    if (entryIdx < 0 || entryIdx >= (int) owner.entries.size()) return;
    const auto& entry = owner.entries[(size_t) entryIdx];

    if (selected)
    {
        g.setColour (Colors::neonPink.withAlpha (0.30f));
        g.fillRect (0, 0, width, height);
        g.setColour (Colors::neonPink);
        g.fillRect (0, 0, 4, height);
    }
    else
    {
        g.setColour (Colors::panelHi.withAlpha (0.4f));
        g.fillRect (0, height - 1, width, 1);
    }

    g.setFont (Fonts::small());
    g.setColour (entry.isFactory ? Colors::neonCyan.withAlpha (0.85f)
                                 : Colors::neonAmber.withAlpha (0.85f));
    g.drawText (entry.isFactory ? "F" : "U", 12, 0, 18, height, juce::Justification::centred);

    g.setColour (selected ? Colors::textBright
                          : (entry.isFactory ? Colors::text : Colors::neonAmber.brighter (0.4f)));
    g.setFont (Fonts::preset());
    const int nameRight = entry.isFactory ? (width - 130) : (width - 56);
    g.drawText (entry.name, 36, 0, juce::jmax (40, nameRight - 36), height, juce::Justification::centredLeft);

    if (entry.isFactory && entry.category >= 0 && entry.category < VKPresets::NumCategories)
    {
        g.setFont (Fonts::small());
        g.setColour (Colors::neonCyan.withAlpha (0.6f));
        g.drawText (juce::String (VKPresets::categoryShortName (entry.category)).toUpperCase(),
                    width - 120, 0, 50, height, juce::Justification::centredRight);
    }

    if (selected)
    {
        g.setColour (Colors::neonPink);
        g.setFont (Fonts::small());
        g.drawText ("PLAYING", width - 80, 0, 70, height, juce::Justification::centredRight);
    }
}

void MasterPage::PresetListModel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int) owner.visibleRows.size()) return;
    owner.loadEntry (owner.visibleRows[(size_t) row]);
}

void MasterPage::rebuildEntries()
{
    entries.clear();

    const auto& factory = VKPresets::all();
    for (const auto& p : factory)
        entries.push_back ({ juce::String (p.name), true, p.category });

    for (const auto& n : proc.getUserPresetNames())
        entries.push_back ({ n, false, -1 });

    rebuildVisible();
}

void MasterPage::rebuildVisible()
{
    visibleRows.clear();
    // categoryFilter item IDs:
    //   1            = All
    //   2..1+N       = factory category
    //   2+N          = User
    const int sel = categoryFilter.getSelectedId();
    const int N   = (int) VKPresets::NumCategories;

    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries[i];
        bool match = false;
        if (sel <= 0 || sel == 1) match = true;
        else if (sel == 2 + N)    match = ! e.isFactory;
        else                      match = e.isFactory && e.category == (sel - 2);

        if (match) visibleRows.push_back ((int) i);
    }

    presetList.updateContent();
    presetList.repaint();
}

int MasterPage::findEntryIndex (const juce::String& name, bool isFactory) const
{
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].isFactory == isFactory && entries[i].name == name)
            return (int) i;
    return -1;
}

int MasterPage::visibleRowFromEntryIndex (int entryIndex) const
{
    for (size_t i = 0; i < visibleRows.size(); ++i)
        if (visibleRows[i] == entryIndex) return (int) i;
    return -1;
}

void MasterPage::loadEntry (int idx)
{
    if (idx < 0 || idx >= (int) entries.size()) return;
    const auto& e = entries[(size_t) idx];
    if (e.isFactory)
    {
        const auto names = VaporKeyAudioProcessor::factoryPresetNames();
        const int fi = names.indexOf (e.name);
        if (fi >= 0) proc.loadFactoryPreset (fi);
    }
    else
    {
        proc.loadUserPresetByName (e.name);
    }
    const int row = visibleRowFromEntryIndex (idx);
    if (row >= 0) presetList.selectRow (row);
    refreshNowPlaying();
    nameField.setText (e.isFactory ? juce::String() : e.name, juce::dontSendNotification);
}

void MasterPage::refreshNowPlaying()
{
    const juce::String name = proc.currentPresetName.isNotEmpty()
                                ? proc.currentPresetName
                                : juce::String ("(unnamed)");
    presetNowLabel.setText (name, juce::dontSendNotification);
    const int idx = findEntryIndex (proc.currentPresetName, proc.currentPresetIsFactory);
    if (idx >= 0)
    {
        const int row = visibleRowFromEntryIndex (idx);
        if (row >= 0) presetList.selectRow (row, false, true);
    }
}

void MasterPage::stepPreset (int dir)
{
    const int n = (int) visibleRows.size();
    if (n == 0) return;
    int curRow = juce::jmax (0, presetList.getSelectedRow());
    curRow = (curRow + dir + n) % n;
    loadEntry (visibleRows[(size_t) curRow]);
}

void MasterPage::showStatus (const juce::String& msg, juce::Colour col)
{
    presetLabel.setText (msg, juce::dontSendNotification);
    presetLabel.setColour (juce::Label::textColourId, col);
    juce::Component::SafePointer<MasterPage> sp (this);
    juce::Timer::callAfterDelay (1800, [sp]
    {
        if (auto* p = sp.getComponent())
        {
            p->presetLabel.setText ("PRESETS", juce::dontSendNotification);
            p->presetLabel.setColour (juce::Label::textColourId, Colors::neonPink);
        }
    });
}

void MasterPage::onSave()
{
    auto name = nameField.getText().trim();
    if (name.isEmpty())
    {
        int n = 1;
        for (;; ++n)
        {
            auto candidate = "User Preset " + juce::String (n);
            if (! proc.getUserPresetNames().contains (candidate))
            {
                name = candidate;
                break;
            }
        }
    }
    if (proc.saveUserPreset (name))
    {
        rebuildEntries();
        const int idx = findEntryIndex (proc.currentPresetName, false);
        if (idx >= 0)
        {
            const int row = visibleRowFromEntryIndex (idx);
            if (row >= 0) presetList.selectRow (row);
        }
        refreshNowPlaying();
        nameField.setText (name, juce::dontSendNotification);
        showStatus ("SAVED  -  " + name.toUpperCase(), Colors::neonGreen);
    }
    else
    {
        showStatus ("SAVE FAILED", Colors::neonAmber);
    }
}

void MasterPage::onRename()
{
    const int row = presetList.getSelectedRow();
    if (row < 0 || row >= (int) visibleRows.size()) { showStatus ("SELECT A PRESET", Colors::neonAmber); return; }
    const int entryIdx = visibleRows[(size_t) row];
    const auto& e = entries[(size_t) entryIdx];
    if (e.isFactory) { showStatus ("CANNOT RENAME FACTORY", Colors::neonAmber); return; }

    const auto newName = nameField.getText().trim();
    if (newName.isEmpty()) { showStatus ("ENTER A NEW NAME", Colors::neonAmber); return; }

    if (proc.renameUserPreset (e.name, newName))
    {
        rebuildEntries();
        const int idx = findEntryIndex (newName, false);
        if (idx >= 0)
        {
            const int newRow = visibleRowFromEntryIndex (idx);
            if (newRow >= 0) presetList.selectRow (newRow);
        }
        refreshNowPlaying();
        showStatus ("RENAMED", Colors::neonGreen);
    }
    else
    {
        showStatus ("RENAME FAILED", Colors::neonAmber);
    }
}

void MasterPage::onDelete()
{
    const int row = presetList.getSelectedRow();
    if (row < 0 || row >= (int) visibleRows.size()) { showStatus ("SELECT A PRESET", Colors::neonAmber); return; }
    const int entryIdx = visibleRows[(size_t) row];
    const auto& e = entries[(size_t) entryIdx];
    if (e.isFactory) { showStatus ("CANNOT DELETE FACTORY", Colors::neonAmber); return; }

    const auto deletedName = e.name;
    if (proc.deleteUserPreset (deletedName))
    {
        rebuildEntries();
        if (proc.currentPresetName == deletedName)
            presetNowLabel.setText ("(unnamed)", juce::dontSendNotification);
        nameField.setText ({}, juce::dontSendNotification);
        showStatus ("DELETED", Colors::neonGreen);
    }
    else
    {
        showStatus ("DELETE FAILED", Colors::neonAmber);
    }
}

MasterPage::MasterPage (VaporKeyAudioProcessor& p) : proc (p)
{
    gain  = std::make_unique<VaporKnob> (p.apvts, "gain",  "Master"); addAndMakeVisible (*gain);
    width = std::make_unique<VaporKnob> (p.apvts, "width", "Width");  addAndMakeVisible (*width);
    meter = std::make_unique<LevelMeter> (p);                          addAndMakeVisible (*meter);
    scope = std::make_unique<Scope>      (p);                          addAndMakeVisible (*scope);

    presetLabel.setText ("PRESETS", juce::dontSendNotification);
    presetLabel.setFont (Fonts::section());
    presetLabel.setColour (juce::Label::textColourId, Colors::neonPink);
    presetLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (presetLabel);

    if (proc.currentPresetName.isEmpty())
    {
        proc.currentPresetName = VaporKeyAudioProcessor::factoryPresetNames()[proc.getCurrentProgram()];
        proc.currentPresetIsFactory = true;
    }
    presetNowLabel.setText (proc.currentPresetName, juce::dontSendNotification);
    presetNowLabel.setFont (Fonts::header());
    presetNowLabel.setColour (juce::Label::textColourId, Colors::neonCyan);
    presetNowLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (presetNowLabel);

    presetModel = std::make_unique<PresetListModel> (*this);
    presetList.setModel (presetModel.get());
    presetList.setRowHeight (30);
    presetList.setColour (juce::ListBox::backgroundColourId, Colors::bg.darker (0.2f));
    presetList.setColour (juce::ListBox::outlineColourId,    Colors::neonCyan.withAlpha (0.45f));
    presetList.setOutlineThickness (1);
    addAndMakeVisible (presetList);

    categoryLabel.setText ("CATEGORY", juce::dontSendNotification);
    categoryLabel.setFont (Fonts::section());
    categoryLabel.setColour (juce::Label::textColourId, Colors::neonCyan);
    categoryLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (categoryLabel);

    categoryFilter.addItem ("All", 1);
    {
        const auto cats = VKPresets::categoryNames();
        for (int i = 0; i < cats.size(); ++i)
            categoryFilter.addItem (cats[i], 2 + i);
        categoryFilter.addItem ("User", 2 + cats.size());
    }
    categoryFilter.setSelectedId (1, juce::dontSendNotification);
    categoryFilter.onChange = [this] { rebuildVisible(); refreshNowPlaying(); };
    addAndMakeVisible (categoryFilter);

    rebuildEntries();
    {
        const int idx = findEntryIndex (proc.currentPresetName, proc.currentPresetIsFactory);
        if (idx >= 0)
        {
            const int row = visibleRowFromEntryIndex (idx);
            if (row >= 0) presetList.selectRow (row);
        }
    }

    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (+1); };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);

    nameLabel.setText ("NAME", juce::dontSendNotification);
    nameLabel.setFont (Fonts::section());
    nameLabel.setColour (juce::Label::textColourId, Colors::neonCyan);
    nameLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (nameLabel);

    nameField.setFont (Fonts::preset());
    nameField.setColour (juce::TextEditor::backgroundColourId, Colors::panelHi2);
    nameField.setColour (juce::TextEditor::textColourId, Colors::textBright);
    nameField.setColour (juce::TextEditor::outlineColourId, Colors::neonCyan.withAlpha (0.5f));
    nameField.setColour (juce::TextEditor::focusedOutlineColourId, Colors::neonPink);
    nameField.setColour (juce::TextEditor::highlightColourId, Colors::neonPink.withAlpha (0.35f));
    nameField.setTextToShowWhenEmpty ("Type a name and SAVE", Colors::textDim);
    nameField.setIndents (8, 4);
    nameField.onReturnKey = [this] { onSave(); };
    addAndMakeVisible (nameField);

    saveBtn.onClick   = [this] { onSave(); };
    renameBtn.onClick = [this] { onRename(); };
    deleteBtn.onClick = [this] { onDelete(); };
    addAndMakeVisible (saveBtn);
    addAndMakeVisible (renameBtn);
    addAndMakeVisible (deleteBtn);

    brand.setText ("VAPORKEY", juce::dontSendNotification);
    brand.setFont (Fonts::header());
    brand.setColour (juce::Label::textColourId, Colors::neonPink);
    brand.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (brand);

    tagline.setText ("Wavetable synthesizer with analog warmth.", juce::dontSendNotification);
    tagline.setFont (Fonts::label());
    tagline.setColour (juce::Label::textColourId, Colors::neonCyan);
    tagline.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (tagline);

    copy.setText ("v" + juce::String (JucePlugin_VersionString) + "   /   3 wavetable osc + sub + noise   /   16-voice poly\n"
                  "ADSR amp/mod, pitch env, 2 LFOs, 4 macros\n"
                  "Distortion, Chorus, Phaser, EQ, Delay, Reverb, Comp\n"
                  "Grit / Vibe / Drift / Sat   -   user presets supported\n"
                  "Built with JUCE.   /   RocketBombs",
                  juce::dontSendNotification);
    copy.setFont (Fonts::value());
    copy.setColour (juce::Label::textColourId, Colors::textDim);
    copy.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (copy);
}

void MasterPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (10);
    const int leftW = (int) (r.getWidth() * 0.42);
    juce::Rectangle<int> sLeft  (r.getX(),                  r.getY(), leftW - 5,             r.getHeight());
    juce::Rectangle<int> sRight (r.getX() + leftW + 5,      r.getY(), r.getWidth() - leftW - 5, r.getHeight());
    drawSectionBg (g, sLeft,  Colors::neonPink, "PRESETS");
    drawSectionBg (g, sRight, Colors::neonCyan, "MASTER  /  ABOUT");
}

void MasterPage::resized()
{
    auto r = getLocalBounds().reduced (10);
    const int leftW = (int) (r.getWidth() * 0.42);
    juce::Rectangle<int> sLeft  (r.getX(),                  r.getY(), leftW - 5,             r.getHeight());
    juce::Rectangle<int> sRight (r.getX() + leftW + 5,      r.getY(), r.getWidth() - leftW - 5, r.getHeight());

    {
        auto a = sLeft; a.removeFromTop (kSectionTitleH); a.reduce (16, 12);

        presetLabel.setBounds (a.removeFromTop (18));
        a.removeFromTop (4);
        presetNowLabel.setBounds (a.removeFromTop (40));
        a.removeFromTop (6);

        auto catRow = a.removeFromTop (28);
        categoryLabel.setBounds (catRow.removeFromLeft (90));
        categoryFilter.setBounds (catRow.reduced (2, 1));
        a.removeFromTop (8);

        auto btnRow = a.removeFromTop (32);
        prevBtn.setBounds (btnRow.removeFromLeft (110));
        btnRow.removeFromLeft (8);
        nextBtn.setBounds (btnRow.removeFromLeft (110));
        a.removeFromTop (8);

        const int controlsH = 24 + 32 + 8 + 32;
        auto bottom = a.removeFromBottom (controlsH);

        presetList.setBounds (a);

        nameLabel.setBounds (bottom.removeFromTop (24));
        nameField.setBounds (bottom.removeFromTop (32));
        bottom.removeFromTop (8);
        auto br = bottom.removeFromTop (32);
        const int bw = (br.getWidth() - 16) / 3;
        saveBtn  .setBounds (br.removeFromLeft (bw)); br.removeFromLeft (8);
        renameBtn.setBounds (br.removeFromLeft (bw)); br.removeFromLeft (8);
        deleteBtn.setBounds (br.removeFromLeft (bw));
    }

    {
        auto a = sRight; a.removeFromTop (kSectionTitleH); a.reduce (16, 12);
        brand.setBounds (a.removeFromTop (40));
        tagline.setBounds (a.removeFromTop (22));
        a.removeFromTop (10);

        scope->setBounds (a.removeFromTop (110));
        a.removeFromTop (10);

        auto knobRow = a.removeFromTop (140);
        auto meterArea = knobRow.removeFromRight (52);
        meter->setBounds (meterArea.reduced (4, 6));
        knobRow.removeFromRight (8);
        const int kw = juce::jmin (170, knobRow.getWidth() / 2);
        gain ->setBounds (knobRow.removeFromLeft (kw));
        knobRow.removeFromLeft (8);
        width->setBounds (knobRow.removeFromLeft (kw));

        a.removeFromTop (12);
        copy.setBounds (a);
    }
}
