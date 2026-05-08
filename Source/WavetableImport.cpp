#include "WavetableImport.h"
#include <algorithm>
#include <atomic>
#include <vector>

WavetableRetirementQueue::WavetableRetirementQueue()
{
    // 100 ms is comfortably longer than any realistic audio buffer (a typical
    // 512-sample block at 44.1 kHz is ~12 ms). After a publish, voices that
    // had snapshotted the previous table release their snapshot at the end
    // of their current renderNextBlock; by the next sweep tick the queue is
    // the sole holder and the destruction runs on the message thread.
    startTimerHz (10);
}

WavetableRetirementQueue::~WavetableRetirementQueue()
{
    stopTimer();
    // Drop everything we still hold. By the time the processor (and thus
    // this queue) is destructed, the audio thread is no longer running, so
    // the destructor of any remaining shared_ptr runs safely on this thread.
}

void WavetableRetirementQueue::retire (std::shared_ptr<Wavetable> oldTable)
{
    if (! oldTable) return;
    const juce::ScopedLock lk (mutex);
    retired.push_back (std::move (oldTable));
}

void WavetableRetirementQueue::sweep()
{
    const juce::ScopedLock lk (mutex);
    retired.erase (
        std::remove_if (retired.begin(), retired.end(),
            [] (const std::shared_ptr<Wavetable>& sp)
            {
                // use_count == 1 means the queue is the sole holder, so any
                // voice that previously snapshotted this table has finished
                // its block. Dropping it here destroys the table on the
                // message thread.
                return sp.use_count() == 1;
            }),
        retired.end());
}

void WavetableRetirementQueue::timerCallback() { sweep(); }

bool WavetableImport::loadInto (std::shared_ptr<Wavetable>& target,
                                juce::String& pathOut,
                                const juce::File& file,
                                WavetableRetirementQueue& retire)
{
    if (! file.existsAsFile()) return false;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->numChannels < 1) return false;

    const int total = (int) juce::jmin ((juce::int64) (Wavetable::kFrameSize * Wavetable::kNumFrames),
                                         reader->lengthInSamples);
    if (total <= 0) return false;

    juce::AudioBuffer<float> buf ((int) reader->numChannels, total);
    if (! reader->read (&buf, 0, total, 0, true, reader->numChannels > 1)) return false;

    // Mix down to mono if needed.
    std::vector<float> mono ((size_t) total, 0.0f);
    if (buf.getNumChannels() == 1)
    {
        const float* s = buf.getReadPointer (0);
        for (int i = 0; i < total; ++i) mono[(size_t) i] = s[i];
    }
    else
    {
        const int ch = buf.getNumChannels();
        const float invCh = 1.0f / (float) ch;
        for (int c = 0; c < ch; ++c)
        {
            const float* s = buf.getReadPointer (c);
            for (int i = 0; i < total; ++i) mono[(size_t) i] += s[i] * invCh;
        }
    }

    auto newTable = std::make_shared<Wavetable>();
    newTable->buildFromMonoAudio (mono.data(), total);

    // Atomic exchange so we recover the previous shared_ptr in one operation.
    // Handing it to the retirement queue guarantees the message thread holds
    // the last reference - voices that snapshotted the old table will see
    // their refcount decrement to "queue + voice", never to zero, so
    // deallocation never runs on the audio thread.
    auto oldTable = std::atomic_exchange (&target, newTable);
    retire.retire (std::move (oldTable));
    pathOut = file.getFullPathName();
    return true;
}

void WavetableImport::clear (std::shared_ptr<Wavetable>& target,
                             juce::String& pathOut,
                             WavetableRetirementQueue& retire)
{
    std::shared_ptr<Wavetable> empty;
    auto oldTable = std::atomic_exchange (&target, empty);
    retire.retire (std::move (oldTable));
    pathOut.clear();
}

juce::String WavetableImport::displayName (const juce::String& path)
{
    if (path.isEmpty()) return {};
    return juce::File (path).getFileNameWithoutExtension();
}
