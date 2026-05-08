#include "Parameters.h"

#include <functional>
#include <type_traits>

namespace Parameters
{
namespace
{
    // A single declaration of every parameter the plugin owns. Each entry
    // carries enough information to (a) construct the JUCE parameter for the
    // APVTS layout and (b) bind the matching std::atomic<float>* into the
    // SynthParams cache. createLayout() and cache() both iterate this list,
    // so adding or renaming a parameter is a one-line change.
    struct Entry
    {
        juce::String id;
        std::function<std::unique_ptr<juce::RangedAudioParameter>()>     make;
        std::function<void (SynthParams&, std::atomic<float>*)>          bind;
    };

    // bindTo() accepts either a pointer-to-member (e.g. &SynthParams::fCut)
    // for plain fields, or a lambda accessor returning a reference to the
    // pointer field (for nested members like sp.osc[i].on or sp.macroVal[m]).
    template <typename Target>
    auto bindTo (Target t)
    {
        if constexpr (std::is_member_object_pointer_v<Target>)
            return [t] (SynthParams& sp, std::atomic<float>* p) { sp.*t = p; };
        else
            return [t] (SynthParams& sp, std::atomic<float>* p) { t (sp) = p; };
    }

    std::vector<Entry> buildRegistry()
    {
        std::vector<Entry> r;
        r.reserve (160);

        auto addF = [&r] (juce::String id, juce::String label,
                          juce::NormalisableRange<float> range, float def, auto target)
        {
            r.push_back ({ id,
                [id, label, range, def]
                {
                    return std::make_unique<juce::AudioParameterFloat> (
                        juce::ParameterID { id, 1 }, label, range, def);
                },
                bindTo (target) });
        };

        auto addI = [&r] (juce::String id, juce::String label,
                          int min, int max, int def, auto target)
        {
            r.push_back ({ id,
                [id, label, min, max, def]
                {
                    return std::make_unique<juce::AudioParameterInt> (
                        juce::ParameterID { id, 1 }, label, min, max, def);
                },
                bindTo (target) });
        };

        auto addB = [&r] (juce::String id, juce::String label, bool def, auto target)
        {
            r.push_back ({ id,
                [id, label, def]
                {
                    return std::make_unique<juce::AudioParameterBool> (
                        juce::ParameterID { id, 1 }, label, def);
                },
                bindTo (target) });
        };

        auto addC = [&r] (juce::String id, juce::String label,
                          juce::StringArray choices, int def, auto target)
        {
            r.push_back ({ id,
                [id, label, choices, def]
                {
                    return std::make_unique<juce::AudioParameterChoice> (
                        juce::ParameterID { id, 1 }, label, choices, def);
                },
                bindTo (target) });
        };

        // Oscillators
        juce::StringArray shapes;
        for (int i = 0; i < WavetableLibrary::NumShapes; ++i)
            shapes.add (WavetableLibrary::shapeName (i));

        for (int i = 0; i < 3; ++i)
        {
            const juce::String idp = "osc" + juce::String (i + 1) + "_";
            const juce::String lbl = "Osc " + juce::String (i + 1) + " ";

            addB (idp + "on",     lbl + "On",       i == 0,
                  [i] (SynthParams& sp) -> auto& { return sp.osc[i].on; });
            addC (idp + "shape",  lbl + "Shape",    shapes, 0,
                  [i] (SynthParams& sp) -> auto& { return sp.osc[i].shape; });
            addF (idp + "pos",    lbl + "Position", { 0.0f, 1.0f }, 0.0f,
                  [i] (SynthParams& sp) -> auto& { return sp.osc[i].position; });
            addF (idp + "level",  lbl + "Level",    { -60.0f, 6.0f }, -6.0f,
                  [i] (SynthParams& sp) -> auto& { return sp.osc[i].level; });
            addF (idp + "pan",    lbl + "Pan",      { -1.0f, 1.0f }, 0.0f,
                  [i] (SynthParams& sp) -> auto& { return sp.osc[i].pan; });
            addF (idp + "coarse", lbl + "Coarse",   { -24.0f, 24.0f, 1.0f }, 0.0f,
                  [i] (SynthParams& sp) -> auto& { return sp.osc[i].coarse; });
            addF (idp + "fine",   lbl + "Fine",     { -100.0f, 100.0f }, 0.0f,
                  [i] (SynthParams& sp) -> auto& { return sp.osc[i].fine; });
            addI (idp + "unison", lbl + "Unison",   1, 7, 1,
                  [i] (SynthParams& sp) -> auto& { return sp.osc[i].unison; });
            addF (idp + "detune", lbl + "Detune",   { 0.0f, 1.0f }, 0.2f,
                  [i] (SynthParams& sp) -> auto& { return sp.osc[i].detune; });
            addF (idp + "phase",  lbl + "Phase",    { -1.0f, 1.0f }, -1.0f,
                  [i] (SynthParams& sp) -> auto& { return sp.osc[i].phase; });
        }

        // Sub
        addB ("sub_on",    "Sub On",     false, &SynthParams::subOn);
        addC ("sub_shape", "Sub Shape",  SubShape::names(), 0, &SynthParams::subShape);
        addI ("sub_oct",   "Sub Octave", -2, -1, -1, &SynthParams::subOct);
        addF ("sub_level", "Sub Level",  { -60.0f, 0.0f }, -12.0f, &SynthParams::subLevel);

        // Noise
        addB ("noise_on",    "Noise On",    false, &SynthParams::noiseOn);
        addC ("noise_color", "Noise Color", NoiseColor::names(), 0, &SynthParams::noiseColor);
        addF ("noise_level", "Noise Level", { -60.0f, 0.0f }, -24.0f, &SynthParams::noiseLevel);

        // Filter
        addF ("f_cut",   "Cutoff",        { 20.0f, 18000.0f, 0.0f, 0.3f }, 12000.0f, &SynthParams::fCut);
        addF ("f_res",   "Resonance",     { 0.1f, 1.0f }, 0.3f,            &SynthParams::fRes);
        addF ("f_env",   "Env -> Cutoff", { -1.0f, 1.0f }, 0.0f,           &SynthParams::fEnv);
        addC ("f_type",  "Filter Type",   juce::StringArray { "LP", "BP", "HP" }, 0, &SynthParams::fType);
        addF ("f_drive", "Filter Drive",  { 0.0f, 1.0f }, 0.0f,            &SynthParams::fDrive);
        addF ("f_key",   "Key Track",     { 0.0f, 1.0f }, 0.0f,            &SynthParams::fKey);

        // Envelopes
        const juce::NormalisableRange<float> envRange { 0.001f, 8.0f, 0.0f, 0.4f };
        addF ("a_a",   "Amp A",          envRange, 0.005f,         &SynthParams::aA);
        addF ("a_d",   "Amp D",          envRange, 0.4f,           &SynthParams::aD);
        addF ("a_s",   "Amp S",          { 0.0f, 1.0f }, 0.7f,     &SynthParams::aS);
        addF ("a_r",   "Amp R",          envRange, 0.3f,           &SynthParams::aR);
        addF ("a_vel", "Amp Vel",        { 0.0f, 1.0f }, 0.5f,     &SynthParams::aVel);

        addF ("m_a",   "Mod A",          envRange, 0.005f,         &SynthParams::mA);
        addF ("m_d",   "Mod D",          envRange, 0.5f,           &SynthParams::mD);
        addF ("m_s",   "Mod S",          { 0.0f, 1.0f }, 0.0f,     &SynthParams::mS);
        addF ("m_r",   "Mod R",          envRange, 0.3f,           &SynthParams::mR);
        addF ("f_vel", "Filter Env Vel", { 0.0f, 1.0f }, 0.0f,     &SynthParams::fVel);

        addF ("p_env_amt",   "Pitch Env Amt",   { -24.0f, 24.0f }, 0.0f,            &SynthParams::pEnvAmt);
        addF ("p_env_decay", "Pitch Env Decay", { 0.005f, 2.0f, 0.0f, 0.4f }, 0.1f, &SynthParams::pEnvDecay);

        // LFOs
        const juce::NormalisableRange<float> rateRange { 0.05f, 30.0f, 0.0f, 0.4f };
        addC ("lfo1_shape", "LFO1 Shape",       LfoShape::names(), 0,    &SynthParams::lfo1Shape);
        addF ("lfo1_rate",  "LFO1 Rate",        rateRange, 4.0f,         &SynthParams::lfo1Rate);
        addF ("lfo1_amt",   "LFO1 -> Cutoff",   { -1.0f, 1.0f }, 0.0f,   &SynthParams::lfo1Amt);
        addB ("lfo1_sync",  "LFO1 Sync",        false,                   &SynthParams::lfo1Sync);
        addC ("lfo1_div",   "LFO1 Div",         syncDivNames(), 4,       &SynthParams::lfo1Div);
        addC ("lfo2_shape", "LFO2 Shape",       LfoShape::names(), 0,    &SynthParams::lfo2Shape);
        addF ("lfo2_rate",  "LFO2 Rate",        rateRange, 0.5f,         &SynthParams::lfo2Rate);
        addF ("lfo2_amt",   "LFO2 -> Position", { -1.0f, 1.0f }, 0.0f,   &SynthParams::lfo2Amt);
        addB ("lfo2_sync",  "LFO2 Sync",        false,                   &SynthParams::lfo2Sync);
        addC ("lfo2_div",   "LFO2 Div",         syncDivNames(), 4,       &SynthParams::lfo2Div);

        // Glide / mono
        addF ("glide",      "Glide",      { 0.0f, 2.0f, 0.0f, 0.4f }, 0.0f, &SynthParams::glide);
        addB ("mono",       "Mono",       false,    &SynthParams::mono);
        addB ("legato",     "Legato",     true,     &SynthParams::legato);
        addI ("bend_range", "Bend Range", 1, 24, 2, &SynthParams::bendRange);

        // Warmth
        addF ("grit",  "Grit",  { 0.0f, 1.0f }, 0.0f,  &SynthParams::grit);
        addF ("vibe",  "Vibe",  { 0.0f, 1.0f }, 0.0f,  &SynthParams::vibe);
        addF ("drift", "Drift", { 0.0f, 1.0f }, 0.15f, &SynthParams::drift);
        addF ("sat",   "Sat",   { 0.0f, 1.0f }, 0.1f,  &SynthParams::sat);

        // Distortion
        addF ("dist_drive", "Dist Drive", { 0.0f, 1.0f }, 0.0f, &SynthParams::distDrive);
        addF ("dist_mix",   "Dist Mix",   { 0.0f, 1.0f }, 0.0f, &SynthParams::distMix);
        addC ("dist_type",  "Dist Type",  DistType::names(), 0, &SynthParams::distType);

        // Chorus
        addF ("chorus",       "Chorus",       { 0.0f, 1.0f }, 0.0f,  &SynthParams::chorus);
        addF ("chorus_rate",  "Chorus Rate",  { 0.05f, 5.0f }, 0.6f, &SynthParams::chorusRate);
        addF ("chorus_depth", "Chorus Depth", { 0.0f, 1.0f }, 0.4f,  &SynthParams::chorusDepth);

        // Phaser
        addF ("phaser",       "Phaser",       { 0.0f, 1.0f }, 0.0f,    &SynthParams::phaser);
        addF ("phaser_rate",  "Phaser Rate",  { 0.05f, 8.0f }, 0.4f,   &SynthParams::phaserRate);
        addF ("phaser_depth", "Phaser Depth", { 0.0f, 1.0f }, 0.6f,    &SynthParams::phaserDepth);
        addF ("phaser_fb",    "Phaser FB",    { -0.95f, 0.95f }, 0.3f, &SynthParams::phaserFb);

        // EQ
        addF ("eq_low",      "EQ Low",      { -18.0f, 18.0f }, 0.0f,                    &SynthParams::eqLow);
        addF ("eq_mid",      "EQ Mid",      { -18.0f, 18.0f }, 0.0f,                    &SynthParams::eqMid);
        addF ("eq_mid_freq", "EQ Mid Freq", { 200.0f, 6000.0f, 0.0f, 0.3f }, 1000.0f,   &SynthParams::eqMidF);
        addF ("eq_high",     "EQ High",     { -18.0f, 18.0f }, 0.0f,                    &SynthParams::eqHigh);

        // Delay
        addF ("delay",      "Delay Mix",  { 0.0f, 1.0f }, 0.0f,  &SynthParams::delay);
        addF ("delay_time", "Delay Time", { 0.02f, 1.5f }, 0.4f, &SynthParams::delayTime);
        addF ("delay_fb",   "Delay FB",   { 0.0f, 0.95f }, 0.4f, &SynthParams::delayFb);
        addB ("delay_sync", "Delay Sync", false,                 &SynthParams::delaySync);
        addC ("delay_div",  "Delay Div",  syncDivNames(), 3,     &SynthParams::delayDiv);

        // Reverb
        addF ("reverb",      "Reverb",      { 0.0f, 1.0f }, 0.0f, &SynthParams::reverb);
        addF ("reverb_size", "Reverb Size", { 0.0f, 1.0f }, 0.6f, &SynthParams::reverbSize);
        addF ("reverb_damp", "Reverb Damp", { 0.0f, 1.0f }, 0.4f, &SynthParams::reverbDamp);

        // Compressor
        addB ("comp_on",     "Comp On",      false,                            &SynthParams::compOn);
        addF ("comp_thr",    "Comp Thresh",  { -60.0f, 0.0f }, -18.0f,         &SynthParams::compThr);
        addF ("comp_ratio",  "Comp Ratio",   { 1.0f, 20.0f }, 4.0f,            &SynthParams::compRatio);
        addF ("comp_atk",    "Comp Attack",  { 0.1f, 200.0f, 0.0f, 0.4f }, 10.0f,  &SynthParams::compAtk);
        addF ("comp_rel",    "Comp Release", { 5.0f, 1000.0f, 0.0f, 0.4f }, 100.0f, &SynthParams::compRel);
        addF ("comp_makeup", "Comp Makeup",  { -12.0f, 24.0f }, 0.0f,          &SynthParams::compMakeup);

        // Master
        addF ("gain",  "Master", { -60.0f, 6.0f }, -6.0f, &SynthParams::gain);
        addF ("width", "Width",  { 0.0f, 2.0f }, 1.0f,    &SynthParams::width);

        // Arpeggiator
        addB ("arp_on",      "Arp On",      false,                  &SynthParams::arpOn);
        addC ("arp_mode",    "Arp Mode",    ArpMode::names(), 0,    &SynthParams::arpMode);
        addC ("arp_div",     "Arp Rate",    syncDivNames(), 1,      &SynthParams::arpDiv);
        addI ("arp_octaves", "Arp Octaves", 1, 4, 1,                &SynthParams::arpOctaves);
        addF ("arp_gate",    "Arp Gate",    { 0.05f, 1.0f }, 0.5f,  &SynthParams::arpGate);
        addF ("arp_swing",   "Arp Swing",   { 0.0f, 0.5f }, 0.0f,   &SynthParams::arpSwing);
        addB ("arp_latch",   "Arp Latch",   false,                  &SynthParams::arpLatch);

        // Macros
        const juce::StringArray destNames = ModDest::names();
        for (int m = 0; m < SynthParams::kNumMacros; ++m)
        {
            const juce::String idp = "macro" + juce::String (m + 1) + "_";
            const juce::String lbl = "Macro " + juce::String (m + 1);

            addF (idp + "val",  lbl,           { 0.0f, 1.0f }, 0.0f,
                  [m] (SynthParams& sp) -> auto& { return sp.macroVal[m]; });
            addC (idp + "dest", lbl + " Dest", destNames, 0,
                  [m] (SynthParams& sp) -> auto& { return sp.macroDest[m]; });
            addF (idp + "amt",  lbl + " Amt",  { -1.0f, 1.0f }, 0.0f,
                  [m] (SynthParams& sp) -> auto& { return sp.macroAmt[m]; });
        }

        // MIDI mod sources: the value is whatever the controller is sending;
        // the user picks a destination and an amount.
        addC ("mw_dest", "Mod Wheel Dest",  destNames, 0,         &SynthParams::mwDest);
        addF ("mw_amt",  "Mod Wheel Amt",   { -1.0f, 1.0f }, 0.0f, &SynthParams::mwAmt);
        addC ("at_dest", "Aftertouch Dest", destNames, 0,         &SynthParams::atDest);
        addF ("at_amt",  "Aftertouch Amt",  { -1.0f, 1.0f }, 0.0f, &SynthParams::atAmt);

        return r;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    auto reg = buildRegistry();
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    v.reserve (reg.size());
    for (auto& e : reg)
        v.push_back (e.make());
    return { v.begin(), v.end() };
}

void cache (SynthParams& sp, juce::AudioProcessorValueTreeState& apvts)
{
    for (auto& e : buildRegistry())
    {
        auto* p = apvts.getRawParameterValue (e.id);
        jassert (p != nullptr);
        e.bind (sp, p);
    }
}

} // namespace Parameters
