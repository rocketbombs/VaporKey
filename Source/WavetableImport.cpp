#include "WavetableImport.h"
#include <atomic>
#include <vector>

bool WavetableImport::loadInto (std::shared_ptr<Wavetable>& target,
                                juce::String& pathOut,
                                const juce::File& file)
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

    std::atomic_store (&target, newTable);
    pathOut = file.getFullPathName();
    return true;
}

void WavetableImport::clear (std::shared_ptr<Wavetable>& target, juce::String& pathOut)
{
    std::shared_ptr<Wavetable> empty;
    std::atomic_store (&target, empty);
    pathOut.clear();
}

juce::String WavetableImport::displayName (const juce::String& path)
{
    if (path.isEmpty()) return {};
    return juce::File (path).getFileNameWithoutExtension();
}
