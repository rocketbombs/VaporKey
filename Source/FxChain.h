#pragma once
#include <JuceHeader.h>
#include "Parameters.h"

// Per-block effects chain run after voice rendering: distortion -> chorus ->
// phaser -> EQ -> delay -> reverb -> width/gain -> compressor.
//
// Owns the JUCE DSP objects and the cached EQ-coefficient sentinels so the
// audio thread never builds a coefficient ReferenceCountedObject unless the
// underlying parameter actually changed.
class FxChain
{
public:
    explicit FxChain (SynthParams& sp);

    void prepare (double sampleRate, int samplesPerBlock);

    // Process the buffer in place. `currentBpm` is consulted only when delay
    // sync is on.
    void process (juce::AudioBuffer<float>& buffer, double currentBpm);

    // Flush all effect tails. Used when loading a preset so the old delay /
    // reverb feedback can't ride the new patch's gain or filter values.
    void reset();

private:
    SynthParams& params;
    double sr = 44100.0;

    juce::dsp::Chorus<float> chorusFx;
    juce::dsp::Phaser<float> phaserFx;
    juce::dsp::Compressor<float> compFx;
    juce::dsp::IIR::Filter<float> eqLowL, eqLowR, eqMidL, eqMidR, eqHighL, eqHighR;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL { 192000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayR { 192000 };
    juce::Reverb reverbFx;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delaySmoothedL, delaySmoothedR;

    // Cached EQ inputs so we only rebuild the IIR coefficients when something
    // actually changes (the JUCE make* helpers allocate every call). The
    // dirty flags force a fresh build on the first audible block and after
    // every prepare(); subsequent blocks rebuild only when the corresponding
    // parameter has moved by more than a small epsilon.
    bool  eqLowDirty  = true;
    bool  eqMidDirty  = true;
    bool  eqHighDirty = true;
    float prevEqLowG  = 0.0f;
    float prevEqMidG  = 0.0f;
    float prevEqMidF  = 0.0f;
    float prevEqHighG = 0.0f;
};
