#pragma once
#include <JuceHeader.h>
#include "Parameters.h"

// Master FX rack: distortion -> chorus -> phaser -> 3-band EQ -> ping-pong
// delay -> reverb -> stereo width / gain -> optional compressor.
//
// The order is fixed by the original synth's voicing; that's preserved here.
// Each stage observes its own subset of SynthParams and is free to early-out
// when it would be a no-op (mix == 0 etc.) so quiet patches don't pay for
// stages they don't use.
class FxChain
{
public:
    void prepare (double sampleRate, int samplesPerBlock);

    // Drop FX state so a preset switch can't ride the previous tail (delay
    // feedback / reverb cloud / compressor envelope) into the new gain. The
    // EQ-coefficient cache is also reset so the first block at the new rate
    // / new params triggers a rebuild.
    void reset();

    // Apply the chain in place. 'bpm' is used for tempo-synced delay time.
    void process (juce::AudioBuffer<float>& buffer, const SynthParams& params, double bpm);

private:
    static float distort (float x, int type, float drive);

    double sr = 44100.0;

    juce::dsp::Chorus<float> chorusFx;
    juce::dsp::Phaser<float> phaserFx;
    juce::dsp::Compressor<float> compFx;
    juce::dsp::IIR::Filter<float> eqLowL, eqLowR, eqMidL, eqMidR, eqHighL, eqHighR;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL { 192000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayR { 192000 };
    juce::Reverb reverbFx;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delaySmoothedL, delaySmoothedR;

    // Cached EQ coefficient inputs so we only rebuild the IIR coefficients
    // when something actually changes (the JUCE make* helpers allocate a
    // ReferenceCountedObject every call - lethal on the audio thread).
    float prevEqLowG  = 1.0e9f;  // sentinel: forces first build
    float prevEqMidG  = 1.0e9f;
    float prevEqMidF  = 1.0e9f;
    float prevEqHighG = 1.0e9f;
};
