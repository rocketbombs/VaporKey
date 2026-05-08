#include "TestSupport.h"

namespace VKTest
{

void resetParametersToDefaults (TestProcessor& tp)
{
    for (auto* param : tp.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
            p->setValueNotifyingHost (p->getDefaultValue());
}

bool setParameter (TestProcessor& tp, juce::StringRef id, float value)
{
    auto* p = tp.apvts.getParameter (id);
    if (p == nullptr) return false;
    const auto& range = p->getNormalisableRange();
    const float norm  = range.convertTo0to1 (value);
    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
    return true;
}

AudioStats analyse (const juce::AudioBuffer<float>& buf)
{
    AudioStats s;
    s.numSamples = buf.getNumSamples();
    if (s.numSamples <= 0 || buf.getNumChannels() <= 0) return s;

    double sumSq = 0.0;
    int    sampleCount = 0;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const float* p = buf.getReadPointer (ch);
        for (int i = 0; i < s.numSamples; ++i)
        {
            const float v = p[i];
            if (std::isnan (v)) { s.hasNaN = true; continue; }
            if (std::isinf (v)) { s.hasInf = true; continue; }
            const float a = std::abs (v);
            if (a > s.peakAbs) s.peakAbs = a;
            if (a > 0.0f && a < 1.0e-30f) s.hasDenormalish = true;
            sumSq += (double) v * (double) v;
            ++sampleCount;
        }
    }
    if (sampleCount > 0)
        s.rms = (float) std::sqrt (sumSq / (double) sampleCount);
    return s;
}

void renderAudio (TestProcessor& tp,
                  SynthEngine& engine,
                  Arpeggiator& arp,
                  FxChain& fx,
                  juce::MidiBuffer& firstBlockMidi,
                  juce::AudioBuffer<float>& outBuffer,
                  double sampleRate,
                  int totalSamples,
                  int blockSize,
                  double bpm)
{
    juce::ignoreUnused (tp);
    outBuffer.clear();
    if (totalSamples <= 0) return;

    juce::AudioBuffer<float> block (juce::jmax (1, outBuffer.getNumChannels()), blockSize);

    int written = 0;
    bool firstBlock = true;
    while (written < totalSamples)
    {
        const int n = juce::jmin (blockSize, totalSamples - written);
        block.setSize (block.getNumChannels(), n, false, false, true);
        block.clear();

        juce::MidiBuffer midi;
        if (firstBlock)
        {
            midi.swapWith (firstBlockMidi);
            firstBlock = false;
        }

        // Mirror the production processBlock order: bpm -> arp -> engine -> fx.
        arp.process (midi, n, bpm);
        engine.process (block, midi);
        fx.process (block, bpm);

        for (int ch = 0; ch < outBuffer.getNumChannels() && ch < block.getNumChannels(); ++ch)
            outBuffer.copyFrom (ch, written, block, ch, 0, n);

        written += n;
    }
}

namespace Midi
{
    juce::MidiMessage noteOn (int note, int velocity, int channel)
    {
        return juce::MidiMessage::noteOn (channel, note, (juce::uint8) juce::jlimit (1, 127, velocity));
    }

    juce::MidiMessage noteOff (int note, int channel)
    {
        return juce::MidiMessage::noteOff (channel, note);
    }

    juce::MidiMessage cc (int controller, int value, int channel)
    {
        return juce::MidiMessage::controllerEvent (channel, controller,
                                                   juce::jlimit (0, 127, value));
    }

    juce::MidiMessage pitchWheel (int value, int channel)
    {
        return juce::MidiMessage::pitchWheel (channel, juce::jlimit (0, 16383, value));
    }

    juce::MidiMessage afterTouch (int note, int value, int channel)
    {
        return juce::MidiMessage::aftertouchChange (channel, note,
                                                    juce::jlimit (0, 127, value));
    }

    juce::MidiMessage channelPressure (int value, int channel)
    {
        return juce::MidiMessage::channelPressureChange (channel,
                                                         juce::jlimit (0, 127, value));
    }
}

juce::MidiBuffer singleEventBuffer (const juce::MidiMessage& msg, int samplePosition)
{
    juce::MidiBuffer out;
    out.addEvent (msg, samplePosition);
    return out;
}

std::vector<MidiNoteEvent> collectNoteEvents (const juce::MidiBuffer& midi)
{
    std::vector<MidiNoteEvent> out;
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (! msg.isNoteOnOrOff()) continue;
        out.push_back ({ m.samplePosition, msg.isNoteOn(),
                         msg.getNoteNumber(), msg.getVelocity() });
    }
    return out;
}

std::vector<float> makeFramedTestSamples (int numFrames)
{
    const int n = numFrames * Wavetable::kFrameSize;
    std::vector<float> out ((size_t) n, 0.0f);
    for (int f = 0; f < numFrames; ++f)
    {
        const float harmonic = 1.0f + (float) f; // frame 0 = sine, frame 1 = +octave, ...
        for (int i = 0; i < Wavetable::kFrameSize; ++i)
        {
            const float t = (float) i / (float) Wavetable::kFrameSize;
            out[(size_t) (f * Wavetable::kFrameSize + i)]
                = std::sin (juce::MathConstants<float>::twoPi * harmonic * t);
        }
    }
    return out;
}

juce::File writeTempWav (const std::vector<float>& samples, int channels, double sampleRate)
{
    auto tmp = juce::File::createTempFile (".wav");

    auto fos = std::make_unique<juce::FileOutputStream> (tmp);
    if (! fos->openedOk()) return {};

    juce::WavAudioFormat fmt;
    // createWriterFor takes ownership of the stream on success (the writer's
    // destructor will delete it). On failure (nullptr returned), the caller
    // owns it - the unique_ptr above keeps that case clean.
    std::unique_ptr<juce::AudioFormatWriter> writer (
        fmt.createWriterFor (fos.get(), sampleRate, (unsigned int) channels, 16, {}, 0));
    if (writer == nullptr) return {};
    fos.release();

    const int total = (int) samples.size();
    if (channels == 1)
    {
        const float* ch[1] = { samples.data() };
        writer->writeFromFloatArrays (ch, 1, total);
    }
    else
    {
        // Spread the input round-robin across `channels` for the test path.
        std::vector<std::vector<float>> per ((size_t) channels);
        const int frames = total / channels;
        for (auto& v : per) v.resize ((size_t) frames, 0.0f);
        for (int i = 0; i < frames; ++i)
            for (int c = 0; c < channels; ++c)
                per[(size_t) c][(size_t) i] = samples[(size_t) (i * channels + c)];

        std::vector<const float*> ptrs ((size_t) channels);
        for (int c = 0; c < channels; ++c) ptrs[(size_t) c] = per[(size_t) c].data();
        writer->writeFromFloatArrays (ptrs.data(), channels, frames);
    }
    writer.reset();
    return tmp;
}

} // namespace VKTest
