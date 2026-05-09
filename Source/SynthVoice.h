#pragma once
#include <JuceHeader.h>
#include "Wavetable.h"

struct SynthParams;

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

    int getCurrentNote() const noexcept { return currentNote; }

    // Asks the next startNote to leave amp / mod / pitch envelopes alone, so a
    // mono-legato transition glides into the new note without re-attacking.
    // The flag is consumed (cleared) inside startNote.
    void setLegatoSkipEnvRetrigger (bool b) noexcept { legatoSkipEnvRetrigger = b; }

    // Arm a legato hand-off. Consumed inside stopNote (when the synth
    // delivers the noteOff for the previously held note): instead of
    // releasing envelopes, the voice clears its JUCE currentlyPlayingNote
    // (so findFreeVoice picks it up for the matching same-sample noteOn)
    // and sets the skip-envelope-retrigger flag for that startNote. Net
    // effect: pitch glides on the same voice with envelopes / phases /
    // filter state intact.
    //
    // We must defer the clear until stopNote runs - clearing synchronously
    // from the message thread would silence the voice for the samples
    // between block start and the actual noteOff sample position, since
    // renderNextBlock early-returns when isVoiceActive() is false.
    void armLegatoTransition() noexcept { legatoArmed = true; }

private:
    struct OscState
    {
        std::array<float, 7> uPhases {}; // unison voices
        float driftPhase = 0.0f;
    };

    SynthParams& params;
    double sr = 44100.0;

    OscState osc[3];
    float subPhase = 0.0f;

    // Pink/brown noise state - one chain per output channel so left and
    // right are decorrelated without resorting to mixing in unfiltered
    // (aliasing-prone) white noise on one side.
    float pinkBL[7] {};
    float pinkBR[7] {};
    float brownStateL = 0.0f;
    float brownStateR = 0.0f;

    // Envelopes
    juce::ADSR ampEnv;
    juce::ADSR modEnv;

    // Pitch envelope (custom: simple exp decay from 1 -> 0 over decay time, scaled by amount)
    float pEnvLevel = 0.0f;
    float pEnvDecayCoef = 0.999f;

    // Per-voice filter (state variable, stereo)
    juce::dsp::StateVariableTPTFilter<float> filterL, filterR;

    // LFOs (per voice for proper retrigger if desired; we still drive shape from params)
    float lfoPhase[2] { 0.0f, 0.0f };
    float shState[2] { 0.0f, 0.0f };
    float shTimer[2] { 0.0f, 0.0f };

    int   currentNote = -1;
    float baseFreqTarget = 440.0f;
    float baseFreqCurrent = 440.0f;
    float glideCoef = 1.0f; // per-sample
    float velocityNorm = 1.0f;
    bool  legatoSkipEnvRetrigger = false;
    bool  legatoArmed = false;  // see armLegatoTransition()

    // Previous-sample state for first-order Antiderivative Anti-Aliasing
    // (ADAA) on the two fastTanh saturation stages (filter drive + analog
    // sat). Per-voice / per-channel: each voice runs its own saturator.
    // Initial value 0 is fine - the first sample after a long silence is
    // treated as a step from 0, which ADAA correctly bandlimits.
    float adaaFiltDriveL = 0.0f, adaaFiltDriveR = 0.0f;
    float adaaSatL       = 0.0f, adaaSatR       = 0.0f;

    juce::Random rng;

    static float nextLfo (int shape, float phase, float& shStateVal, float& shTimerVal,
                          float incPerSample, juce::Random& rng);
    static inline float fastTanh (float x) noexcept;
    // Antiderivative F(x) of fastTanh. Constant of integration is
    // irrelevant - ADAA only ever uses differences.
    static inline float fastTanhAntideriv (float x) noexcept;
    // First-order ADAA on fastTanh. `xPrev` is read AND written - the
    // caller stores the per-voice state across samples.
    static inline float fastTanhADAA (float x, float& xPrev) noexcept;
};
