#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>
#include "Wavetable.h"

// Cross-thread carrier for a per-oscillator user wavetable.
//
//   Audio thread   : reads with snapshot() once per block.
//   Message thread : publishes new tables with swap() (drag-and-drop, file
//                    chooser, state-restore) and clears with clear().
//
// Lifetime guarantee: a Wavetable is only ever destroyed on the message
// thread. swap() captures the outgoing shared_ptr into a retired list before
// publishing the replacement, so an old table's reference count cannot fall
// to zero on the audio thread - the destructor (which deallocates KB-scale
// frame storage via free()) cannot run inside processBlock. drain() releases
// retired entries only once their use_count is back to one, meaning every
// audio reader has moved on and the retired list itself is the sole owner.
//
// The retired list is touched only from the message thread; the audio thread
// only reads `published` via the atomic helpers below.
class WavetableSlot
{
public:
    WavetableSlot() = default;

    WavetableSlot (const WavetableSlot&) = delete;
    WavetableSlot& operator= (const WavetableSlot&) = delete;
    WavetableSlot (WavetableSlot&&) = delete;
    WavetableSlot& operator= (WavetableSlot&&) = delete;

    // Audio thread.
    std::shared_ptr<Wavetable> snapshot() const noexcept
    {
        // TODO(C++20): once `published` is std::atomic<std::shared_ptr<Wavetable>>
        // this becomes published.load(); the free-function overload of
        // std::atomic_load on a non-atomic shared_ptr is deprecated in C++20
        // and removed in C++26.
        return std::atomic_load (&published);
    }

    // Message thread. Publishes `newTable` (may be null) and parks the
    // outgoing table in the retired list. Always calls drain() so the list
    // doesn't grow unboundedly across repeated swaps.
    void swap (std::shared_ptr<Wavetable> newTable)
    {
        auto old = std::atomic_load (&published);
        // TODO(C++20): see snapshot().
        std::atomic_store (&published, std::move (newTable));
        if (old) retired.push_back (std::move (old));
        drain();
    }

    // Message thread.
    void clear() { swap ({}); }

    // Message thread. Releases every retired entry whose only remaining
    // reference is the retired list itself - i.e. no audio voice is still
    // holding a snapshot. The audio thread cannot acquire a fresh reference
    // to a retired table because `published` no longer points to it, so once
    // an entry hits use_count == 1 it stays there until drain runs.
    void drain()
    {
        retired.erase (
            std::remove_if (retired.begin(), retired.end(),
                            [] (const std::shared_ptr<Wavetable>& p)
                            { return p.use_count() == 1; }),
            retired.end());
    }

private:
    std::shared_ptr<Wavetable> published;
    std::vector<std::shared_ptr<Wavetable>> retired;
};
