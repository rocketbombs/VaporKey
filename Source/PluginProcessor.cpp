#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Voice.h"
#include "PresetStore.h"
#include "WavetableImport.h"
#include <algorithm>
#include <initializer_list>

VaporKeyAudioProcessor::VaporKeyAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", Parameters::createLayout())
{
    Parameters::cache (apvts, synthParams);
    WavetableLibrary::get();

    // Each WTVoice and the Arpeggiator seed their RNGs randomly in their own
    // constructor; nothing extra needs to happen here. Without per-instance
    // seeding, multiple plugin instances would produce identical random
    // streams that sum coherently and sound buzzy/aliased.

    synth.addSound (new WTSound());
    for (int i = 0; i < 16; ++i)
        synth.addVoice (new WTVoice (synthParams));
    synth.setNoteStealingEnabled (true);
}

void VaporKeyAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<WTVoice*> (synth.getVoice (i)))
            v->prepare (sampleRate);

    fx.prepare (sampleRate, samplesPerBlock);

    // Reserve generous capacity once so the audio thread never reallocates
    // these scratch buffers (the source of multi-instance host freezes).
    arp.prepare (sampleRate);
    monoFilterBuf.ensureSize (8192);
    monoHeldNotes.ensureStorageAllocated (64);
}

bool VaporKeyAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void VaporKeyAudioProcessor::updateMacroSums()
{
    for (auto& s : synthParams.modSum) s = 0.0f;
    for (int m = 0; m < SynthParams::kNumMacros; ++m)
    {
        const int dest = (int) (synthParams.macroDest[m]->load() + 0.5f);
        if (dest <= ModDest::None || dest >= ModDest::NumDests) continue;
        const float val = synthParams.macroVal[m]->load();   // 0..1
        const float amt = synthParams.macroAmt[m]->load();   // -1..1
        synthParams.modSum[dest] += val * amt;
    }

    auto applyMidiSource = [this] (std::atomic<float>* destP, std::atomic<float>* amtP, float val01)
    {
        const int dest = (int) (destP->load() + 0.5f);
        if (dest <= ModDest::None || dest >= ModDest::NumDests) return;
        synthParams.modSum[dest] += val01 * amtP->load();
    };
    applyMidiSource (synthParams.mwDest, synthParams.mwAmt, synthParams.modWheel.load());
    applyMidiSource (synthParams.atDest, synthParams.atAmt, synthParams.aftertouch.load());
}

void VaporKeyAudioProcessor::filterMidi (juce::MidiBuffer& midi)
{
    const bool monoMode = *synthParams.mono > 0.5f;
    const bool legato   = *synthParams.legato > 0.5f;
    const float bendRange = synthParams.bendRange->load();

    // Reuse pre-allocated MidiBuffer (audio-thread allocation is what freezes
    // hosts under heavy multi-instance loads).
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
            synthParams.pitchBendSemis.store (norm * bendRange);
            out.addEvent (msg, sample);
            continue;
        }
        if (msg.isController() && msg.getControllerNumber() == 1)
        {
            synthParams.modWheel.store (msg.getControllerValue() / 127.0f);
            out.addEvent (msg, sample);
            continue;
        }
        if (msg.isAftertouch() || msg.isChannelPressure())
        {
            const int p = msg.isChannelPressure() ? msg.getChannelPressureValue() : msg.getAfterTouchValue();
            synthParams.aftertouch.store (p / 127.0f);
            out.addEvent (msg, sample);
            continue;
        }

        if (monoMode && msg.isNoteOn())
        {
            // Mono = last-note priority: turn off whatever was sounding before
            // letting the new note through. The synth then steals the freed
            // voice for the new note, so a single voice always carries the
            // melody.
            for (int n : monoHeldNotes)
                if (n != msg.getNoteNumber())
                    out.addEvent (juce::MidiMessage::noteOff (msg.getChannel(), n), sample);

            // Legato: ask the voice that's about to be (re)started to leave
            // its envelopes alone. Together with last-note priority above and
            // voice stealing on the synth, this glides the existing voice into
            // the new pitch instead of re-attacking.
            if (legato && ! monoHeldNotes.isEmpty())
                markVoicesLegato();

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
                if (legato) markVoicesLegato();
                out.addEvent (msg, sample); // emit the off
                out.addEvent (juce::MidiMessage::noteOn (msg.getChannel(), n, (juce::uint8) 100), sample);
                continue;
            }
        }

        out.addEvent (msg, sample);
    }
    midi.swapWith (out);
}

