#pragma once
#include <JuceHeader.h>
#include <memory>
#include "Parameters.h"
#include "Arpeggiator.h"
#include "FxChain.h"
#include "SynthEngine.h"

// Audio-reactive UI state. The audio thread writes; the editor's timers read.
// Plain floats are fine for the scope ring buffer (occasional tearing is
// invisible at 60Hz repaint); the index uses a release-store so the editor
// always sees a consistent "most recent sample index".
struct VisData
{
    static constexpr int kScopeSize = 1024;     // power of two for cheap wrap
    static constexpr int kScopeMask = kScopeSize - 1;

    std::atomic<float> peakL { 0.0f };
    std::atomic<float> peakR { 0.0f };
    std::atomic<float> rms   { 0.0f };

    float                 scope[kScopeSize] {};
    std::atomic<uint32_t> scopeWrite { 0 };
};

class VaporKeyAudioProcessor : public juce::AudioProcessor
{
public:
    VaporKeyAudioProcessor();
    ~VaporKeyAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    SynthParams synthParams;
    VisData     vis;

    // External (UI) helpers.
    void loadFactoryPreset (int index);
    static juce::StringArray factoryPresetNames();

    // Replace oscillator i's custom wavetable from a .wav file. Thread-safe to
    // call from the message thread; voices observe the swap atomically.
    bool loadCustomWavetable (int oscIndex, const juce::File& file);
    void clearCustomWavetable (int oscIndex);
    juce::String getCustomWavetableName (int oscIndex) const;

    // User presets (read/written under userApplicationDataDirectory/RocketBombs/VaporKey/Presets).
    juce::File        getUserPresetsDir() const;
    juce::StringArray getUserPresetNames() const;
    bool              saveUserPreset (const juce::String& name);
    bool              loadUserPresetByName (const juce::String& name);
    bool              deleteUserPreset (const juce::String& name);
    bool              renameUserPreset (const juce::String& oldName, const juce::String& newName);

    // Most recently selected preset (for highlight in UI). "" if none.
    juce::String currentPresetName;
    bool         currentPresetIsFactory = true;

private:
    // Silence active voices and clear FX state. Used when loading a preset so
    // the old voices/FX tails don't ride the new parameter values and produce
    // a loud burst (filter cracks, delay/reverb feedback into the new gain).
    void silenceForPresetSwitch();

    SynthEngine engine;
    Arpeggiator arp;
    FxChain     fx;

    double sr = 44100.0;
    double currentBpm = 120.0;
    int currentProgram = 0;

    // The synth + FX path is internally stereo (oscillators have pan, FX is
    // stereo). When the host has selected a mono output bus we render into
    // this scratch buffer and mix L+R down to the host's single channel at
    // the end of processBlock. Allocated once in prepareToPlay so the audio
    // thread only ever clamps the view, never reallocates.
    juce::AudioBuffer<float> stereoScratch;

    // Custom wavetable file paths (kept in apvts state for persistence).
    juce::String customWavPath[3];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VaporKeyAudioProcessor)
};
