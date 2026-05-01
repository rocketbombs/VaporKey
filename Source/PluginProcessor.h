#pragma once
#include <JuceHeader.h>
#include "Wavetable.h"

// Centralised parameter access (read by voices, written by APVTS).
struct SynthParams
{
    // Per-oscillator
    struct OscP {
        std::atomic<float>* on        = nullptr; // 0/1
        std::atomic<float>* shape     = nullptr; // 0..NumShapes-1
        std::atomic<float>* position  = nullptr; // 0..1
        std::atomic<float>* level     = nullptr; // dB
        std::atomic<float>* pan       = nullptr; // -1..1
        std::atomic<float>* coarse    = nullptr; // semis
        std::atomic<float>* fine      = nullptr; // cents
        std::atomic<float>* unison    = nullptr; // 1..7
        std::atomic<float>* detune    = nullptr; // 0..1
    };
    OscP osc[3];

    // Filter
    std::atomic<float>* fCut   = nullptr;
    std::atomic<float>* fRes   = nullptr;
    std::atomic<float>* fEnv   = nullptr; // mod env -> cutoff
    std::atomic<float>* fType  = nullptr; // 0=LP, 1=BP, 2=HP

    // Envelopes
    std::atomic<float>* aA = nullptr; std::atomic<float>* aD = nullptr;
    std::atomic<float>* aS = nullptr; std::atomic<float>* aR = nullptr;
    std::atomic<float>* mA = nullptr; std::atomic<float>* mD = nullptr;
    std::atomic<float>* mS = nullptr; std::atomic<float>* mR = nullptr;

    // LFO
    std::atomic<float>* lfo1Rate = nullptr; std::atomic<float>* lfo1Amt = nullptr;
    std::atomic<float>* lfo2Rate = nullptr; std::atomic<float>* lfo2Amt = nullptr; // -> wt position

    // Analog warmth
    std::atomic<float>* grit  = nullptr; // 0..1 phase jitter amount
    std::atomic<float>* vibe  = nullptr; // 0..1 background noise
    std::atomic<float>* drift = nullptr; // 0..1 slow pitch drift
    std::atomic<float>* sat   = nullptr; // 0..1 soft saturation

    // FX
    std::atomic<float>* chorus = nullptr;
    std::atomic<float>* delay  = nullptr;
    std::atomic<float>* delayTime = nullptr; // seconds
    std::atomic<float>* delayFb   = nullptr;
    std::atomic<float>* reverb    = nullptr;

    // Master
    std::atomic<float>* gain   = nullptr; // dB
    std::atomic<float>* glide  = nullptr; // unused for now
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
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    SynthParams synthParams;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void cacheParams();

    juce::Synthesiser synth;

    // FX
    juce::dsp::Chorus<float> chorusFx;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL { 192000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayR { 192000 };
    juce::dsp::Reverb reverbFx;

    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VaporKeyAudioProcessor)
};
