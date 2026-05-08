#pragma once
#include <JuceHeader.h>
#include "Parameters.h"

// Standalone arpeggiator: consumes incoming note-on/off events and emits a
// stepped sequence at sample positions inside the current block. Other MIDI
// (CCs, pitch bend, aftertouch) flows through unchanged so downstream MIDI
// scanning still sees it.
//
// All scratch buffers are reserved in prepare() so process() never allocates
// on the audio thread.
class Arpeggiator
{
public:
    explicit Arpeggiator (SynthParams& sp);

    void prepare (double sampleRate);

    // Rewrites `midi` in place. `numSamples` is the buffer length in samples.
    // `currentBpm` is taken from the host transport (or the cached fallback).
    void process (juce::MidiBuffer& midi, int numSamples, double currentBpm);

private:
    struct Held { int note; int velocity; };

    SynthParams& params;
    double sr = 44100.0;

    // State (audio-thread owned).
    juce::Array<Held> held;     // notes physically held by user
    juce::Array<Held> latched;  // latch buffer (active only when latch is on)
    bool   wasOn         = false;
    int    stepIdx       = 0;
    int    octOffset     = 0;
    double samplesToStep = 0.0;
    int    samplesToOff  = -1;
    int    currentNote   = -1;
    int    currentChan   = 1;
    juce::Random rng;

    // Pre-allocated scratch buffers - sized once in prepare() so process() never
    // calls malloc on the audio thread.
    juce::MidiBuffer               passBuf;
    juce::Array<juce::MidiMessage> noteEventsBuf;
    juce::Array<int>               noteSamplesBuf;
    juce::Array<Held>              activeBuf;
    juce::Array<Held>              orderedBuf;
};
