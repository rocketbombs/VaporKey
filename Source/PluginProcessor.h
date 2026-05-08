#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "Arpeggiator.h"

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
    void updateMacroSums();
    void filterMidi (juce::MidiBuffer& midi);
    void markVoicesLegato();

    // Silence active voices and clear FX state. Used when loading a preset so
    // the old voices/FX tails don't ride the new parameter values and produce
    // a loud burst (filter cracks, delay/reverb feedback into the new gain).
    void silenceForPresetSwitch();

    juce::Synthesiser synth;

    // FX
    juce::dsp::Chorus<float> chorusFx;
    juce::dsp::Phaser<float> phaserFx;
    juce::dsp::Compressor<float> compFx;
    juce::dsp::IIR::Filter<float> eqLowL, eqLowR, eqMidL, eqMidR, eqHighL, eqHighR;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL { 192000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayR { 192000 };
    juce::Reverb reverbFx;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delaySmoothedL, delaySmoothedR;

    double sr = 44100.0;
    double currentBpm = 120.0;
    int currentProgram = 0;

    // Mono mode helpers
    juce::Array<int> monoHeldNotes;

    // Arpeggiator (owns its scratch buffers + held / latched note tables).
    Arpeggiator arp;

    // Pre-allocated MidiBuffer for filterMidi - keeps the audio thread off
    // malloc when host instances pile up.
    juce::MidiBuffer monoFilterBuf;

    // Cached EQ coefficient inputs so we only rebuild the IIR coefficients
    // when something actually changes (the JUCE make* helpers allocate a
    // ReferenceCountedObject every call - lethal on the audio thread).
    float  prevEqLowG  = 1.0e9f;  // sentinel: forces first build
    float  prevEqMidG  = 1.0e9f;
    float  prevEqMidF  = 1.0e9f;
    float  prevEqHighG = 1.0e9f;

    // Custom wavetable file paths (kept in apvts state for persistence).
    juce::String customWavPath[3];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VaporKeyAudioProcessor)
};
