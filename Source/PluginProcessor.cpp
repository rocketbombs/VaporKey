#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthVoice.h"
#include "Presets.h"
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

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    chorusFx.prepare (spec); chorusFx.setCentreDelay (7.0f); chorusFx.setFeedback (0.2f);
    phaserFx.prepare (spec);
    compFx.prepare (spec);
    delayL.prepare (spec); delayL.setMaximumDelayInSamples ((int) (sampleRate * 2.0));
    delayR.prepare (spec); delayR.setMaximumDelayInSamples ((int) (sampleRate * 2.0));
    delayL.reset(); delayR.reset();
    reverbFx.setSampleRate (sampleRate);

    juce::dsp::ProcessSpec mono { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    eqLowL.prepare (mono); eqLowR.prepare (mono);
    eqMidL.prepare (mono); eqMidR.prepare (mono);
    eqHighL.prepare (mono); eqHighR.prepare (mono);

    delaySmoothedL.reset (sampleRate, 0.05);
    delaySmoothedR.reset (sampleRate, 0.05);

    // Reserve generous capacity once so the audio thread never reallocates
    // these scratch buffers (the source of multi-instance host freezes).
    arp.prepare (sampleRate);
    monoFilterBuf.ensureSize (8192);
    monoHeldNotes.ensureStorageAllocated (64);

    // Force EQ coefficients to be rebuilt on the first block at the new rate.
    prevEqLowG = prevEqMidG = prevEqMidF = prevEqHighG = 1.0e9f;
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

static inline float distort (float x, int type, float drive)
{
    const float d = 1.0f + drive * 9.0f;
    const float xd = x * d;
    switch (type)
    {
        case DistType::Soft:
        {
            const float x2 = xd * xd;
            return xd * (27.0f + x2) / (27.0f + 9.0f * x2);
        }
        case DistType::Hard:
            return juce::jlimit (-1.0f, 1.0f, xd);
        case DistType::Fold:
        {
            float v = std::fmod (xd + 1.0f, 4.0f);
            if (v < 0) v += 4.0f;
            return std::abs (v - 2.0f) - 1.0f;
        }
        case DistType::Bit:
        {
            const float steps = std::pow (2.0f, 8.0f - drive * 6.5f);
            return std::round (xd * steps) / steps;
        }
    }
    return xd;
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

    auto* L = buffer.getWritePointer (0);
    auto* R = numCh > 1 ? buffer.getWritePointer (1) : L;
    const int n = buffer.getNumSamples();

    // Distortion
    {
        const float dMix = *synthParams.distMix;
        const float drv  = *synthParams.distDrive;
        if (dMix > 0.001f && drv > 0.0001f)
        {
            const int type = (int) (synthParams.distType->load() + 0.5f);
            const float wet = dMix;
            const float dry = 1.0f - dMix;
            for (int i = 0; i < n; ++i)
            {
                L[i] = L[i] * dry + distort (L[i], type, drv) * wet;
                R[i] = R[i] * dry + distort (R[i], type, drv) * wet;
            }
        }
    }

    // Chorus
    {
        const float chMix = *synthParams.chorus;
        if (chMix > 0.001f)
        {
            chorusFx.setMix (chMix);
            chorusFx.setRate (*synthParams.chorusRate);
            chorusFx.setDepth (*synthParams.chorusDepth);
            juce::dsp::AudioBlock<float> blk (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (blk);
            chorusFx.process (ctx);
        }
    }

    // Phaser
    {
        const float phMix = *synthParams.phaser;
        if (phMix > 0.001f)
        {
            phaserFx.setMix (phMix);
            phaserFx.setRate (*synthParams.phaserRate);
            phaserFx.setDepth (*synthParams.phaserDepth);
            phaserFx.setFeedback (*synthParams.phaserFb);
            phaserFx.setCentreFrequency (1300.0f);
            juce::dsp::AudioBlock<float> blk (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (blk);
            phaserFx.process (ctx);
        }
    }

    // EQ (3-band: low shelf, peak mid, high shelf). The make* helpers each
    // allocate a ReferenceCountedObject under the hood, so we only call them
    // when an input parameter actually changes.
    {
        const float lowG  = *synthParams.eqLow;
        const float midG  = *synthParams.eqMid;
        const float midF  = *synthParams.eqMidF;
        const float highG = *synthParams.eqHigh;
        if (std::abs (lowG) + std::abs (midG) + std::abs (highG) > 0.05f)
        {
            constexpr float kEqEps = 1.0e-6f;
            if (std::abs (lowG - prevEqLowG) > kEqEps)
            {
                *eqLowL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 200.0f, 0.707f, juce::Decibels::decibelsToGain (lowG));
                *eqLowR.coefficients = *eqLowL.coefficients;
                prevEqLowG = lowG;
            }
            if (std::abs (midG - prevEqMidG) > kEqEps || std::abs (midF - prevEqMidF) > kEqEps)
            {
                *eqMidL.coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, midF, 0.8f, juce::Decibels::decibelsToGain (midG));
                *eqMidR.coefficients = *eqMidL.coefficients;
                prevEqMidG = midG;
                prevEqMidF = midF;
            }
            if (std::abs (highG - prevEqHighG) > kEqEps)
            {
                *eqHighL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 5000.0f, 0.707f, juce::Decibels::decibelsToGain (highG));
                *eqHighR.coefficients = *eqHighL.coefficients;
                prevEqHighG = highG;
            }

            for (int i = 0; i < n; ++i)
            {
                L[i] = eqHighL.processSample (eqMidL.processSample (eqLowL.processSample (L[i])));
                R[i] = eqHighR.processSample (eqMidR.processSample (eqLowR.processSample (R[i])));
            }
        }
    }

    // Stereo delay (ping-pong-ish), tempo sync optional
    {
        const float dMix = *synthParams.delay;
        if (dMix > 0.001f)
        {
            float dt;
            if (*synthParams.delaySync > 0.5f)
            {
                const int divIdx = (int) (synthParams.delayDiv->load() + 0.5f);
                const double beats = syncDivToBeats (divIdx);
                dt = (float) (beats * 60.0 / juce::jmax (20.0, currentBpm));
                dt = juce::jlimit (0.005f, 1.5f, dt);
            }
            else
            {
                dt = juce::jlimit (0.005f, 1.5f, synthParams.delayTime->load());
            }
            const float fb = juce::jlimit (0.0f, 0.95f, synthParams.delayFb->load());
            delaySmoothedL.setTargetValue ((float) (dt * sr));
            delaySmoothedR.setTargetValue ((float) (dt * sr * 1.07f));
            for (int i = 0; i < n; ++i)
            {
                delayL.setDelay (delaySmoothedL.getNextValue());
                delayR.setDelay (delaySmoothedR.getNextValue());
                const float dlOut = delayL.popSample (0);
                const float drOut = delayR.popSample (0);
                delayL.pushSample (0, L[i] + drOut * fb);
                delayR.pushSample (0, R[i] + dlOut * fb);
                L[i] += dlOut * dMix;
                R[i] += drOut * dMix;
            }
        }
    }

    // Reverb
    {
        const float rMix = *synthParams.reverb;
        if (rMix > 0.001f)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = juce::jlimit (0.0f, 1.0f, synthParams.reverbSize->load());
            rp.damping  = juce::jlimit (0.0f, 1.0f, synthParams.reverbDamp->load());
            rp.wetLevel = rMix * 0.5f;
            rp.dryLevel = 1.0f;
            rp.width    = 1.0f;
            reverbFx.setParameters (rp);
            reverbFx.processStereo (L, R, n);
        }
    }

    // Master width (M/S) and gain
    {
        const float w = juce::jlimit (0.0f, 2.0f, synthParams.width->load() + synthParams.modSum[ModDest::Width] * 0.5f);
        const float g = juce::Decibels::decibelsToGain (synthParams.gain->load());
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            const float s = 0.5f * (L[i] - R[i]) * w;
            L[i] = (m + s) * g;
            R[i] = (m - s) * g;
        }
    }

    // Compressor (post)
    if (*synthParams.compOn > 0.5f)
    {
        compFx.setThreshold (*synthParams.compThr);
        compFx.setRatio (*synthParams.compRatio);
        compFx.setAttack (*synthParams.compAtk);
        compFx.setRelease (*synthParams.compRel);
        juce::dsp::AudioBlock<float> blk (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        compFx.process (ctx);
        const float mk = juce::Decibels::decibelsToGain (synthParams.compMakeup->load());
        if (std::abs (mk - 1.0f) > 0.001f) buffer.applyGain (mk);
    }

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

int VaporKeyAudioProcessor::getNumPrograms() { return (int) VKPresets::all().size(); }
int VaporKeyAudioProcessor::getCurrentProgram() { return currentProgram; }
void VaporKeyAudioProcessor::setCurrentProgram (int idx)
{
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, idx);
    loadFactoryPreset (currentProgram);
}
const juce::String VaporKeyAudioProcessor::getProgramName (int idx)
{
    const auto& list = VKPresets::all();
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

    delayL.reset();
    delayR.reset();
    delaySmoothedL.setCurrentAndTargetValue (0.0f);
    delaySmoothedR.setCurrentAndTargetValue (0.0f);

    chorusFx.reset();
    phaserFx.reset();
    compFx.reset();
    eqLowL.reset();  eqLowR.reset();
    eqMidL.reset();  eqMidR.reset();
    eqHighL.reset(); eqHighR.reset();
    reverbFx.reset();
}

