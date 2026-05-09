#pragma once
#include <JuceHeader.h>
#include "Parameters.h"

// Voice-rendering coordinator. Owns the JUCE Synthesiser + WTSound + WTVoice
// instances, the mono/legato bookkeeping, and the MIDI-controller scan that
// stores live mod-wheel / pitch-bend / aftertouch into SynthParams atomics.
//
// Per block:
//   1. caller already ran the arpeggiator over `midi` (it transforms note
//      events but lets CC/pitch-bend/aftertouch pass through);
//   2. SynthEngine::process scans the buffer (filterMidi), then sums macros
//      (so CC1/aftertouch from this block reach modSum), then asks the
//      Synthesiser to render voices into `buffer`.
//
// Macros are summed *after* the MIDI scan so a controller event arriving in
// the current block reaches voices in the same callback.
class SynthEngine
{
public:
    explicit SynthEngine (SynthParams& sp);

    void prepare (double sampleRate, int samplesPerBlock);

    // Buffer is cleared and filled. `midi` is consumed (filtered for mono/
    // legato in place). Caller has already run the arpeggiator.
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    // Used during preset switch to flush in-flight envelopes / voice state.
    void allNotesOff();

    juce::Synthesiser& synth() { return synthesiser; }

private:
    void filterMidi (juce::MidiBuffer& midi);
    void updateMacroSums();

    // Hand off the currently-sounding voice to the next noteOn so the
    // mono-legato transition glides on the same voice (envelopes, phases,
    // and filter state preserved). Returns true if a voice was handed off.
    bool prepareLegatoTransition();

    SynthParams& params;
    juce::Synthesiser synthesiser;

    // Mono mode helpers
    juce::Array<int> monoHeldNotes;
    juce::MidiBuffer monoFilterBuf;
};
