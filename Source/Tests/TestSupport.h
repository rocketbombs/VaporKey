// Shared scaffolding the per-suite tests build on top of.
//
// TestProcessor : minimal juce::AudioProcessor that owns an APVTS built from
//                 Parameters::createLayout() and a SynthParams cache. We avoid
//                 instantiating the real VaporKeyAudioProcessor in tests so we
//                 don't pull in the editor / pages / widgets translation
//                 units; the production processor's only "audio" surface is
//                 SynthEngine + Arpeggiator + FxChain, which we can drive
//                 directly.
//
// AudioStats  : RMS / peak / NaN-Inf checks. Used by every audio-rendering
//                 test in this suite.
//
// MidiBuilder : convenience constructors for the MIDI events we send most.
//
// renderSilent / collectAudio : drive a configurable number of audio blocks
//                 through the engine + fx and concatenate the result so a test
//                 can inspect the produced waveform. The render helper sets
//                 sane block size / sample rate defaults but lets the test
//                 override.
#pragma once

#include <JuceHeader.h>

#include "../Parameters.h"
#include "../SynthEngine.h"
#include "../Arpeggiator.h"
#include "../FxChain.h"

namespace VKTest
{

// Minimal AudioProcessor that exists only so APVTS has somewhere to live.
class TestProcessor : public juce::AudioProcessor
{
public:
    TestProcessor()
        : juce::AudioProcessor (BusesProperties()
                                    .withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "TEST", Parameters::createLayout())
    {
        Parameters::cache (params, apvts);
    }

    // Pull AudioProcessor into satisfaction without doing anything interesting.
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override                     { return false; }

    const juce::String getName() const override   { return "VaporKeyTestProcessor"; }
    bool acceptsMidi() const override             { return true; }
    bool producesMidi() const override            { return false; }
    bool isMidiEffect() const override            { return false; }
    double getTailLengthSeconds() const override  { return 0.0; }

    int   getNumPrograms() override               { return 1; }
    int   getCurrentProgram() override            { return 0; }
    void  setCurrentProgram (int) override        {}
    const juce::String getProgramName (int) override { return {}; }
    void  changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        if (auto xml = apvts.copyState().createXml())
            copyXmlToBinary (*xml, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    juce::AudioProcessorValueTreeState apvts;
    SynthParams                        params;
};

// Set every parameter to its declared default. Useful for "reset between
// tests" so suite ordering is irrelevant.
void resetParametersToDefaults (TestProcessor& tp);

// Set a parameter by id to a non-normalised value (whatever the parameter's
// natural range is). Returns false if no such parameter exists.
bool setParameter (TestProcessor& tp, juce::StringRef id, float value);

struct AudioStats
{
    int   numSamples { 0 };
    float peakAbs { 0.0f };
    float rms { 0.0f };
    bool  hasNaN { false };
    bool  hasInf { false };
    bool  hasDenormalish { false }; // |x| < 1e-30 but non-zero, after a denormal flush should not happen
};

AudioStats analyse (const juce::AudioBuffer<float>& buf);

// Drive the synth engine + fx for `totalSamples` samples in `blockSize`-sample
// chunks, collecting the resulting audio into `outBuffer`. The supplied MIDI
// is consumed in the first block; subsequent blocks use empty MIDI.
//
// Use this rather than rolling your own loop in each test - it ensures the
// caller can't forget to call prepare() / set bpm / clear MIDI between blocks.
void renderAudio (TestProcessor& tp,
                  SynthEngine& engine,
                  Arpeggiator& arp,
                  FxChain& fx,
                  juce::MidiBuffer& firstBlockMidi,
                  juce::AudioBuffer<float>& outBuffer,
                  double sampleRate,
                  int totalSamples,
                  int blockSize,
                  double bpm = 120.0);

// MIDI message helpers (channel 1 by default, defaulting to consistent
// velocities so tests don't have to spell them out).
namespace Midi
{
    juce::MidiMessage noteOn  (int note, int velocity = 100, int channel = 1);
    juce::MidiMessage noteOff (int note, int channel = 1);
    juce::MidiMessage cc      (int controller, int value, int channel = 1);
    juce::MidiMessage pitchWheel (int value, int channel = 1); // 0..16383
    juce::MidiMessage afterTouch (int note, int value, int channel = 1);
    juce::MidiMessage channelPressure (int value, int channel = 1);
}

// Build a MidiBuffer containing one event at sample 0. Tests that need
// multiple events in a single block construct their own.
juce::MidiBuffer singleEventBuffer (const juce::MidiMessage& msg, int samplePosition = 0);

// Collect every note-on/off event in a buffer with their sample positions.
// Useful for verifying the arpeggiator's output sequence.
struct MidiNoteEvent
{
    int  sample { 0 };
    bool isOn   { false };
    int  note   { 0 };
    int  velocity { 0 };
};

std::vector<MidiNoteEvent> collectNoteEvents (const juce::MidiBuffer& midi);

// Build a synthetic mono audio buffer for wavetable-import tests. Generates
// `numFrames * Wavetable::kFrameSize` samples; each frame is a different
// audible shape so the import can verify "frame N != frame M".
std::vector<float> makeFramedTestSamples (int numFrames);

// Write `samples` to a temp .wav file. Returns the file (the caller should
// delete it). Used by wavetable import tests.
juce::File writeTempWav (const std::vector<float>& samples, int channels = 1, double sampleRate = 48000.0);

} // namespace VKTest
