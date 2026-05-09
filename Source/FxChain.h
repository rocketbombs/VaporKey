#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "PlateReverb.h"

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
    // sync is on. Mono input buffers route through an internal stereo scratch
    // buffer and the result is mixed back down: running the stereo FX chain
    // directly on a one-channel buffer with R aliased to L would double-apply
    // per-channel processing (distortion, EQ) and scramble any cross-channel
    // routing (ping-pong delay, M/S width).
    void process (juce::AudioBuffer<float>& buffer, double currentBpm);

    // Flush all effect tails. Used when loading a preset so the old delay /
    // reverb feedback can't ride the new patch's gain or filter values.
    void reset();

private:
    void processStereo (juce::AudioBuffer<float>& buffer, double currentBpm);
    void processDistortionOversampled (juce::AudioBuffer<float>& buffer,
                                       int type, float drive, float mix);

    SynthParams& params;
    double sr = 44100.0;

    juce::dsp::Chorus<float> chorusFx;
    juce::dsp::Phaser<float> phaserFx;
    juce::dsp::Compressor<float> compFx;
    juce::dsp::IIR::Filter<float> eqLowL, eqLowR, eqMidL, eqMidR, eqHighL, eqHighR;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL { 192000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayR { 192000 };

    // 4x polyphase-IIR oversampler wrapped around the distortion stage.
    // The half-band IIR's reported latency is zero (group delay is sub-
    // sample) so we don't need to advertise extra latency to the host.
    // Engaged unconditionally on every block: toggling oversampling with
    // the dist mix would step the chain's phase response under the user's
    // foot, which is audible.
    std::unique_ptr<juce::dsp::Oversampling<float>> distOversampler;

    // Plate reverb (Dattorro-style). Replaces the previous juce::Reverb
    // (FreeVerb) - plates have a denser, smoother tail with a metallic
    // shimmer, and the figure-eight tank produces true stereo
    // de-correlation without any explicit width control.
    PlateReverb plate;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delaySmoothedL, delaySmoothedR;

    // Cached EQ inputs so we only rebuild the IIR coefficients when something
    // actually changes (the JUCE make* helpers allocate every call).
    float prevEqLowG  = 1.0e9f; // sentinel: forces first build
    float prevEqMidG  = 1.0e9f;
    float prevEqMidF  = 1.0e9f;
    float prevEqHighG = 1.0e9f;

    // Stereo scratch used when the host bus is mono. Sized once in prepare()
    // to the host's max block size; the audio thread reads/writes it but
    // never resizes.
    juce::AudioBuffer<float> stereoScratch;
};