// Flag currently-playing voices as "next startNote is a legato transition -
// keep envelopes running". The synth steals one of the active voices for the
// new note; that voice consumes the flag inside startNote. We deliberately
// skip idle voices so the flag can't leak across a mono->poly switch (idle
// voices that get retriggered later would otherwise skip their attack).
void VaporKeyAudioProcessor::markVoicesLegato()
{
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<WTVoice*> (synth.getVoice (i)))
            if (v->isVoiceActive())
                v->setLegatoSkipEnvRetrigger (true);
}

void VaporKeyAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nodn;

    if (auto* ph = getPlayHead())
    {
        if (auto info = ph->getPosition())
            if (auto bpm = info->getBpm()) currentBpm = *bpm;
    }

    synthParams.bpm.store (currentBpm);

    // Arpeggiator runs first: it consumes incoming note-on/off events and emits
    // a stepped sequence into the buffer. Mono/legato handling then operates
    // on whatever notes are flowing through (live or arpeggiated).
    arp.process (midi, buffer.getNumSamples(), synthParams, currentBpm);

    filterMidi (midi);

    // Compute the per-block modulation sums *after* filterMidi has scanned the
    // buffer for CC1 / channel pressure - otherwise mod-wheel / aftertouch
    // destinations are one audio callback stale and a note-on in this same
    // buffer would start with the previous block's modulation values.
    updateMacroSums();

    buffer.clear();
    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    const int numCh = buffer.getNumChannels();
    if (numCh < 1) return;

    fx.process (buffer, synthParams, currentBpm);

    auto* L = buffer.getWritePointer (0);
    auto* R = numCh > 1 ? buffer.getWritePointer (1) : L;
    const int n = buffer.getNumSamples();

    // Audio-reactive UI snapshots. Push the post-FX/post-master signal into
    // the scope ring + capture peak and RMS for the meter. The editor reads
    // these from a Timer; relaxed atomics are fine - this isn't synchronisation,
    // just a "most recent value wins" handoff.
    {
        float pL = 0.0f, pR = 0.0f, sumSq = 0.0f;
        auto write = vis.scopeWrite.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R[i];
            pL = juce::jmax (pL, std::abs (l));
            pR = juce::jmax (pR, std::abs (r));
            const float m = 0.5f * (l + r);
            sumSq += m * m;
            vis.scope[(write + (uint32_t) i) & VisData::kScopeMask] = m;
        }
        vis.scopeWrite.store (write + (uint32_t) n, std::memory_order_release);
        vis.peakL.store (pL, std::memory_order_relaxed);
        vis.peakR.store (pR, std::memory_order_relaxed);
        vis.rms.store (std::sqrt (sumSq / (float) juce::jmax (1, n)), std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* VaporKeyAudioProcessor::createEditor()
{
    return new VaporKeyAudioProcessorEditor (*this);
}

void VaporKeyAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Stash custom wavetable paths into the state tree so they survive saves.
    for (int i = 0; i < 3; ++i)
    {
        const auto id = juce::Identifier ("osc" + juce::String (i + 1) + "_wav");
        if (customWavPath[i].isNotEmpty())
            apvts.state.setProperty (id, customWavPath[i], nullptr);
        else
            apvts.state.removeProperty (id, nullptr);
    }
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VaporKeyAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        // Mute voices and clear FX tails before swapping in the new state -
        // otherwise the in-flight envelope releases and delay/reverb feedback
        // ride the new gain/filter values and produce clicks or bursts on
        // session reload.
        silenceForPresetSwitch();

        apvts.replaceState (juce::ValueTree::fromXml (*xml));

        // Restore custom wavetables from any stored paths (best-effort: file
        // may have moved). Only the path strings are persisted; the wavetable
        // is rebuilt on demand from disk.
        for (int i = 0; i < 3; ++i)
        {
            const auto id = juce::Identifier ("osc" + juce::String (i + 1) + "_wav");
            const auto path = apvts.state.getProperty (id).toString();
            customWavPath[i].clear();
            std::shared_ptr<Wavetable> empty;
            std::atomic_store (&synthParams.customTables[i], empty);
            if (path.isNotEmpty())
                loadCustomWavetable (i, juce::File (path));
        }

        // Host-restored state: we no longer know which named preset this corresponds to.
        currentPresetName = "(unnamed)";
        currentPresetIsFactory = false;
    }
}