void VaporKeyAudioProcessor::loadFactoryPreset (int index)
{
    const auto& list = VKPresets::all();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;

    silenceForPresetSwitch();

    // Reset to defaults first by re-creating defaults from parameter ranges.
    for (auto* param : getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
            p->setValueNotifyingHost (p->getDefaultValue());

    for (const auto& kv : list[(size_t) index].values)
    {
        if (auto* p = apvts.getParameter (kv.id))
        {
            const auto& range = p->getNormalisableRange();
            const float norm = range.convertTo0to1 (kv.v);
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
        }
    }

    currentPresetName = juce::String (list[(size_t) index].name);
    currentPresetIsFactory = true;
}

juce::File VaporKeyAudioProcessor::getUserPresetsDir() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("RocketBombs")
                   .getChildFile ("VaporKey")
                   .getChildFile ("Presets");
    if (! dir.exists()) dir.createDirectory();
    return dir;
}

juce::StringArray VaporKeyAudioProcessor::getUserPresetNames() const
{
    juce::StringArray names;
    auto dir = getUserPresetsDir();
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.vkpreset");
    for (auto& f : files) names.add (f.getFileNameWithoutExtension());
    names.sort (false);
    return names;
}

bool VaporKeyAudioProcessor::saveUserPreset (const juce::String& name)
{
    auto safeName = juce::File::createLegalFileName (name).trim();
    if (safeName.isEmpty()) return false;
    auto file = getUserPresetsDir().getChildFile (safeName + ".vkpreset");
    if (auto xml = apvts.copyState().createXml())
    {
        if (! xml->writeTo (file)) return false;
        currentPresetName = safeName;
        currentPresetIsFactory = false;
        return true;
    }
    return false;
}

