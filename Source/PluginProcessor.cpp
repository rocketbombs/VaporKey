#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetStore.h"
#include "WavetableImport.h"

VaporKeyAudioProcessor::VaporKeyAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", Parameters::createLayout()),
      engine (synthParams),
      arp (synthParams),
      fx (synthParams)
{
    Parameters::cache (synthParams, apvts);
    WavetableLibrary::get();
}

void VaporKeyAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    arp.prepare (sampleRate);
    fx.prepare (sampleRate, samplesPerBlock);

    // Reserve scratch capacity for the mono-output mixdown path: processBlock
    // only ever clamps the logical size down to the current block's samples,
    // never expands beyond what we allocate here.
    stereoScratch.setSize (2, samplesPerBlock, false, false, false);
    stereoScratch.clear();
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

    const int outCh = buffer.getNumChannels();
    if (outCh < 1) return;
    const int n = buffer.getNumSamples();

    // Synth and FX always operate on a stereo buffer. When the host has
    // selected a mono output we point them at stereoScratch instead of the
    // host buffer and mix down at the end - that way the per-stage code
    // (distortion, EQ, delay, width) never has to handle aliased L == R, and
    // mono really is the average of L+R rather than a doubled-up monoised
    // mess. setSize with avoidReallocating=true is allocation-free as long
    // as the host honours samplesPerBlock from prepareToPlay.
    const bool isMonoOut = (outCh < 2);
    if (isMonoOut)
        stereoScratch.setSize (2, n, false, false, true);
    juce::AudioBuffer<float>& work = isMonoOut ? stereoScratch : buffer;

    // Arpeggiator runs first: it consumes incoming note-on/off events and emits
    // a stepped sequence into the buffer. Mono/legato handling inside the
    // engine then operates on whatever notes flow through (live or arpeggiated).
    arp.process (midi, n, currentBpm);

    // Engine runs the MIDI-controller scan, sums macros (so this block's
    // CC1/aftertouch land in modSum), and renders all voices.
    engine.process (work, midi);

    // Per-block FX chain.
    fx.process (work, currentBpm);

    // Audio-reactive UI snapshots. Push the post-FX/post-master signal into
    // the scope ring + capture peak and RMS for the meter. We read from
    // `work` (always stereo) so the meter shows the synth's actual L/R image
    // even when the host bus is mono. The editor reads these from a Timer;
    // relaxed atomics are fine - this isn't synchronisation, just a "most
    // recent value wins" handoff.
    auto* L = work.getWritePointer (0);
    auto* R = work.getWritePointer (1);
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

    // Mono mixdown: average L+R into the host's single output channel.
    if (isMonoOut)
    {
        auto* outMono = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i)
            outMono[i] = 0.5f * (L[i] + R[i]);
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
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (! xml) return;

    // Hold the callback lock for the full silence -> reset -> APVTS replace
    // sequence (RealtimeSafety.md: preset apply is atomic from the audio
    // thread's point of view). Without this, processBlock could run between
    // the silencing and the state replacement and briefly drive in-flight
    // voices and FX tails through partially-updated parameters - audible as
    // clicks or filter cracks on session reload.
    {
        const juce::ScopedLock sl (getCallbackLock());
        engine.allNotesOff();
        fx.reset();
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    // Restore custom wavetables from any stored paths (best-effort: file
    // may have moved). This is deliberately outside the callback lock - it
    // does file I/O, and the wavetable publish path is already lock-free
    // (atomic shared_ptr swap; voices observe on the next block).
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

    // RealtimeSafety.md: preset apply is atomic from the audio thread's
    // point of view. Hold the callback lock for the full silence -> reset
    // -> APVTS overwrite so processBlock can't see a partially-applied
    // state (e.g. old envelope tail running through new gain).
    bool ok;
    {
        const juce::ScopedLock sl (getCallbackLock());
        engine.allNotesOff();
        fx.reset();
        ok = PresetStore::applyFactoryPreset (apvts, *this, index);
    }
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
    // Route the public-facing accessor through the creating helper: any caller
    // (e.g. a future "Open user presets folder" button) gets a usable path.
    // Internal read paths in PresetStore use the pure getter so that simply
    // opening the editor doesn't materialise an empty Presets directory.
    return PresetStore::ensureUserPresetsDir();
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

    // Hold the callback lock for the full silence -> reset -> APVTS replace.
    // See loadFactoryPreset and RealtimeSafety.md for the invariant.
    {
        const juce::ScopedLock sl (getCallbackLock());
        engine.allNotesOff();
        fx.reset();
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

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
                                       file);
}

void VaporKeyAudioProcessor::clearCustomWavetable (int oscIndex)
{
    if (oscIndex < 0 || oscIndex >= 3) return;
    WavetableImport::clear (synthParams.customTables[oscIndex], customWavPath[oscIndex]);
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
