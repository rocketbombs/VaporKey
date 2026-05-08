#pragma once
#include <JuceHeader.h>
#include "Wavetable.h"

// Helpers for swapping a per-oscillator user wavetable in or out at runtime.
// All swaps happen on the message thread (drag-and-drop / file chooser /
// state-restore) and publish through the SynthParams shared_ptr atomically;
// voices observe the swap via std::atomic_load on the audio thread.
//
// The caller (PluginProcessor) is responsible for persisting the path string
// alongside the APVTS state - we only mutate the path it hands us by reference.
namespace WavetableImport
{
    // Read `file` from disk, build a wavetable, swap it in, and remember the
    // path. Returns false (and leaves both target and path untouched) on any
    // failure: missing file, unreadable audio, zero-length data.
    bool loadInto (std::shared_ptr<Wavetable>& target,
                   juce::String& pathOut,
                   const juce::File& file);

    // Clear the slot: publish an empty shared_ptr and forget the path.
    void clear (std::shared_ptr<Wavetable>& target,
                juce::String& pathOut);

    // Display name (filename without extension) for a stored path. Empty if
    // no custom wavetable is set.
    juce::String displayName (const juce::String& path);
}
