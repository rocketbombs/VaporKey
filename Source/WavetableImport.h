#pragma once
#include <JuceHeader.h>
#include <memory>
#include "Wavetable.h"

namespace WavetableImport
{
    // Read a mono or multi-channel .wav file and build a Wavetable from the
    // first kFrameSize * kNumFrames samples (multi-channel sources are mixed
    // down to mono first). Returns nullptr on read or format failure; never
    // throws.
    //
    // Safe to call from the message thread; the returned shared_ptr can be
    // handed to a SynthParams::customTables slot via std::atomic_store while
    // the audio thread is running.
    std::shared_ptr<Wavetable> loadFromFile (const juce::File& file);
}