int VaporKeyAudioProcessor::getNumPrograms() { return (int) PresetStore::factoryAll().size(); }
int VaporKeyAudioProcessor::getCurrentProgram() { return currentProgram; }
void VaporKeyAudioProcessor::setCurrentProgram (int idx)
{
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, idx);
    loadFactoryPreset (currentProgram);
}
const juce::String VaporKeyAudioProcessor::getProgramName (int idx)
{
    const auto& list = PresetStore::factoryAll();
    if (idx >= 0 && idx < (int) list.size()) return juce::String (list[(size_t) idx].name);
    return {};
}

void VaporKeyAudioProcessor::silenceForPresetSwitch()
{
    // Hold the audio callback lock so processBlock can't run while we kill
    // voices and clear FX buffers. Without this the old voices and the delay/
    // reverb tail would ride the new parameter values for a few ms and
    // produce a loud burst (filter cracks, feedback into a louder gain).
    const juce::ScopedLock sl (getCallbackLock());

    synth.allNotesOff (0, false);
    fx.reset();
}

void VaporKeyAudioProcessor::loadFactoryPreset (int index)
{
    const auto& list = PresetStore::factoryAll();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;

    silenceForPresetSwitch();
    PresetStore::applyFactory (*this, apvts, index);

    currentPresetName = juce::String (list[(size_t) index].name);
    currentPresetIsFactory = true;
}

juce::File VaporKeyAudioProcessor::getUserPresetsDir() const
{
    return PresetStore::userDir();
}

juce::StringArray VaporKeyAudioProcessor::getUserPresetNames() const
{
    return PresetStore::userList();
}

bool VaporKeyAudioProcessor::saveUserPreset (const juce::String& name)
{
    auto safeName = juce::File::createLegalFileName (name).trim();
    if (! PresetStore::userSave (apvts, safeName)) return false;
    currentPresetName = safeName;
    currentPresetIsFactory = false;
    return true;
}

bool VaporKeyAudioProcessor::loadUserPresetByName (const juce::String& name)
{
    silenceForPresetSwitch();
    if (! PresetStore::userLoad (apvts, name)) return false;
    currentPresetName = name;
    currentPresetIsFactory = false;
    return true;
}

bool VaporKeyAudioProcessor::deleteUserPreset (const juce::String& name)
{
    if (! PresetStore::userDelete (name)) return false;
    if (currentPresetName == name && ! currentPresetIsFactory)
        currentPresetName.clear();
    return true;
}

bool VaporKeyAudioProcessor::renameUserPreset (const juce::String& oldName, const juce::String& newName)
{
    auto safeNew = juce::File::createLegalFileName (newName).trim();
    if (! PresetStore::userRename (oldName, safeNew)) return false;
    if (currentPresetName == oldName && ! currentPresetIsFactory)
        currentPresetName = safeNew;
    return true;
}

juce::StringArray VaporKeyAudioProcessor::factoryPresetNames()
{
    return PresetStore::factoryNames();
}

bool VaporKeyAudioProcessor::loadCustomWavetable (int oscIndex, const juce::File& file)
{
    if (oscIndex < 0 || oscIndex >= 3) return false;

    auto wt = WavetableImport::loadFromFile (file);
    if (! wt) return false;

    std::atomic_store (&synthParams.customTables[oscIndex], wt);
    customWavPath[oscIndex] = file.getFullPathName();
    return true;
}

void VaporKeyAudioProcessor::clearCustomWavetable (int oscIndex)
{
    if (oscIndex < 0 || oscIndex >= 3) return;
    std::shared_ptr<Wavetable> empty;
    std::atomic_store (&synthParams.customTables[oscIndex], empty);
    customWavPath[oscIndex].clear();
}

juce::String VaporKeyAudioProcessor::getCustomWavetableName (int oscIndex) const
{
    if (oscIndex < 0 || oscIndex >= 3) return {};
    if (customWavPath[oscIndex].isEmpty()) return {};
    return juce::File (customWavPath[oscIndex]).getFileNameWithoutExtension();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VaporKeyAudioProcessor();
}
