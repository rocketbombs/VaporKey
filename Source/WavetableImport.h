#pragma once
#include <JuceHeader.h>
#include <vector>
#include "Wavetable.h"

// Owns the lifetime of replaced custom wavetables. When the message thread
// publishes a new table via std::atomic_exchange, the previous shared_ptr is
// handed to this queue; the queue holds a reference until any voice that may
// still be rendering with a snapshot of that table has finished its block,
// at which point a periodic sweep on the message thread destroys the table.
//
// This is the hardening described in docs/RealtimeSafety.md: without it, the
// last reference to an old table can drop on the audio thread (when a voice's
// local snapshot in renderNextBlock destructs) and the resulting heap
// deallocation runs on the audio path. The queue keeps the message thread as
// the guaranteed last holder so destruction always happens off-thread.
class WavetableRetirementQueue : private juce::Timer
{
public:
    WavetableRetirementQueue();
    ~WavetableRetirementQueue() override;

    // Take ownership of a retired table. Safe to call with a null pointer
    // (no-op). Called only from the message thread (file chooser / drag-drop /
    // state restore).
    void retire (std::shared_ptr<Wavetable> oldTable);

    // Drop any retired entries whose use_count has fallen to 1 (we are the
    // sole holder). Runs automatically on a message-thread timer; exposed
    // here for tests and for explicit drains during shutdown.
    void sweep();

private:
    void timerCallback() override;

    juce::CriticalSection mutex;
    std::vector<std::shared_ptr<Wavetable>> retired;
};

// Helpers for swapping a per-oscillator user wavetable in or out at runtime.
// All swaps happen on the message thread (drag-and-drop / file chooser /
// state-restore) and publish through the SynthParams shared_ptr atomically;
// voices observe the swap via std::atomic_load on the audio thread. The
// previous shared_ptr is handed to the WavetableRetirementQueue so the audio
// thread is never the last holder.
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
                   const juce::File& file,
                   WavetableRetirementQueue& retire);

    // Clear the slot: publish an empty shared_ptr and forget the path.
    void clear (std::shared_ptr<Wavetable>& target,
                juce::String& pathOut,
                WavetableRetirementQueue& retire);

    // Display name (filename without extension) for a stored path. Empty if
    // no custom wavetable is set.
    juce::String displayName (const juce::String& path);
}
