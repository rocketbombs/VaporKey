#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthVoice.h"

namespace IDs {
    // Per-osc helpers
    inline juce::String on  (int i) { return "osc" + juce::String (i + 1) + "_on"; }
    inline juce::String sh  (int i) { return "osc" + juce::String (i + 1) + "_shape"; }
    inline juce::String po  (int i) { return "osc" + juce::String (i + 1) + "_pos"; }
    inline juce::String lv  (int i) { return "osc" + juce::String (i + 1) + "_level"; }
    inline juce::String pa  (int i) { return "osc" + juce::String (i + 1) + "_pan"; }
    inline juce::String co  (int i) { return "osc" + juce::String (i + 1) + "_coarse"; }
    inline juce::String fi  (int i) { return "osc" + juce::String (i + 1) + "_fine"; }
    inline juce::String un  (int i) { return "osc" + juce::String (i + 1) + "_unison"; }
    inline juce::String dt  (int i) { return "osc" + juce::String (i + 1) + "_detune"; }
}

juce::AudioProcessorValueTreeState::ParameterLayout VaporKeyAudioProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using PI = juce::AudioParameterInt;
    using PB = juce::AudioParameterBool;
    using PC = juce::AudioParameterChoice;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;

    juce::StringArray shapes;
    for (int i = 0; i < WavetableLibrary::NumShapes; ++i) shapes.add (WavetableLibrary::shapeName (i));

    for (int i = 0; i < 3; ++i)
    {
        v.push_back (std::make_unique<PB> (juce::ParameterID { IDs::on (i), 1 }, "Osc " + juce::String (i+1) + " On", i == 0));
        v.push_back (std::make_unique<PC> (juce::ParameterID { IDs::sh (i), 1 }, "Osc " + juce::String (i+1) + " Shape", shapes, 0));
        v.push_back (std::make_unique<P>  (juce::ParameterID { IDs::po (i), 1 }, "Osc " + juce::String (i+1) + " Position",
                                            juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
        v.push_back (std::make_unique<P>  (juce::ParameterID { IDs::lv (i), 1 }, "Osc " + juce::String (i+1) + " Level",
                                            juce::NormalisableRange<float> (-60.0f, 6.0f), -6.0f));
        v.push_back (std::make_unique<P>  (juce::ParameterID { IDs::pa (i), 1 }, "Osc " + juce::String (i+1) + " Pan",
                                            juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
        v.push_back (std::make_unique<P>  (juce::ParameterID { IDs::co (i), 1 }, "Osc " + juce::String (i+1) + " Coarse",
                                            juce::NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f));
        v.push_back (std::make_unique<P>  (juce::ParameterID { IDs::fi (i), 1 }, "Osc " + juce::String (i+1) + " Fine",
                                            juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f));
        v.push_back (std::make_unique<PI> (juce::ParameterID { IDs::un (i), 1 }, "Osc " + juce::String (i+1) + " Unison", 1, 7, 1));
        v.push_back (std::make_unique<P>  (juce::ParameterID { IDs::dt (i), 1 }, "Osc " + juce::String (i+1) + " Detune",
                                            juce::NormalisableRange<float> (0.0f, 1.0f), 0.2f));
    }

    v.push_back (std::make_unique<P>  (juce::ParameterID { "f_cut",  1 }, "Cutoff",
                                        juce::NormalisableRange<float> (20.0f, 18000.0f, 0.0f, 0.3f), 12000.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "f_res",  1 }, "Resonance", juce::NormalisableRange<float> (0.1f, 1.0f), 0.3f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "f_env",  1 }, "Env -> Cutoff", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<PC> (juce::ParameterID { "f_type", 1 }, "Filter Type", juce::StringArray { "LP", "BP", "HP" }, 0));

    auto envRange = juce::NormalisableRange<float> (0.001f, 8.0f, 0.0f, 0.4f);
    v.push_back (std::make_unique<P> (juce::ParameterID { "a_a", 1 }, "Amp A", envRange, 0.005f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "a_d", 1 }, "Amp D", envRange, 0.4f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "a_s", 1 }, "Amp S", juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "a_r", 1 }, "Amp R", envRange, 0.3f));

    v.push_back (std::make_unique<P> (juce::ParameterID { "m_a", 1 }, "Mod A", envRange, 0.005f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "m_d", 1 }, "Mod D", envRange, 0.5f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "m_s", 1 }, "Mod S", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "m_r", 1 }, "Mod R", envRange, 0.3f));

    auto rateRange = juce::NormalisableRange<float> (0.05f, 20.0f, 0.0f, 0.4f);
    v.push_back (std::make_unique<P> (juce::ParameterID { "lfo1_rate", 1 }, "LFO1 Rate", rateRange, 4.0f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "lfo1_amt",  1 }, "LFO1 -> Cutoff", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "lfo2_rate", 1 }, "LFO2 Rate", rateRange, 0.5f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "lfo2_amt",  1 }, "LFO2 -> Position", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));

    v.push_back (std::make_unique<P> (juce::ParameterID { "grit",  1 }, "Grit",  juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "vibe",  1 }, "Vibe",  juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "drift", 1 }, "Drift", juce::NormalisableRange<float> (0.0f, 1.0f), 0.15f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "sat",   1 }, "Sat",   juce::NormalisableRange<float> (0.0f, 1.0f), 0.1f));

    v.push_back (std::make_unique<P> (juce::ParameterID { "chorus", 1 }, "Chorus", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "delay",  1 }, "Delay Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "delay_time", 1 }, "Delay Time", juce::NormalisableRange<float> (0.02f, 1.5f), 0.4f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "delay_fb",   1 }, "Delay FB",   juce::NormalisableRange<float> (0.0f, 0.95f), 0.4f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "reverb", 1 }, "Reverb", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    v.push_back (std::make_unique<P> (juce::ParameterID { "gain",  1 }, "Master", juce::NormalisableRange<float> (-60.0f, 6.0f), -6.0f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "glide", 1 }, "Glide",  juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    return { v.begin(), v.end() };
}

