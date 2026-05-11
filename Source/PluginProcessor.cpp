#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "PresetStore.h"
#include "WavetableImport.h"

#include <mutex>
#include <thread>

namespace
{
    // Kicks the two heavy Meyers singletons (WavetableLibrary - 23 FFT-built
    // mip-mapped tables; VKPresets factory JSON parse) onto a background
    // thread on the very first plugin construction.
    //
    // Why this exists: with the synchronous get() call living inside the
    // constructor, the host's message thread would block for the full init
    // duration whenever the .dll's static singletons were touched for the
    // first time - and would do so AGAIN serially per instance if the host
    // happened to construct several at once. Pulling the work onto a worker
    // thread lets the message thread queue up multiple constructors in
    // parallel; by the time any of them reaches prepareToPlay (which blocks
    // on the same static-init mutex) the warmer has typically completed and
    // the wait is zero.
    //
    // C++11 magic statics make WavetableLibrary::get() / VKPresets::all()
    // intrinsically thread-safe: whichever thread enters static init first
    // runs the body; any other thread that touches it blocks on the same
    // mutex. The warmer thread is detached because its only side effect is
    // populating those (program-lifetime) singletons; if the host unloads
    // the .dll while the warmer is still running the OS terminates the
    // thread along with the rest of the module.
    void kickSingletonWarmerOnce()
    {
        static std::once_flag flag;
        std::call_once (flag, []
        {
            std::thread ([]
            {
                try
                {
                    WavetableLibrary::get();
                    (void) VKPresets::all();
                }
                catch (...) {}
            }).detach();
        });
    }
}

VaporKeyAudioProcessor::VaporKeyAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", Parameters::createLayout()),
      engine (synthParams),
      arp (synthParams),
      fx (synthParams)
{
    Parameters::cache (synthParams, apvts);

    // The previous version blocked the message thread here until the entire
    // wavetable library had finished generating. The warmer below runs that
    // work in parallel, and prepareToPlay below joins on completion before
    // any audio block can ask the audio thread to touch the library.
    kickSingletonWarmerOnce();
}

void VaporKeyAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // prepareToPlay is invoked on the message thread by every JUCE-supported
    // host before processBlock starts. Blocking here is the right place to
    // join on the warmer: the audio thread isn't running yet, and we
    // guarantee that voices never trigger static-init on the audio path.
    WavetableLibrary::get();

    sr = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    arp.prepare (sampleRate);
    fx.prepare (sampleRate, samplesPerBlock);
}

bool VaporKeyAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
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
    // a stepped sequence into the buffer. Mono/legato handling inside the
    // engine then operates on whatever notes flow through (live or arpeggiated).
    arp.process (midi, buffer.getNumSamples(), currentBpm);

    // Engine runs the MIDI-controller scan, sums macros (so this block's
    // CC1/aftertouch land in modSum), and renders all voices.
    engine.process (buffer, midi);

    // Per-block FX chain.
    fx.process (buffer, currentBpm);

    // Audio-reactive UI snapshots. Push the post-FX/post-master signal into
    // the scope ring + capture peak and RMS for the meter. The editor reads
    // these from a Timer; relaxed atomics are fine - this isn't synchronisation,
    // just a "most recent value wins" handoff.
    const int numCh = buffer.getNumChannels();
    if (numCh < 1) return;
    const int n = buffer.getNumSamples();
    auto* L = buffer.getWritePointer (0);
    auto* R = numCh > 1 ? buffer.getWritePointer (1) : L;

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
        // Silence + state swap happen atomically under the callback lock so
        // the audio thread never sees a "voices muted, FX cleared, but
        // parameters are still the old ones" intermediate state.
        applyPresetUnderLock ([&]
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
        });

        // Restore custom wavetables from any stored paths (best-effort: file
        // may have moved). Only the path strings are persisted; the wavetable
        // is rebuilt on demand from disk. Disk I/O happens outside the
        // callback lock - the swap itself is RT-safe via the retirement
        // queue.
        for (int i = 0; i < 3; ++i)
        {
            const auto id = juce::Identifier ("osc" + juce::String (i + 1) + "_wav");
            const auto path = apvts.state.getProperty (id).toString();
            clearCustomWavetable (i);
            if (path.isNotEmpty())
                loadCustomWavetable (i, juce::File (path));
        }

        // Host-restored state: we no longer know which named preset this corresponds to.
        currentPresetName = "(unnamed)";
        currentPresetIsFactory = false;
    }
}

