#include "SynthEngine.h"
#include "SynthVoice.h"

SynthEngine::SynthEngine (SynthParams& sp) : params (sp)
{
    synthesiser.addSound (new WTSound());
    for (int i = 0; i < 16; ++i)
        synthesiser.addVoice (new WTVoice (params));
    synthesiser.setNoteStealingEnabled (true);
}

void SynthEngine::prepare (double sampleRate, int samplesPerBlock)
{
    synthesiser.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synthesiser.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<WTVoice*> (synthesiser.getVoice (i)))
            v->prepare (sampleRate);

    // Reserve generous capacity so the audio thread never reallocates these
    // scratch buffers (multi-instance host freezes otherwise).
    monoFilterBuf.ensureSize (8192);
    monoHeldNotes.ensureStorageAllocated (64);

    juce::ignoreUnused (samplesPerBlock);
}

void SynthEngine::allNotesOff()
{
    synthesiser.allNotesOff (0, false);
}

void SynthEngine::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    filterMidi (midi);

    // Macros are summed after the MIDI scan so CC1/aftertouch arriving in
    // this block feed modulation for any note-on the synth renders below.
    // Running before filterMidi would leave a one-callback lag and start
    // same-block notes with stale modulation.
    updateMacroSums();

    buffer.clear();
    synthesiser.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
}

void SynthEngine::filterMidi (juce::MidiBuffer& midi)
{
    const bool monoMode = *params.mono > 0.5f;
    const bool legato   = *params.legato > 0.5f;
    const float bendRange = params.bendRange->load();

    auto& out = monoFilterBuf;
    out.clear();

    for (const auto meta : midi)
    {
        auto msg = meta.getMessage();
        const int sample = meta.samplePosition;

        // Pitch bend: convert to semitones for voices
        if (msg.isPitchWheel())
        {
            const int wheel = msg.getPitchWheelValue();
            const float norm = (float) (wheel - 8192) / 8192.0f; // -1..1
            params.pitchBendSemis.store (norm * bendRange);
            out.addEvent (msg, sample);
            continue;
        }
        if (msg.isController() && msg.getControllerNumber() == 1)
        {
            params.modWheel.store (msg.getControllerValue() / 127.0f);
            out.addEvent (msg, sample);
            continue;
        }
        if (msg.isAftertouch() || msg.isChannelPressure())
        {
            const int p = msg.isChannelPressure() ? msg.getChannelPressureValue() : msg.getAfterTouchValue();
            params.aftertouch.store (p / 127.0f);
            out.addEvent (msg, sample);
            continue;
        }

        if (monoMode && msg.isNoteOn())
        {
            // Mono = last-note priority: turn off whatever was sounding before
            // letting the new note through. The synth then steals the freed
            // voice for the new note, so a single voice always carries the
            // melody.
            //
            // For legato we arm a hand-off on the active voice. When the
            // synth processes the noteOff/noteOn pair below, the voice's
            // stopNote consumes the arm flag - it clears its
            // currentlyPlayingNote without releasing envelopes - and the
            // following findFreeVoice picks that same voice up so the new
            // startNote glides on preserved envelope / phase / filter
            // state. Without it, JUCE routed the noteOn to whatever idle
            // voice was first in line and the legato flag was wasted.
            if (legato && ! monoHeldNotes.isEmpty())
                prepareLegatoTransition();

            for (int n : monoHeldNotes)
                if (n != msg.getNoteNumber())
                    out.addEvent (juce::MidiMessage::noteOff (msg.getChannel(), n), sample);

            monoHeldNotes.removeAllInstancesOf (msg.getNoteNumber());
            monoHeldNotes.add (msg.getNoteNumber());
            out.addEvent (msg, sample);
            continue;
        }
        if (monoMode && msg.isNoteOff())
        {
            monoHeldNotes.removeAllInstancesOf (msg.getNoteNumber());
            // If other notes are still held, re-trigger the most recent.
            if (! monoHeldNotes.isEmpty())
            {
                const int n = monoHeldNotes.getLast();
                // Same hand-off pattern as the noteOn branch: free the
                // active voice ahead of the noteOff/noteOn pair so the
                // re-trigger lands on it with the legato flag set.
                if (legato) prepareLegatoTransition();
                out.addEvent (msg, sample); // emit the off
                out.addEvent (juce::MidiMessage::noteOn (msg.getChannel(), n, (juce::uint8) 100), sample);
                continue;
            }
        }

        out.addEvent (msg, sample);
    }
    midi.swapWith (out);
}

// Arm a legato hand-off on the currently-sounding voice so that when the
// synth processes the upcoming noteOff (which we emit alongside the new
// noteOn at the same sample position), the voice clears its
// currentlyPlayingNote without releasing envelopes - then findFreeVoice
// picks up that same voice for the matching noteOn and the new startNote
// glides on top of preserved state.
//
// We arm a flag rather than clearing currentlyPlayingNote synchronously
// because synchronous clearing flips isVoiceActive() to false immediately;
// any samples between block start and the noteOff position would then be
// rendered silently (renderNextBlock early-returns when inactive),
// producing an audible dropout at the legato transition point. Arming
// defers the clear until the noteOff is actually delivered.
//
// Without this hand-off, JUCE's voice picker preferred any of the 15 idle
// voices over the held voice (which is still in sustain) for the incoming
// noteOn, so the new note re-attacked on a fresh voice - i.e. mono+legato
// silently behaved like mono+retrigger.
//
// Returns true if a voice was found and armed.
bool SynthEngine::prepareLegatoTransition()
{
    for (int i = 0; i < synthesiser.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<WTVoice*> (synthesiser.getVoice (i)))
            if (v->isVoiceActive())
            {
                v->armLegatoTransition();
                return true;
            }
    return false;
}

void SynthEngine::updateMacroSums()
{
    for (auto& s : params.modSum) s = 0.0f;
    for (int m = 0; m < SynthParams::kNumMacros; ++m)
    {
        const int dest = (int) (params.macroDest[m]->load() + 0.5f);
        if (dest <= ModDest::None || dest >= ModDest::NumDests) continue;
        const float val = params.macroVal[m]->load();   // 0..1
        const float amt = params.macroAmt[m]->load();   // -1..1
        params.modSum[dest] += val * amt;
    }

    auto applyMidiSource = [this] (std::atomic<float>* destP, std::atomic<float>* amtP, float val01)
    {
        const int dest = (int) (destP->load() + 0.5f);
        if (dest <= ModDest::None || dest >= ModDest::NumDests) return;
        params.modSum[dest] += val01 * amtP->load();
    };
    applyMidiSource (params.mwDest, params.mwAmt, params.modWheel.load());
    applyMidiSource (params.atDest, params.atAmt, params.aftertouch.load());
}
