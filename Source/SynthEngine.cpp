#include "SynthEngine.h"
#include "Voice.h"

SynthEngine::SynthEngine (SynthParams& params)
{
    synth.addSound (new WTSound());
    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice (new WTVoice (params));
    synth.setNoteStealingEnabled (true);
}

void SynthEngine::prepare (double sampleRate)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<WTVoice*> (synth.getVoice (i)))
            v->prepare (sampleRate);
}

void SynthEngine::renderBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
}

void SynthEngine::allNotesOff()
{
    synth.allNotesOff (0, false);
}

void SynthEngine::markVoicesLegato()
{
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<WTVoice*> (synth.getVoice (i)))
            if (v->isVoiceActive())
                v->setLegatoSkipEnvRetrigger (true);
}