bool VaporKeyAudioProcessor::loadUserPresetByName (const juce::String& name)
{
    auto file = getUserPresetsDir().getChildFile (name + ".vkpreset");
    if (! file.existsAsFile()) return false;
    if (auto xml = juce::XmlDocument::parse (file))
    {
        silenceForPresetSwitch();
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        currentPresetName = name;
        currentPresetIsFactory = false;
        return true;
    }
    return false;
}

bool VaporKeyAudioProcessor::deleteUserPreset (const juce::String& name)
{
    auto file = getUserPresetsDir().getChildFile (name + ".vkpreset");
    if (! file.existsAsFile()) return false;
    const bool ok = file.deleteFile();
    if (ok && currentPresetName == name && ! currentPresetIsFactory)
        currentPresetName.clear();
    return ok;
}

bool VaporKeyAudioProcessor::renameUserPreset (const juce::String& oldName, const juce::String& newName)
{
    auto safeNew = juce::File::createLegalFileName (newName).trim();
    if (safeNew.isEmpty() || safeNew == oldName) return false;
    auto src = getUserPresetsDir().getChildFile (oldName + ".vkpreset");
    auto dst = getUserPresetsDir().getChildFile (safeNew + ".vkpreset");
    if (! src.existsAsFile() || dst.existsAsFile()) return false;
    if (! src.moveFileTo (dst)) return false;
    if (currentPresetName == oldName && ! currentPresetIsFactory)
        currentPresetName = safeNew;
    return true;
}

juce::StringArray VaporKeyAudioProcessor::factoryPresetNames()
{
    juce::StringArray names;
    for (const auto& p : VKPresets::all()) names.add (p.name);
    return names;
}

bool VaporKeyAudioProcessor::loadCustomWavetable (int oscIndex, const juce::File& file)
{
    if (oscIndex < 0 || oscIndex >= 3) return false;
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

    std::atomic_store (&synthParams.customTables[oscIndex], newTable);
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
