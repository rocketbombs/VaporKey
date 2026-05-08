#pragma once
#include <JuceHeader.h>
#include "Parameters.h"

// Voice rendering coordinator. Owns the juce::Synthesiser and the WTVoice
// instances that render audio for it; everything voice-side is reachable
// through this single object so the audio processor doesn't need to know
// about WTVoice / WTSound or the per-voice prepare protocol.
class SynthEngine
{
public:
    static constexpr int kNumVoices = 16;

    // Builds the voice pool. The SynthParams reference must outlive the
    // engine: each voice holds a non-owning reference to it for its entire
    // lifetime. The pointers inside SynthParams are allowed to be null at
    // this point (they will be populated by Parameters::cache before the
    // first render).
    explicit SynthEngine (SynthParams& params);

    // Voice-side prepare (sample rate + filter spec). Safe to call repeatedly.
    void prepare (double sampleRate);

    // Run one audio block. The synth.renderNextBlock signature requires the
    // buffer to be cleared by the caller before this is invoked.
    void renderBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    // Force every active voice into release / clear state. Used during preset
    // switches so the in-flight envelopes don't ride the new patch's gain.
    void allNotesOff();

    // Mono+legato hand-off: tag every currently-active voice so the next
    // startNote (which will be on one of those voices, via stealing) leaves
    // the envelopes running and just glides into the new pitch. Idle voices
    // are intentionally not flagged so the flag can't leak into a future
    // poly note.
    void markVoicesLegato();

private:
    juce::Synthesiser synth;
};
