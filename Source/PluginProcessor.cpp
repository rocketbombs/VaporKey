#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthVoice.h"
#include "Presets.h"
#include <algorithm>
#include <initializer_list>

namespace IDs {
    inline juce::String on  (int i) { return "osc" + juce::String (i + 1) + "_on"; }
    inline juce::String sh  (int i) { return "osc" + juce::String (i + 1) + "_shape"; }
    inline juce::String po  (int i) { return "osc" + juce::String (i + 1) + "_pos"; }
    inline juce::String lv  (int i) { return "osc" + juce::String (i + 1) + "_level"; }
    inline juce::String pa  (int i) { return "osc" + juce::String (i + 1) + "_pan"; }
    inline juce::String co  (int i) { return "osc" + juce::String (i + 1) + "_coarse"; }
    inline juce::String fi  (int i) { return "osc" + juce::String (i + 1) + "_fine"; }
    inline juce::String un  (int i) { return "osc" + juce::String (i + 1) + "_unison"; }
    inline juce::String dt  (int i) { return "osc" + juce::String (i + 1) + "_detune"; }
    inline juce::String ph  (int i) { return "osc" + juce::String (i + 1) + "_phase"; }
}

juce::AudioProcessorValueTreeState::ParameterLayout VaporKeyAudioProcessor::createLayout()
{
    using P  = juce::AudioParameterFloat;
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
        v.push_back (std::make_unique<P>  (juce::ParameterID { IDs::po (i), 1 }, "Osc " + juce::String (i+1) + " Position", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
        v.push_back (std::make_unique<P>  (juce::ParameterID { IDs::lv (i), 1 }, "Osc " + juce::String (i+1) + " Level", juce::NormalisableRange<float> (-60.0f, 6.0f), -6.0f));
        v.push_back (std::make_unique<P>  (juce::ParameterID { IDs::pa (i), 1 }, "Osc " + juce::String (i+1) + " Pan", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
        v.push_back (std::make_unique<P>  (juce::ParameterID { IDs::co (i), 1 }, "Osc " + juce::String (i+1) + " Coarse", juce::NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f));
        v.push_back (std::make_unique<P>  (juce::ParameterID { IDs::fi (i), 1 }, "Osc " + juce::String (i+1) + " Fine", juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f));
        v.push_back (std::make_unique<PI> (juce::ParameterID { IDs::un (i), 1 }, "Osc " + juce::String (i+1) + " Unison", 1, 7, 1));
        v.push_back (std::make_unique<P>  (juce::ParameterID { IDs::dt (i), 1 }, "Osc " + juce::String (i+1) + " Detune", juce::NormalisableRange<float> (0.0f, 1.0f), 0.2f));
        v.push_back (std::make_unique<P>  (juce::ParameterID { IDs::ph (i), 1 }, "Osc " + juce::String (i+1) + " Phase", juce::NormalisableRange<float> (-1.0f, 1.0f), -1.0f));
    }

    // Sub
    v.push_back (std::make_unique<PB> (juce::ParameterID { "sub_on", 1 }, "Sub On", false));
    v.push_back (std::make_unique<PC> (juce::ParameterID { "sub_shape", 1 }, "Sub Shape", SubShape::names(), 0));
    v.push_back (std::make_unique<PI> (juce::ParameterID { "sub_oct", 1 }, "Sub Octave", -2, -1, -1));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "sub_level", 1 }, "Sub Level", juce::NormalisableRange<float> (-60.0f, 0.0f), -12.0f));

    // Noise
    v.push_back (std::make_unique<PB> (juce::ParameterID { "noise_on", 1 }, "Noise On", false));
    v.push_back (std::make_unique<PC> (juce::ParameterID { "noise_color", 1 }, "Noise Color", NoiseColor::names(), 0));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "noise_level", 1 }, "Noise Level", juce::NormalisableRange<float> (-60.0f, 0.0f), -24.0f));

    // Filter
    v.push_back (std::make_unique<P>  (juce::ParameterID { "f_cut", 1 }, "Cutoff", juce::NormalisableRange<float> (20.0f, 18000.0f, 0.0f, 0.3f), 12000.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "f_res", 1 }, "Resonance", juce::NormalisableRange<float> (0.1f, 1.0f), 0.3f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "f_env", 1 }, "Env -> Cutoff", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<PC> (juce::ParameterID { "f_type", 1 }, "Filter Type", juce::StringArray { "LP", "BP", "HP" }, 0));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "f_drive", 1 }, "Filter Drive", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "f_key", 1 }, "Key Track", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    // Envelopes
    auto envRange = juce::NormalisableRange<float> (0.001f, 8.0f, 0.0f, 0.4f);
    v.push_back (std::make_unique<P> (juce::ParameterID { "a_a", 1 }, "Amp A", envRange, 0.005f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "a_d", 1 }, "Amp D", envRange, 0.4f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "a_s", 1 }, "Amp S", juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "a_r", 1 }, "Amp R", envRange, 0.3f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "a_vel", 1 }, "Amp Vel", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    v.push_back (std::make_unique<P> (juce::ParameterID { "m_a", 1 }, "Mod A", envRange, 0.005f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "m_d", 1 }, "Mod D", envRange, 0.5f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "m_s", 1 }, "Mod S", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "m_r", 1 }, "Mod R", envRange, 0.3f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "f_vel", 1 }, "Filter Env Vel", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    v.push_back (std::make_unique<P> (juce::ParameterID { "p_env_amt", 1 }, "Pitch Env Amt", juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "p_env_decay", 1 }, "Pitch Env Decay", juce::NormalisableRange<float> (0.005f, 2.0f, 0.0f, 0.4f), 0.1f));

    // LFOs
    auto rateRange = juce::NormalisableRange<float> (0.05f, 30.0f, 0.0f, 0.4f);
    v.push_back (std::make_unique<PC> (juce::ParameterID { "lfo1_shape", 1 }, "LFO1 Shape", LfoShape::names(), 0));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "lfo1_rate", 1 }, "LFO1 Rate", rateRange, 4.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "lfo1_amt", 1 }, "LFO1 -> Cutoff", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<PB> (juce::ParameterID { "lfo1_sync", 1 }, "LFO1 Sync", false));
    v.push_back (std::make_unique<PC> (juce::ParameterID { "lfo1_div", 1 }, "LFO1 Div", syncDivNames(), 4));
    v.push_back (std::make_unique<PC> (juce::ParameterID { "lfo2_shape", 1 }, "LFO2 Shape", LfoShape::names(), 0));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "lfo2_rate", 1 }, "LFO2 Rate", rateRange, 0.5f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "lfo2_amt", 1 }, "LFO2 -> Position", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<PB> (juce::ParameterID { "lfo2_sync", 1 }, "LFO2 Sync", false));
    v.push_back (std::make_unique<PC> (juce::ParameterID { "lfo2_div", 1 }, "LFO2 Div", syncDivNames(), 4));

    // Glide / mono
    v.push_back (std::make_unique<P>  (juce::ParameterID { "glide", 1 }, "Glide", juce::NormalisableRange<float> (0.0f, 2.0f, 0.0f, 0.4f), 0.0f));
    v.push_back (std::make_unique<PB> (juce::ParameterID { "mono", 1 }, "Mono", false));
    v.push_back (std::make_unique<PB> (juce::ParameterID { "legato", 1 }, "Legato", true));
    v.push_back (std::make_unique<PI> (juce::ParameterID { "bend_range", 1 }, "Bend Range", 1, 24, 2));

    // Warmth
    v.push_back (std::make_unique<P> (juce::ParameterID { "grit", 1 }, "Grit", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "vibe", 1 }, "Vibe", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "drift", 1 }, "Drift", juce::NormalisableRange<float> (0.0f, 1.0f), 0.15f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "sat", 1 }, "Sat", juce::NormalisableRange<float> (0.0f, 1.0f), 0.1f));

    // Distortion
    v.push_back (std::make_unique<P>  (juce::ParameterID { "dist_drive", 1 }, "Dist Drive", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "dist_mix", 1 }, "Dist Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<PC> (juce::ParameterID { "dist_type", 1 }, "Dist Type", DistType::names(), 0));

    // Chorus
    v.push_back (std::make_unique<P>  (juce::ParameterID { "chorus", 1 }, "Chorus", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "chorus_rate", 1 }, "Chorus Rate", juce::NormalisableRange<float> (0.05f, 5.0f), 0.6f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "chorus_depth", 1 }, "Chorus Depth", juce::NormalisableRange<float> (0.0f, 1.0f), 0.4f));

    // Phaser
    v.push_back (std::make_unique<P>  (juce::ParameterID { "phaser", 1 }, "Phaser", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "phaser_rate", 1 }, "Phaser Rate", juce::NormalisableRange<float> (0.05f, 8.0f), 0.4f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "phaser_depth", 1 }, "Phaser Depth", juce::NormalisableRange<float> (0.0f, 1.0f), 0.6f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "phaser_fb", 1 }, "Phaser FB", juce::NormalisableRange<float> (-0.95f, 0.95f), 0.3f));

    // EQ
    v.push_back (std::make_unique<P>  (juce::ParameterID { "eq_low", 1 }, "EQ Low", juce::NormalisableRange<float> (-18.0f, 18.0f), 0.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "eq_mid", 1 }, "EQ Mid", juce::NormalisableRange<float> (-18.0f, 18.0f), 0.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "eq_mid_freq", 1 }, "EQ Mid Freq", juce::NormalisableRange<float> (200.0f, 6000.0f, 0.0f, 0.3f), 1000.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "eq_high", 1 }, "EQ High", juce::NormalisableRange<float> (-18.0f, 18.0f), 0.0f));

    // Delay
    v.push_back (std::make_unique<P>  (juce::ParameterID { "delay", 1 }, "Delay Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "delay_time", 1 }, "Delay Time", juce::NormalisableRange<float> (0.02f, 1.5f), 0.4f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "delay_fb", 1 }, "Delay FB", juce::NormalisableRange<float> (0.0f, 0.95f), 0.4f));
    v.push_back (std::make_unique<PB> (juce::ParameterID { "delay_sync", 1 }, "Delay Sync", false));
    v.push_back (std::make_unique<PC> (juce::ParameterID { "delay_div", 1 }, "Delay Div", syncDivNames(), 3));

    // Reverb
    v.push_back (std::make_unique<P>  (juce::ParameterID { "reverb", 1 }, "Reverb", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "reverb_size", 1 }, "Reverb Size", juce::NormalisableRange<float> (0.0f, 1.0f), 0.6f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "reverb_damp", 1 }, "Reverb Damp", juce::NormalisableRange<float> (0.0f, 1.0f), 0.4f));

    // Compressor
    v.push_back (std::make_unique<PB> (juce::ParameterID { "comp_on", 1 }, "Comp On", false));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "comp_thr", 1 }, "Comp Thresh", juce::NormalisableRange<float> (-60.0f, 0.0f), -18.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "comp_ratio", 1 }, "Comp Ratio", juce::NormalisableRange<float> (1.0f, 20.0f), 4.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "comp_atk", 1 }, "Comp Attack", juce::NormalisableRange<float> (0.1f, 200.0f, 0.0f, 0.4f), 10.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "comp_rel", 1 }, "Comp Release", juce::NormalisableRange<float> (5.0f, 1000.0f, 0.0f, 0.4f), 100.0f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "comp_makeup", 1 }, "Comp Makeup", juce::NormalisableRange<float> (-12.0f, 24.0f), 0.0f));

    // Master
    v.push_back (std::make_unique<P> (juce::ParameterID { "gain", 1 }, "Master", juce::NormalisableRange<float> (-60.0f, 6.0f), -6.0f));
    v.push_back (std::make_unique<P> (juce::ParameterID { "width", 1 }, "Width", juce::NormalisableRange<float> (0.0f, 2.0f), 1.0f));

    // Arpeggiator
    v.push_back (std::make_unique<PB> (juce::ParameterID { "arp_on", 1 }, "Arp On", false));
    v.push_back (std::make_unique<PC> (juce::ParameterID { "arp_mode", 1 }, "Arp Mode", ArpMode::names(), 0));
    v.push_back (std::make_unique<PC> (juce::ParameterID { "arp_div", 1 }, "Arp Rate", syncDivNames(), 1));
    v.push_back (std::make_unique<PI> (juce::ParameterID { "arp_octaves", 1 }, "Arp Octaves", 1, 4, 1));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "arp_gate", 1 }, "Arp Gate", juce::NormalisableRange<float> (0.05f, 1.0f), 0.5f));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "arp_swing", 1 }, "Arp Swing", juce::NormalisableRange<float> (0.0f, 0.5f), 0.0f));
    v.push_back (std::make_unique<PB> (juce::ParameterID { "arp_latch", 1 }, "Arp Latch", false));

    // Macros
    auto destNames = ModDest::names();
    for (int m = 0; m < SynthParams::kNumMacros; ++m)
    {
        v.push_back (std::make_unique<P>  (juce::ParameterID { "macro" + juce::String (m + 1) + "_val", 1 },
                                            "Macro " + juce::String (m + 1), juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
        v.push_back (std::make_unique<PC> (juce::ParameterID { "macro" + juce::String (m + 1) + "_dest", 1 },
                                            "Macro " + juce::String (m + 1) + " Dest", destNames, 0));
        v.push_back (std::make_unique<P>  (juce::ParameterID { "macro" + juce::String (m + 1) + "_amt", 1 },
                                            "Macro " + juce::String (m + 1) + " Amt", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
    }

    return { v.begin(), v.end() };
}

VaporKeyAudioProcessor::VaporKeyAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    cacheParams();
    WavetableLibrary::get();

    // Decorrelate per-instance RNGs. Without this every plugin instance starts
    // its arpeggiator (and below: each voice's noise/grit generator) with the
    // same JUCE default seed, so multiple instances produce identical random
    // streams - which sums coherently and sounds buzzy/aliased.
    arpRng.setSeedRandomly();

    synth.addSound (new WTSound());
    for (int i = 0; i < 16; ++i)
        synth.addVoice (new WTVoice (synthParams));
    synth.setNoteStealingEnabled (true);
}

void VaporKeyAudioProcessor::cacheParams()
{
    auto getF = [this] (const juce::String& id) {
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
        synthParams.osc[i].phase    = getF (IDs::ph (i));
    }

    synthParams.subOn = getF ("sub_on"); synthParams.subShape = getF ("sub_shape");
    synthParams.subOct = getF ("sub_oct"); synthParams.subLevel = getF ("sub_level");

    synthParams.noiseOn = getF ("noise_on"); synthParams.noiseColor = getF ("noise_color");
    synthParams.noiseLevel = getF ("noise_level");

    synthParams.fCut = getF ("f_cut"); synthParams.fRes = getF ("f_res");
    synthParams.fEnv = getF ("f_env"); synthParams.fType = getF ("f_type");
    synthParams.fDrive = getF ("f_drive"); synthParams.fKey = getF ("f_key");

    synthParams.aA = getF ("a_a"); synthParams.aD = getF ("a_d");
    synthParams.aS = getF ("a_s"); synthParams.aR = getF ("a_r");
    synthParams.aVel = getF ("a_vel");
    synthParams.mA = getF ("m_a"); synthParams.mD = getF ("m_d");
    synthParams.mS = getF ("m_s"); synthParams.mR = getF ("m_r");
    synthParams.fVel = getF ("f_vel");
    synthParams.pEnvAmt = getF ("p_env_amt"); synthParams.pEnvDecay = getF ("p_env_decay");

    synthParams.lfo1Shape = getF ("lfo1_shape"); synthParams.lfo1Rate = getF ("lfo1_rate");
    synthParams.lfo1Amt = getF ("lfo1_amt");
    synthParams.lfo1Sync = getF ("lfo1_sync"); synthParams.lfo1Div = getF ("lfo1_div");
    synthParams.lfo2Shape = getF ("lfo2_shape"); synthParams.lfo2Rate = getF ("lfo2_rate");
    synthParams.lfo2Amt = getF ("lfo2_amt");
    synthParams.lfo2Sync = getF ("lfo2_sync"); synthParams.lfo2Div = getF ("lfo2_div");

    synthParams.glide = getF ("glide");
    synthParams.mono = getF ("mono"); synthParams.legato = getF ("legato");
    synthParams.bendRange = getF ("bend_range");

    synthParams.grit = getF ("grit"); synthParams.vibe = getF ("vibe");
    synthParams.drift = getF ("drift"); synthParams.sat = getF ("sat");

    synthParams.distDrive = getF ("dist_drive"); synthParams.distMix = getF ("dist_mix");
    synthParams.distType  = getF ("dist_type");
    synthParams.chorus = getF ("chorus");
    synthParams.chorusRate = getF ("chorus_rate"); synthParams.chorusDepth = getF ("chorus_depth");
    synthParams.phaser = getF ("phaser"); synthParams.phaserRate = getF ("phaser_rate");
    synthParams.phaserDepth = getF ("phaser_depth"); synthParams.phaserFb = getF ("phaser_fb");
    synthParams.eqLow = getF ("eq_low"); synthParams.eqMid = getF ("eq_mid");
    synthParams.eqMidF = getF ("eq_mid_freq"); synthParams.eqHigh = getF ("eq_high");
    synthParams.delay = getF ("delay"); synthParams.delayTime = getF ("delay_time");
    synthParams.delayFb = getF ("delay_fb");
    synthParams.delaySync = getF ("delay_sync"); synthParams.delayDiv = getF ("delay_div");
    synthParams.reverb = getF ("reverb"); synthParams.reverbSize = getF ("reverb_size");
    synthParams.reverbDamp = getF ("reverb_damp");
    synthParams.compOn = getF ("comp_on"); synthParams.compThr = getF ("comp_thr");
    synthParams.compRatio = getF ("comp_ratio");
    synthParams.compAtk = getF ("comp_atk"); synthParams.compRel = getF ("comp_rel");
    synthParams.compMakeup = getF ("comp_makeup");

    synthParams.gain = getF ("gain"); synthParams.width = getF ("width");

    for (int m = 0; m < SynthParams::kNumMacros; ++m)
    {
        const juce::String prefix = "macro" + juce::String (m + 1) + "_";
        synthParams.macroVal[m]  = getF (prefix + "val");
        synthParams.macroDest[m] = getF (prefix + "dest");
        synthParams.macroAmt[m]  = getF (prefix + "amt");
    }

    synthParams.arpOn      = getF ("arp_on");
    synthParams.arpMode    = getF ("arp_mode");
    synthParams.arpDiv     = getF ("arp_div");
    synthParams.arpOctaves = getF ("arp_octaves");
    synthParams.arpGate    = getF ("arp_gate");
    synthParams.arpSwing   = getF ("arp_swing");
    synthParams.arpLatch   = getF ("arp_latch");
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
    arpPassBuf.ensureSize (8192);
    monoFilterBuf.ensureSize (8192);
    arpNoteEventsBuf.ensureStorageAllocated (256);
    arpNoteSamplesBuf.ensureStorageAllocated (256);
    arpActiveBuf.ensureStorageAllocated (32);
    arpOrderedBuf.ensureStorageAllocated (32);

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
}

void VaporKeyAudioProcessor::processArpeggiator (juce::MidiBuffer& midi, int numSamples)
{
    const bool on    = *synthParams.arpOn > 0.5f;
    const bool latch = *synthParams.arpLatch > 0.5f;

    // Reuse pre-allocated scratch buffers so we never call malloc on the audio
    // thread (multi-instance setups serialize on the heap lock and freeze).
    auto& pass        = arpPassBuf;
    auto& noteEvents  = arpNoteEventsBuf;
    auto& noteSamples = arpNoteSamplesBuf;
    pass.clear();
    noteEvents.clearQuick();
    noteSamples.clearQuick();

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOnOrOff())
        {
            noteEvents.add (msg);
            noteSamples.add (meta.samplePosition);
        }
        else
        {
            pass.addEvent (msg, meta.samplePosition);
        }
    }

    // Transition into/out of arp mode: clear pending state cleanly.
    if (on != arpWasOn)
    {
        if (arpCurrentNote >= 0)
            pass.addEvent (juce::MidiMessage::noteOff (arpCurrentChan, arpCurrentNote), 0);
        arpCurrentNote = -1;
        arpSamplesToOff = -1;
        arpSamplesToStep = 0.0;
        arpStepIdx = 0;
        arpOctOffset = 0;
        arpUpDir = true;
        if (! on) { arpHeld.clearQuick(); arpLatched.clearQuick(); }
        arpWasOn = on;
    }

    if (! on)
    {
        // Pass note events through untouched.
        for (int i = 0; i < noteEvents.size(); ++i)
            pass.addEvent (noteEvents.getReference (i), noteSamples.getReference (i));
        midi.swapWith (pass);
        return;
    }

    // ---- Arp on ----
    // Step duration in samples, derived from tempo + sync division.
    const int divIdx = (int) (synthParams.arpDiv->load() + 0.5f);
    const double beats = syncDivToBeats (divIdx);
    const double bpm   = juce::jmax (20.0, currentBpm);
    const double stepSamples = juce::jmax (4.0, beats * 60.0 / bpm * sr);

    const int   mode    = (int) (synthParams.arpMode->load() + 0.5f);
    const int   numOct  = juce::jlimit (1, 4, (int) synthParams.arpOctaves->load());
    const float gate    = juce::jlimit (0.05f, 1.0f, synthParams.arpGate->load());
    const float swing   = juce::jlimit (0.0f, 0.5f, synthParams.arpSwing->load());

    // Walk the block, advancing time and emitting events at sub-block points.
    int cursor = 0;
    int eventIdx = 0;

    auto applyHeldNoteEvent = [this, latch] (const juce::MidiMessage& msg)
    {
        if (msg.isNoteOn())
        {
            // Latch behavior: a fresh press while no notes physically held should
            // start a new chord (clear latched buffer first).
            if (latch && arpHeld.isEmpty())
                arpLatched.clearQuick();

            const int n = msg.getNoteNumber();
            for (int j = arpHeld.size(); --j >= 0;)
                if (arpHeld.getReference (j).note == n) arpHeld.remove (j);
            arpHeld.add ({ n, msg.getVelocity() });

            if (latch)
            {
                for (int j = arpLatched.size(); --j >= 0;)
                    if (arpLatched.getReference (j).note == n) arpLatched.remove (j);
                arpLatched.add ({ n, msg.getVelocity() });
            }
        }
        else if (msg.isNoteOff())
        {
            const int n = msg.getNoteNumber();
            for (int j = arpHeld.size(); --j >= 0;)
                if (arpHeld.getReference (j).note == n) arpHeld.remove (j);
        }
    };

    auto pickStepNote = [&] (const juce::Array<ArpHeldNote>& source) -> ArpHeldNote
    {
        // Sorted copy for ordered modes - reuse a member buffer to avoid heap
        // allocation every step boundary.
        auto& ordered = arpOrderedBuf;
        ordered.clearQuick();
        ordered.addArray (source);
        std::sort (ordered.begin(), ordered.end(),
                   [] (const ArpHeldNote& a, const ArpHeldNote& b) { return a.note < b.note; });

        const int N = ordered.size();
        if (N == 0) return { -1, 0 };

        const int totalSteps = N * numOct;

        auto wrapStep = [&] (int s)
        {
            const int m = s % juce::jmax (1, totalSteps);
            return m < 0 ? m + totalSteps : m;
        };

        switch (mode)
        {
            case ArpMode::Up:
            {
                const int s = wrapStep (arpStepIdx);
                arpOctOffset = (s / N) * 12;
                return { ordered.getReference (s % N).note + arpOctOffset, ordered.getReference (s % N).velocity };
            }
            case ArpMode::Down:
            {
                const int s = wrapStep (arpStepIdx);
                const int rev = totalSteps - 1 - s;
                arpOctOffset = (rev / N) * 12;
                return { ordered.getReference (rev % N).note + arpOctOffset, ordered.getReference (rev % N).velocity };
            }
            case ArpMode::UpDown:
            {
                const int span = juce::jmax (1, 2 * totalSteps - 2);
                const int s    = ((arpStepIdx % span) + span) % span;
                const int idx  = (s < totalSteps) ? s : (span - s);
                arpOctOffset = (idx / N) * 12;
                return { ordered.getReference (idx % N).note + arpOctOffset, ordered.getReference (idx % N).velocity };
            }
            case ArpMode::DownUp:
            {
                const int span = juce::jmax (1, 2 * totalSteps - 2);
                const int s    = ((arpStepIdx % span) + span) % span;
                const int idxFwd = (s < totalSteps) ? s : (span - s);
                const int idx    = totalSteps - 1 - idxFwd;
                arpOctOffset = (idx / N) * 12;
                return { ordered.getReference (idx % N).note + arpOctOffset, ordered.getReference (idx % N).velocity };
            }
            case ArpMode::AsPlayed:
            {
                const int s = wrapStep (arpStepIdx);
                arpOctOffset = (s / N) * 12;
                return { source.getReference (s % N).note + arpOctOffset, source.getReference (s % N).velocity };
            }
            case ArpMode::Random:
            {
                const int oct = arpRng.nextInt (numOct);
                const int idx = arpRng.nextInt (N);
                return { ordered.getReference (idx).note + oct * 12, ordered.getReference (idx).velocity };
            }
        }
        return { -1, 0 };
    };

    while (cursor < numSamples)
    {
        // Apply any input note events at or before the cursor that we haven't yet processed.
        while (eventIdx < noteEvents.size() && noteSamples.getReference (eventIdx) <= cursor)
        {
            applyHeldNoteEvent (noteEvents.getReference (eventIdx));
            ++eventIdx;
        }

        // Active source for picking: physical held + (optionally) latched
        // extras. Reuse a member buffer so the inner loop allocates nothing.
        auto& activeSource = arpActiveBuf;
        activeSource.clearQuick();
        activeSource.addArray (arpHeld);
        if (latch)
        {
            for (const auto& e : arpLatched)
            {
                bool found = false;
                for (const auto& a : activeSource) if (a.note == e.note) { found = true; break; }
                if (! found) activeSource.add (e);
            }
        }

        // How many samples until the next event boundary?
        double samplesToNextStep = arpSamplesToStep;
        const double samplesToInputEvent = (eventIdx < noteEvents.size())
            ? (double) (noteSamples.getReference (eventIdx) - cursor) : 1e18;
        const double samplesToOff = (arpSamplesToOff >= 0) ? (double) arpSamplesToOff : 1e18;

        const double dt = std::min ({ (double) (numSamples - cursor),
                                       samplesToNextStep,
                                       samplesToInputEvent,
                                       samplesToOff });

        cursor += (int) dt;
        arpSamplesToStep -= dt;
        if (arpSamplesToOff >= 0) arpSamplesToOff -= (int) dt;

        // Fire scheduled note-off if it's now due.
        if (arpSamplesToOff == 0 && arpCurrentNote >= 0)
        {
            pass.addEvent (juce::MidiMessage::noteOff (arpCurrentChan, arpCurrentNote), juce::jmin (cursor, numSamples - 1));
            arpCurrentNote = -1;
            arpSamplesToOff = -1;
        }

        // Step boundary?
        if (arpSamplesToStep <= 0.0)
        {
            // End any still-sounding note (e.g. when gate == 1.0 it overlaps).
            if (arpCurrentNote >= 0)
            {
                pass.addEvent (juce::MidiMessage::noteOff (arpCurrentChan, arpCurrentNote), juce::jmin (cursor, numSamples - 1));
                arpCurrentNote = -1;
                arpSamplesToOff = -1;
            }

            if (! activeSource.isEmpty())
            {
                const ArpHeldNote pick = pickStepNote (activeSource);
                if (pick.note >= 0)
                {
                    const int note = juce::jlimit (0, 127, pick.note);
                    const int vel  = juce::jlimit (1, 127, pick.velocity > 0 ? pick.velocity : 100);
                    pass.addEvent (juce::MidiMessage::noteOn (arpCurrentChan, note, (juce::uint8) vel),
                                   juce::jmin (cursor, numSamples - 1));
                    arpCurrentNote = note;
                    arpSamplesToOff = juce::jmax (1, (int) (stepSamples * gate));
                }
            }

            // Schedule next step. Apply swing on odd steps (delay them).
            const bool oddStep = (arpStepIdx & 1) != 0;
            const double stepDur = stepSamples * (oddStep ? (1.0 + swing) : (1.0 - swing));
            arpSamplesToStep += stepDur;
            ++arpStepIdx;
        }
    }

    // Apply any remaining input events that landed past the loop's tail.
    while (eventIdx < noteEvents.size())
    {
        applyHeldNoteEvent (noteEvents.getReference (eventIdx));
        ++eventIdx;
    }

    midi.swapWith (pass);
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
            // Insert note-offs for currently held notes (last-note priority).
            for (int n : monoHeldNotes)
                if (n != msg.getNoteNumber())
                    out.addEvent (juce::MidiMessage::noteOff (msg.getChannel(), n), sample);

            if (legato && ! monoHeldNotes.isEmpty())
            {
                // For legato we still emit a note on (host expects it) — voices will retarget.
                // To keep envelope sustained, we ALSO mark this so the voice retargets without restart.
                // We handle that by sending a controller hint — easiest: just emit note-on; voice/synth
                // will steal and retrigger envs (acceptable behavior).
            }
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
                out.addEvent (msg, sample); // emit the off
                out.addEvent (juce::MidiMessage::noteOn (msg.getChannel(), n, (juce::uint8) 100), sample);
                continue;
            }
        }

        out.addEvent (msg, sample);
    }
    midi.swapWith (out);
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

    // Update macros and tempo info
    updateMacroSums();

    if (auto* ph = getPlayHead())
    {
        if (auto info = ph->getPosition())
            if (auto bpm = info->getBpm()) currentBpm = *bpm;
    }

    synthParams.bpm.store (currentBpm);

    // Arpeggiator runs first: it consumes incoming note-on/off events and emits
    // a stepped sequence into the buffer. Mono/legato handling then operates
    // on whatever notes are flowing through (live or arpeggiated).
    processArpeggiator (midi, buffer.getNumSamples());

    filterMidi (midi);

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
