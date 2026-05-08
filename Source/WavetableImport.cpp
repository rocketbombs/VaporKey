#include "WavetableImport.h"
#include <vector>

std::shared_ptr<Wavetable> WavetableImport::loadFromFile (const juce::File& file)
{
    if (! file.existsAsFile()) return {};

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->numChannels < 1) return {};

    const int total = (int) juce::jmin ((juce::int64) (Wavetable::kFrameSize * Wavetable::kNumFrames),
                                         reader->lengthInSamples);
    if (total <= 0) return {};

    juce::AudioBuffer<float> buf ((int) reader->numChannels, total);
    if (! reader->read (&buf, 0, total, 0, true, reader->numChannels > 1)) return {};

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

    auto wt = std::make_shared<Wavetable>();
    wt->buildFromMonoAudio (mono.data(), total);
    return wt;
}
