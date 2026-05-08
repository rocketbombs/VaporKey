#pragma once
#include <JuceHeader.h>
#include "Parameters.h"

// Hardware-style arpeggiator. Owns its held-note tables, latch buffer, the
// step counter / sample-accurate timing state, and the small scratch buffers
// it needs to avoid heap allocation on the audio thread.
//
// process() consumes incoming note-on / note-off events and emits a stepped
// sequence into the same MidiBuffer. Non-note events are passed through
// untouched. When arp is off the input stream is forwarded verbatim.
class Arpeggiator
{
public:
    Arpeggiator();

    // Sets sample rate and pre-reserves all scratch buffers. Safe to call
    // repeatedly; cheap on subsequent calls.
    void prepare (double sampleRate);

    // Run for one audio block. Reads/writes 'midi' in place. 'bpm' should be
    // the host tempo for the current block (the engine snapshots this before
    // calling).
    void process (juce::MidiBuffer& midi, int numSamples,
                  const SynthParams& params, double bpm);

private:
    struct HeldNote { int note; int velocity; };

    // Step-picking helper: returns the (note, velocity) for the current step
    // index given the active source notes.
    HeldNote pickStep (const juce::Array<HeldNote>& source, int mode, int numOct);

    double sr = 44100.0;

    juce::Array<HeldNote> held;     // notes physically held by user
    juce::Array<HeldNote> latched;  // latch buffer (active only when latch is on)
    bool   wasOn         = false;
    int    stepIdx       = 0;       // monotonic step counter (cycles pattern)
    int    octOffset     = 0;       // current octave offset (multiples of 12)
    double samplesToStep = 0.0;     // samples until next step boundary
    int    samplesToOff  = -1;      // samples until current note ends (-1 = inactive)
    int    currentNote   = -1;      // currently sounding arp note (-1 = none)
    int    currentChan   = 1;       // channel of currently sounding note
    juce::Random rng;

    // Scratch buffers reused every block - reserved in prepare() so the audio
    // thread never hits malloc.
    juce::MidiBuffer                  passBuf;
    juce::Array<juce::MidiMessage>    noteEventsBuf;
    juce::Array<int>                  noteSamplesBuf;
    juce::Array<HeldNote>             activeBuf;
    juce::Array<HeldNote>             orderedBuf;
};
