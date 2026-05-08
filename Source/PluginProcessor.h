#pragma once
#include <JuceHeader.h>
#include <memory>
#include "Parameters.h"
#include "Arpeggiator.h"
#include "FxChain.h"
#include "SynthEngine.h"
#include "WavetableImport.h"

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
    // Run a preset transition on the message thread. Holds the audio callback
    // lock across the full sequence required by docs/RealtimeSafety.md:
    //   1. Take getCallbackLock() so processBlock cannot run.
    //   2. Silence active voices (engine.allNotesOff) and flush FX tails
    //      (fx.reset) so old envelope releases and delay/reverb feedback
    //      can't ride the new patch's gain or filter values.
    //   3. Run `apply` (the actual APVTS state swap or factory-preset apply).
    //   4. Release the lock; the next callback runs with clean state.
    // The previous helper (silenceForPresetSwitch) released the lock before
    // step 3, leaving a window between silence and the new state where the
    // callback could run with a clean engine but stale parameters.
    template <typename ApplyFn>
    void applyPresetUnderLock (ApplyFn&& apply)
    {
        const juce::ScopedLock sl (getCallbackLock());
        engine.allNotesOff();
        fx.reset();
        apply();
    }

    SynthEngine engine;
    Arpeggiator arp;
    FxChain     fx;

    double sr = 44100.0;
    double currentBpm = 120.0;
    int currentProgram = 0;

    // Custom wavetable file paths (kept in apvts state for persistence).
    juce::String customWavPath[3];

    // Owns the lifetime of replaced custom wavetables so the audio thread is
    // never the last holder of an old shared_ptr (would otherwise run heap
    // deallocation on the audio path). See WavetableRetirementQueue and
    // docs/RealtimeSafety.md.
    WavetableRetirementQueue wavetableRetire;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VaporKeyAudioProcessor)
};
