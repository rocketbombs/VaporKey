#pragma once
#include <JuceHeader.h>
#include "Wavetable.h"

struct SynthParams; // fwd

class WTSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class WTVoice : public juce::SynthesiserVoice
{
public:
    explicit WTVoice (SynthParams& p);

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<WTSound*> (s) != nullptr; }
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

    void prepare (double sampleRate);

private:
    struct OscState
    {
        float phase = 0.0f;
        float driftPhase = 0.0f;   // slow drift LFO
        std::array<float, 7> uPhases {}; // unison voices
    };

    SynthParams& params;
    double sr = 44100.0;

    OscState osc[3];

    // Envelopes
    juce::ADSR ampEnv;
    juce::ADSR modEnv;
    juce::ADSR::Parameters ampP, modP;

    // Per-voice filter (state variable, stereo)
    juce::dsp::StateVariableTPTFilter<float> filterL, filterR;

    // LFOs
    float lfoPhase[2] { 0.0f, 0.0f };

    float baseFreq = 440.0f;
    float velocityGain = 1.0f;
    bool  noteHeld = false;

    // Random gen for analog jitter (per voice for stereo decorrelation)
    juce::Random rng;
};
