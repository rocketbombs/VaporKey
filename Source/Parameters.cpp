#include "Parameters.h"

namespace {
    // Compact ID helpers shared between createLayout and cache.
    inline juce::String oscOn  (int i) { return "osc" + juce::String (i + 1) + "_on"; }
    inline juce::String oscSh  (int i) { return "osc" + juce::String (i + 1) + "_shape"; }
    inline juce::String oscPo  (int i) { return "osc" + juce::String (i + 1) + "_pos"; }
    inline juce::String oscLv  (int i) { return "osc" + juce::String (i + 1) + "_level"; }
    inline juce::String oscPa  (int i) { return "osc" + juce::String (i + 1) + "_pan"; }
    inline juce::String oscCo  (int i) { return "osc" + juce::String (i + 1) + "_coarse"; }
    inline juce::String oscFi  (int i) { return "osc" + juce::String (i + 1) + "_fine"; }
    inline juce::String oscUn  (int i) { return "osc" + juce::String (i + 1) + "_unison"; }
    inline juce::String oscDt  (int i) { return "osc" + juce::String (i + 1) + "_detune"; }
    inline juce::String oscPh  (int i) { return "osc" + juce::String (i + 1) + "_phase"; }
}

juce::AudioProcessorValueTreeState::ParameterLayout Parameters::createLayout()
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
        v.push_back (std::make_unique<PB> (juce::ParameterID { oscOn (i), 1 }, "Osc " + juce::String (i+1) + " On", i == 0));
        v.push_back (std::make_unique<PC> (juce::ParameterID { oscSh (i), 1 }, "Osc " + juce::String (i+1) + " Shape", shapes, 0));
        v.push_back (std::make_unique<P>  (juce::ParameterID { oscPo (i), 1 }, "Osc " + juce::String (i+1) + " Position", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
        v.push_back (std::make_unique<P>  (juce::ParameterID { oscLv (i), 1 }, "Osc " + juce::String (i+1) + " Level", juce::NormalisableRange<float> (-60.0f, 6.0f), -6.0f));
        v.push_back (std::make_unique<P>  (juce::ParameterID { oscPa (i), 1 }, "Osc " + juce::String (i+1) + " Pan", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
        v.push_back (std::make_unique<P>  (juce::ParameterID { oscCo (i), 1 }, "Osc " + juce::String (i+1) + " Coarse", juce::NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f));
        v.push_back (std::make_unique<P>  (juce::ParameterID { oscFi (i), 1 }, "Osc " + juce::String (i+1) + " Fine", juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f));
        v.push_back (std::make_unique<PI> (juce::ParameterID { oscUn (i), 1 }, "Osc " + juce::String (i+1) + " Unison", 1, 7, 1));
        v.push_back (std::make_unique<P>  (juce::ParameterID { oscDt (i), 1 }, "Osc " + juce::String (i+1) + " Detune", juce::NormalisableRange<float> (0.0f, 1.0f), 0.2f));
        v.push_back (std::make_unique<P>  (juce::ParameterID { oscPh (i), 1 }, "Osc " + juce::String (i+1) + " Phase", juce::NormalisableRange<float> (-1.0f, 1.0f), -1.0f));
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

    // MIDI mod sources: the value is whatever the controller is sending; the
    // user picks a destination and an amount.
    v.push_back (std::make_unique<PC> (juce::ParameterID { "mw_dest", 1 }, "Mod Wheel Dest", destNames, 0));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "mw_amt",  1 }, "Mod Wheel Amt",  juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<PC> (juce::ParameterID { "at_dest", 1 }, "Aftertouch Dest", destNames, 0));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "at_amt",  1 }, "Aftertouch Amt",  juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));

    return { v.begin(), v.end() };
}

void Parameters::cache (juce::AudioProcessorValueTreeState& apvts, SynthParams& synthParams)
{
    auto getF = [&] (const juce::String& id) {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    for (int i = 0; i < 3; ++i)
    {
        synthParams.osc[i].on       = getF (oscOn (i));
        synthParams.osc[i].shape    = getF (oscSh (i));
        synthParams.osc[i].position = getF (oscPo (i));
        synthParams.osc[i].level    = getF (oscLv (i));
        synthParams.osc[i].pan      = getF (oscPa (i));
        synthParams.osc[i].coarse   = getF (oscCo (i));
        synthParams.osc[i].fine     = getF (oscFi (i));
        synthParams.osc[i].unison   = getF (oscUn (i));
        synthParams.osc[i].detune   = getF (oscDt (i));
        synthParams.osc[i].phase    = getF (oscPh (i));
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

    synthParams.mwDest = getF ("mw_dest"); synthParams.mwAmt = getF ("mw_amt");
    synthParams.atDest = getF ("at_dest"); synthParams.atAmt = getF ("at_amt");

    synthParams.arpOn      = getF ("arp_on");
    synthParams.arpMode    = getF ("arp_mode");
    synthParams.arpDiv     = getF ("arp_div");
    synthParams.arpOctaves = getF ("arp_octaves");
    synthParams.arpGate    = getF ("arp_gate");
    synthParams.arpSwing   = getF ("arp_swing");
    synthParams.arpLatch   = getF ("arp_latch");
}