VaporKeyAudioProcessor::VaporKeyAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    cacheParams();

    // Trigger wavetable build now
    WavetableLibrary::get();

    synth.addSound (new WTSound());
    for (int i = 0; i < 16; ++i)
        synth.addVoice (new WTVoice (synthParams));
}

void VaporKeyAudioProcessor::cacheParams()
{
    auto getF = [this] (const juce::String& id)
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    for (int i = 0; i < 3; ++i)
    {
        synthParams.osc[i].on       = getF (IDs::on (i));
        synthParams.osc[i].shape    = getF (IDs::sh (i));
        synthParams.osc[i].position = getF (IDs::po (i));
        synthParams.osc[i].level    = getF (IDs::lv (i));
        synthParams.osc[i].pan      = getF (IDs::pa (i));
        synthParams.osc[i].coarse   = getF (IDs::co (i));
        synthParams.osc[i].fine     = getF (IDs::fi (i));
        synthParams.osc[i].unison   = getF (IDs::un (i));
        synthParams.osc[i].detune   = getF (IDs::dt (i));
    }

    synthParams.fCut = getF ("f_cut"); synthParams.fRes = getF ("f_res");
    synthParams.fEnv = getF ("f_env"); synthParams.fType = getF ("f_type");

    synthParams.aA = getF ("a_a"); synthParams.aD = getF ("a_d");
    synthParams.aS = getF ("a_s"); synthParams.aR = getF ("a_r");
    synthParams.mA = getF ("m_a"); synthParams.mD = getF ("m_d");
    synthParams.mS = getF ("m_s"); synthParams.mR = getF ("m_r");

    synthParams.lfo1Rate = getF ("lfo1_rate"); synthParams.lfo1Amt = getF ("lfo1_amt");
    synthParams.lfo2Rate = getF ("lfo2_rate"); synthParams.lfo2Amt = getF ("lfo2_amt");

    synthParams.grit  = getF ("grit");
    synthParams.vibe  = getF ("vibe");
    synthParams.drift = getF ("drift");
    synthParams.sat   = getF ("sat");

    synthParams.chorus    = getF ("chorus");
    synthParams.delay     = getF ("delay");
    synthParams.delayTime = getF ("delay_time");
    synthParams.delayFb   = getF ("delay_fb");
    synthParams.reverb    = getF ("reverb");

    synthParams.gain  = getF ("gain");
    synthParams.glide = getF ("glide");
}

void VaporKeyAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<WTVoice*> (synth.getVoice (i)))
            v->prepare (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    chorusFx.prepare (spec);
    chorusFx.setCentreDelay (7.0f);
    chorusFx.setFeedback (0.2f);
    delayL.prepare (spec); delayL.setMaximumDelayInSamples ((int) (sampleRate * 2.0));
    delayR.prepare (spec); delayR.setMaximumDelayInSamples ((int) (sampleRate * 2.0));
    delayL.reset(); delayR.reset();
    reverbFx.setSampleRate (sampleRate);
}

bool VaporKeyAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void VaporKeyAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nodn;
    buffer.clear();
    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    const int numCh = buffer.getNumChannels();
    if (numCh < 2) return;

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    const int n = buffer.getNumSamples();

    // Chorus
    const float chMix = *synthParams.chorus;
    if (chMix > 0.001f)
    {
        chorusFx.setMix (chMix);
        chorusFx.setRate (0.6f);
        chorusFx.setDepth (0.4f * chMix);
        juce::dsp::AudioBlock<float> blk (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        chorusFx.process (ctx);
    }

    // Stereo delay (ping-pong-ish)
    const float dMix = *synthParams.delay;
    if (dMix > 0.001f)
    {
        const float dt = juce::jlimit (0.005f, 1.5f, synthParams.delayTime->load());
        const float fb = juce::jlimit (0.0f, 0.95f, synthParams.delayFb->load());
        delayL.setDelay ((float) (dt * sr));
        delayR.setDelay ((float) (dt * sr * 1.07f));
        for (int i = 0; i < n; ++i)
        {
            const float dlOut = delayL.popSample (0);
            const float drOut = delayR.popSample (0);
            delayL.pushSample (0, L[i] + drOut * fb);
            delayR.pushSample (0, R[i] + dlOut * fb);
            L[i] += dlOut * dMix;
            R[i] += drOut * dMix;
        }
    }

    // Reverb
    const float rMix = *synthParams.reverb;
    if (rMix > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.55f + rMix * 0.4f;
        rp.damping  = 0.4f;
        rp.wetLevel = rMix * 0.5f;
        rp.dryLevel = 1.0f;
        rp.width    = 1.0f;
        reverbFx.setParameters (rp);
        reverbFx.processStereo (L, R, n);
    }

    // Master gain
    const float g = juce::Decibels::decibelsToGain (synthParams.gain->load());
    buffer.applyGain (g);
}

juce::AudioProcessorEditor* VaporKeyAudioProcessor::createEditor()
{
    return new VaporKeyAudioProcessorEditor (*this);
}

void VaporKeyAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VaporKeyAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VaporKeyAudioProcessor();
}
