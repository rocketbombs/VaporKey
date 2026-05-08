#pragma once
#include <JuceHeader.h>
#include "Wavetable.h"
#include "WavetableSlot.h"

// Helpers for swapping a per-oscillator user wavetable in or out at runtime.
// All swaps happen on the message thread (drag-and-drop / file chooser /
// state-restore). The slot's swap()/clear() handle the cross-thread publish
// and retired-table bookkeeping; voices observe via WavetableSlot::snapshot()
// on the audio thread.
//
// The caller (PluginProcessor) is responsible for persisting the path string
// alongside the APVTS state - we only mutate the path it hands us by reference.
namespace WavetableImport
{
    // Read `file` from disk, build a wavetable, publish it into `target`,
    // and remember the path. Returns false (and leaves both target and path
    // untouched) on any failure: missing file, unreadable audio, zero-length
    // data.
    bool loadInto (WavetableSlot& target,
                   juce::String& pathOut,
                   const juce::File& file);

    // Clear the slot: publish an empty table and forget the path.
    void clear (WavetableSlot& target,
                juce::String& pathOut);

    // Display name (filename without extension) for a stored path. Empty if
    // no custom wavetable is set.
    juce::String displayName (const juce::String& path);
}
