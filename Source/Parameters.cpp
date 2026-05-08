#include "Parameters.h"

namespace
{
    namespace IDs
    {
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

    // MIDI mod sources: the value is whatever the controller is sending; the
    // user picks a destination and an amount.
    v.push_back (std::make_unique<PC> (juce::ParameterID { "mw_dest", 1 }, "Mod Wheel Dest", destNames, 0));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "mw_amt",  1 }, "Mod Wheel Amt",  juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
    v.push_back (std::make_unique<PC> (juce::ParameterID { "at_dest", 1 }, "Aftertouch Dest", destNames, 0));
    v.push_back (std::make_unique<P>  (juce::ParameterID { "at_amt",  1 }, "Aftertouch Amt",  juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));

    return { v.begin(), v.end() };
}

void Parameters::cache (SynthParams& sp, juce::AudioProcessorValueTreeState& apvts)
{
    auto getF = [&apvts] (const juce::String& id) {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    for (int i = 0; i < 3; ++i)
    {
        sp.osc[i].on       = getF (IDs::on (i));
        sp.osc[i].shape    = getF (IDs::sh (i));
        sp.osc[i].position = getF (IDs::po (i));
        sp.osc[i].level    = getF (IDs::lv (i));
        sp.osc[i].pan      = getF (IDs::pa (i));
        sp.osc[i].coarse   = getF (IDs::co (i));
        sp.osc[i].fine     = getF (IDs::fi (i));
        sp.osc[i].unison   = getF (IDs::un (i));
        sp.osc[i].detune   = getF (IDs::dt (i));
        sp.osc[i].phase    = getF (IDs::ph (i));
    }

    sp.subOn = getF ("sub_on"); sp.subShape = getF ("sub_shape");
    sp.subOct = getF ("sub_oct"); sp.subLevel = getF ("sub_level");

    sp.noiseOn = getF ("noise_on"); sp.noiseColor = getF ("noise_color");
    sp.noiseLevel = getF ("noise_level");

    sp.fCut = getF ("f_cut"); sp.fRes = getF ("f_res");
    sp.fEnv = getF ("f_env"); sp.fType = getF ("f_type");
    sp.fDrive = getF ("f_drive"); sp.fKey = getF ("f_key");

    sp.aA = getF ("a_a"); sp.aD = getF ("a_d");
    sp.aS = getF ("a_s"); sp.aR = getF ("a_r");
    sp.aVel = getF ("a_vel");
    sp.mA = getF ("m_a"); sp.mD = getF ("m_d");
    sp.mS = getF ("m_s"); sp.mR = getF ("m_r");
    sp.fVel = getF ("f_vel");
    sp.pEnvAmt = getF ("p_env_amt"); sp.pEnvDecay = getF ("p_env_decay");

    sp.lfo1Shape = getF ("lfo1_shape"); sp.lfo1Rate = getF ("lfo1_rate");
    sp.lfo1Amt = getF ("lfo1_amt");
    sp.lfo1Sync = getF ("lfo1_sync"); sp.lfo1Div = getF ("lfo1_div");
    sp.lfo2Shape = getF ("lfo2_shape"); sp.lfo2Rate = getF ("lfo2_rate");
    sp.lfo2Amt = getF ("lfo2_amt");
    sp.lfo2Sync = getF ("lfo2_sync"); sp.lfo2Div = getF ("lfo2_div");

    sp.glide = getF ("glide");
    sp.mono = getF ("mono"); sp.legato = getF ("legato");
    sp.bendRange = getF ("bend_range");

    sp.grit = getF ("grit"); sp.vibe = getF ("vibe");
    sp.drift = getF ("drift"); sp.sat = getF ("sat");

    sp.distDrive = getF ("dist_drive"); sp.distMix = getF ("dist_mix");
    sp.distType  = getF ("dist_type");
    sp.chorus = getF ("chorus");
    sp.chorusRate = getF ("chorus_rate"); sp.chorusDepth = getF ("chorus_depth");
    sp.phaser = getF ("phaser"); sp.phaserRate = getF ("phaser_rate");
    sp.phaserDepth = getF ("phaser_depth"); sp.phaserFb = getF ("phaser_fb");
    sp.eqLow = getF ("eq_low"); sp.eqMid = getF ("eq_mid");
    sp.eqMidF = getF ("eq_mid_freq"); sp.eqHigh = getF ("eq_high");
    sp.delay = getF ("delay"); sp.delayTime = getF ("delay_time");
    sp.delayFb = getF ("delay_fb");
    sp.delaySync = getF ("delay_sync"); sp.delayDiv = getF ("delay_div");
    sp.reverb = getF ("reverb"); sp.reverbSize = getF ("reverb_size");
    sp.reverbDamp = getF ("reverb_damp");
    sp.compOn = getF ("comp_on"); sp.compThr = getF ("comp_thr");
    sp.compRatio = getF ("comp_ratio");
    sp.compAtk = getF ("comp_atk"); sp.compRel = getF ("comp_rel");
    sp.compMakeup = getF ("comp_makeup");

    sp.gain = getF ("gain"); sp.width = getF ("width");

    for (int m = 0; m < SynthParams::kNumMacros; ++m)
    {
        const juce::String prefix = "macro" + juce::String (m + 1) + "_";
        sp.macroVal[m]  = getF (prefix + "val");
        sp.macroDest[m] = getF (prefix + "dest");
        sp.macroAmt[m]  = getF (prefix + "amt");
    }

    sp.mwDest = getF ("mw_dest"); sp.mwAmt = getF ("mw_amt");
    sp.atDest = getF ("at_dest"); sp.atAmt = getF ("at_amt");

    sp.arpOn      = getF ("arp_on");
    sp.arpMode    = getF ("arp_mode");
    sp.arpDiv     = getF ("arp_div");
    sp.arpOctaves = getF ("arp_octaves");
    sp.arpGate    = getF ("arp_gate");
    sp.arpSwing   = getF ("arp_swing");
    sp.arpLatch   = getF ("arp_latch");
}