int VaporKeyAudioProcessor::getNumPrograms() { return PresetStore::factoryPresetCount(); }
int VaporKeyAudioProcessor::getCurrentProgram() { return currentProgram; }
void VaporKeyAudioProcessor::setCurrentProgram (int idx)
{
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, idx);
    loadFactoryPreset (currentProgram);
}
const juce::String VaporKeyAudioProcessor::getProgramName (int idx)
{
    const auto names = PresetStore::factoryPresetNames();
    if (idx >= 0 && idx < names.size()) return names[idx];
    return {};
}

void VaporKeyAudioProcessor::loadFactoryPreset (int index)
{
    if (index < 0 || index >= PresetStore::factoryPresetCount()) return;
    currentProgram = index;

    bool ok = false;
    applyPresetUnderLock ([&]
    {
        ok = PresetStore::applyFactoryPreset (apvts, *this, index);
    });
    if (! ok) return;

    currentPresetName = PresetStore::factoryPresetNames()[index];
    currentPresetIsFactory = true;
}

juce::StringArray VaporKeyAudioProcessor::factoryPresetNames()
{
    return PresetStore::factoryPresetNames();
}

juce::File VaporKeyAudioProcessor::getUserPresetsDir() const
{
    return PresetStore::getUserPresetsDir();
}

juce::StringArray VaporKeyAudioProcessor::getUserPresetNames() const
{
    return PresetStore::getUserPresetNames();
}

bool VaporKeyAudioProcessor::saveUserPreset (const juce::String& name)
{
    auto safeName = juce::File::createLegalFileName (name).trim();
    if (safeName.isEmpty()) return false;
    if (! PresetStore::saveUserPreset (apvts, safeName)) return false;
    currentPresetName = safeName;
    currentPresetIsFactory = false;
    return true;
}

bool VaporKeyAudioProcessor::loadUserPresetByName (const juce::String& name)
{
    // Parse before silencing so a missing or corrupt file doesn't briefly
    // kill audio for a click that ultimately fails.
    auto xml = PresetStore::readUserPresetXml (name);
    if (! xml) return false;
    applyPresetUnderLock ([&]
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    });
    currentPresetName = name;
    currentPresetIsFactory = false;
    return true;
}

bool VaporKeyAudioProcessor::deleteUserPreset (const juce::String& name)
{
    if (! PresetStore::deleteUserPreset (name)) return false;
    if (currentPresetName == name && ! currentPresetIsFactory)
        currentPresetName.clear();
    return true;
}

bool VaporKeyAudioProcessor::renameUserPreset (const juce::String& oldName, const juce::String& newName)
{
    auto safeNew = juce::File::createLegalFileName (newName).trim();
    if (safeNew.isEmpty() || safeNew == oldName) return false;
    if (! PresetStore::renameUserPreset (oldName, safeNew)) return false;
    if (currentPresetName == oldName && ! currentPresetIsFactory)
        currentPresetName = safeNew;
    return true;
}

bool VaporKeyAudioProcessor::loadCustomWavetable (int oscIndex, const juce::File& file)
{
    if (oscIndex < 0 || oscIndex >= 3) return false;
    return WavetableImport::loadInto (synthParams.customTables[oscIndex],
                                       customWavPath[oscIndex],
                                       file,
                                       wavetableRetire);
}

void VaporKeyAudioProcessor::clearCustomWavetable (int oscIndex)
{
    if (oscIndex < 0 || oscIndex >= 3) return;
    WavetableImport::clear (synthParams.customTables[oscIndex],
                            customWavPath[oscIndex],
                            wavetableRetire);
}

juce::String VaporKeyAudioProcessor::getCustomWavetableName (int oscIndex) const
{
    if (oscIndex < 0 || oscIndex >= 3) return {};
    return WavetableImport::displayName (customWavPath[oscIndex]);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VaporKeyAudioProcessor();
}
