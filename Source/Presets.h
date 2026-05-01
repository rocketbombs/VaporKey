#pragma once
#include <JuceHeader.h>

namespace VKPresets
{
    enum Category {
        Bass = 0,
        Lead,
        Pad,
        Pluck,
        Keys,
        Bell,
        FX,
        Arp,
        NumCategories
    };

    inline juce::StringArray categoryNames()
    {
        return { "Bass", "Lead", "Pad", "Pluck", "Keys", "Bell", "FX", "Arp" };
    }

    inline const char* categoryShortName (int c) noexcept
    {
        switch (c)
        {
            case Bass:  return "Bass";
            case Lead:  return "Lead";
            case Pad:   return "Pad";
            case Pluck: return "Pluck";
            case Keys:  return "Keys";
            case Bell:  return "Bell";
            case FX:    return "FX";
            case Arp:   return "Arp";
            default:    return "?";
        }
    }

    struct PV { const char* id; float v; };

    struct Preset {
        const char* name;
        int         category;
        std::vector<PV> values;
    };

    // Wavetable shape indices (mirror WavetableLibrary::Shape):
    // 0 Basic, 1 Saws, 2 Squares, 3 Vocal, 4 Bell, 5 Digital, 6 Harmonic, 7 Glass, 8 Reso

    inline const std::vector<Preset>& all()
    {
        static const std::vector<Preset> list = {
            // ============================================================
            // INIT (always first; treated as Keys for filtering)
            // ============================================================
            { "Init", Keys,
                {{"osc1_on",1},{"osc2_on",0},{"osc3_on",0},{"osc1_shape",0},{"osc1_pos",0},
                 {"f_cut",12000},{"f_res",0.3f},{"a_a",0.005f},{"a_d",0.4f},{"a_s",0.7f},{"a_r",0.3f},
                 {"grit",0},{"vibe",0},{"drift",0.1f},{"sat",0.05f},
                 {"chorus",0},{"delay",0},{"reverb",0},{"gain",-6}}},

            // ============================================================
            // BASS
            // ============================================================
            { "Neon Bass", Bass,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",1},{"osc1_pos",0.2f},{"osc1_unison",1},
                 {"osc2_shape",2},{"osc2_pos",0.0f},{"osc2_coarse",0},{"osc2_level",-6},
                 {"sub_on",1},{"sub_oct",-1},{"sub_level",-6},
                 {"f_cut",1800},{"f_res",0.55f},{"f_env",0.6f},{"f_drive",0.3f},
                 {"a_a",0.001f},{"a_d",0.25f},{"a_s",0.6f},{"a_r",0.15f},
                 {"m_a",0.001f},{"m_d",0.3f},{"m_s",0.0f},{"m_r",0.2f},
                 {"sat",0.35f},{"gain",-4}}},

            { "Sub Hammer", Bass,
                {{"osc1_on",1},{"osc2_on",0},{"osc3_on",0},
                 {"osc1_shape",2},{"osc1_pos",0.0f},
                 {"sub_on",1},{"sub_oct",-1},{"sub_level",-3},{"sub_shape",0},
                 {"f_cut",900},{"f_res",0.4f},{"f_env",0.5f},{"f_drive",0.5f},
                 {"a_a",0.001f},{"a_d",0.2f},{"a_s",0.8f},{"a_r",0.1f},
                 {"sat",0.5f},{"gain",-3}}},

            { "Reese Bass", Bass,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",1},{"osc1_pos",0.3f},{"osc1_unison",5},{"osc1_detune",0.15f},
                 {"osc2_shape",1},{"osc2_pos",0.3f},{"osc2_fine",12},{"osc2_unison",5},{"osc2_detune",0.18f},{"osc2_level",-2},
                 {"f_cut",1500},{"f_res",0.55f},{"f_drive",0.35f},
                 {"a_a",0.002f},{"a_d",0.3f},{"a_s",0.85f},{"a_r",0.4f},
                 {"chorus",0.25f},{"sat",0.4f},{"gain",-6}}},

            { "Acid 303", Bass,
                {{"osc1_on",1},{"osc2_on",0},{"osc3_on",0},
                 {"osc1_shape",1},{"osc1_pos",0.0f},
                 {"f_cut",800},{"f_res",0.85f},{"f_env",0.85f},{"f_drive",0.5f},
                 {"a_a",0.001f},{"a_d",0.18f},{"a_s",0.0f},{"a_r",0.15f},
                 {"m_a",0.001f},{"m_d",0.18f},{"m_s",0.0f},{"m_r",0.15f},
                 {"glide",0.08f},{"mono",1},
                 {"sat",0.3f},{"dist_drive",0.3f},{"dist_mix",0.3f},{"gain",-5}}},

            { "Glide Bass", Bass,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",1},{"osc1_pos",0.4f},{"osc1_unison",3},{"osc1_detune",0.2f},
                 {"osc2_shape",2},{"osc2_pos",0.0f},{"osc2_coarse",-12},{"osc2_level",-4},
                 {"f_cut",1400},{"f_res",0.5f},{"f_env",0.4f},
                 {"a_a",0.005f},{"a_d",0.4f},{"a_s",0.7f},{"a_r",0.3f},
                 {"glide",0.2f},{"mono",1},{"legato",1},
                 {"chorus",0.2f},{"sat",0.3f},{"gain",-5}}},

            { "FM Bass", Bass,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",5},{"osc1_pos",0.45f},
                 {"osc2_shape",0},{"osc2_pos",0.0f},{"osc2_coarse",-12},{"osc2_level",-4},
                 {"sub_on",1},{"sub_oct",-1},{"sub_level",-9},
                 {"f_cut",2200},{"f_res",0.4f},{"f_env",0.4f},
                 {"a_a",0.002f},{"a_d",0.3f},{"a_s",0.5f},{"a_r",0.2f},
                 {"sat",0.25f},{"gain",-6}}},

            { "Pulse Bass", Bass,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",2},{"osc1_pos",0.55f},
                 {"osc2_shape",2},{"osc2_pos",0.7f},{"osc2_fine",-7},{"osc2_level",-6},
                 {"sub_on",1},{"sub_oct",-1},{"sub_level",-8},
                 {"f_cut",1700},{"f_res",0.45f},{"f_env",0.5f},
                 {"a_a",0.001f},{"a_d",0.25f},{"a_s",0.7f},{"a_r",0.2f},
                 {"lfo2_rate",0.5f},{"lfo2_amt",0.25f},
                 {"sat",0.3f},{"gain",-5}}},

            { "Wobble", Bass,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",5},{"osc1_pos",0.5f},{"osc1_unison",3},{"osc1_detune",0.3f},
                 {"osc2_shape",1},{"osc2_pos",0.4f},{"osc2_coarse",-12},{"osc2_level",-6},
                 {"sub_on",1},{"sub_oct",-1},{"sub_level",-9},
                 {"f_cut",1200},{"f_res",0.7f},{"f_drive",0.4f},
                 {"lfo1_rate",4.0f},{"lfo1_shape",2},{"lfo1_amt",0.85f},
                 {"a_a",0.002f},{"a_d",0.3f},{"a_s",0.9f},{"a_r",0.3f},
                 {"sat",0.4f},{"gain",-6}}},

            // ============================================================
            // LEAD
            // ============================================================
            { "Vapor Lead", Lead,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",1},{"osc1_pos",0.4f},{"osc1_unison",5},{"osc1_detune",0.45f},
                 {"osc2_shape",1},{"osc2_pos",0.6f},{"osc2_coarse",-12},{"osc2_unison",3},{"osc2_detune",0.35f},{"osc2_level",-10},
                 {"f_cut",6500},{"f_res",0.4f},{"f_env",0.3f},
                 {"a_a",0.01f},{"a_d",0.3f},{"a_s",0.85f},{"a_r",0.6f},
                 {"m_a",0.005f},{"m_d",0.5f},{"m_s",0.0f},{"m_r",0.5f},
                 {"lfo2_rate",4.0f},{"lfo2_amt",0.25f},
                 {"grit",0.15f},{"drift",0.3f},{"sat",0.2f},
                 {"chorus",0.4f},{"delay",0.35f},{"delay_time",0.45f},{"delay_fb",0.45f},
                 {"reverb",0.4f},{"gain",-8}}},

            { "Aggro Lead", Lead,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",1},
                 {"osc1_shape",2},{"osc1_pos",0.3f},{"osc1_unison",7},{"osc1_detune",0.55f},
                 {"osc2_shape",2},{"osc2_pos",0.5f},{"osc2_coarse",-12},{"osc2_unison",3},{"osc2_detune",0.3f},
                 {"osc3_shape",1},{"osc3_pos",0.7f},{"osc3_coarse",7},{"osc3_unison",3},{"osc3_detune",0.4f},{"osc3_level",-9},
                 {"f_cut",8500},{"f_res",0.5f},{"f_env",0.3f},{"f_drive",0.4f},
                 {"a_a",0.002f},{"a_d",0.3f},{"a_s",0.8f},{"a_r",0.4f},
                 {"dist_drive",0.4f},{"dist_mix",0.6f},
                 {"phaser",0.3f},{"chorus",0.3f},{"delay",0.3f},{"reverb",0.45f},
                 {"sat",0.3f},{"gain",-10}}},

            { "Sawtooth Hero", Lead,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",1},{"osc1_pos",0.5f},{"osc1_unison",7},{"osc1_detune",0.5f},
                 {"osc2_shape",1},{"osc2_pos",0.5f},{"osc2_coarse",12},{"osc2_unison",3},{"osc2_detune",0.3f},{"osc2_level",-12},
                 {"f_cut",10000},{"f_res",0.3f},
                 {"a_a",0.005f},{"a_d",0.3f},{"a_s",0.9f},{"a_r",0.4f},
                 {"chorus",0.5f},{"delay",0.3f},{"reverb",0.5f},
                 {"sat",0.2f},{"gain",-9}}},

            { "Square Wave", Lead,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",2},{"osc1_pos",0.5f},{"osc1_unison",3},{"osc1_detune",0.2f},
                 {"osc2_shape",2},{"osc2_pos",0.7f},{"osc2_fine",7},{"osc2_level",-9},
                 {"f_cut",6000},{"f_res",0.4f},{"f_env",0.2f},
                 {"a_a",0.005f},{"a_d",0.4f},{"a_s",0.8f},{"a_r",0.3f},
                 {"lfo2_rate",5.5f},{"lfo2_amt",0.15f},
                 {"chorus",0.3f},{"delay",0.3f},{"reverb",0.35f},{"gain",-9}}},

            { "Vintage Mono", Lead,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",1},{"osc1_pos",0.4f},
                 {"osc2_shape",2},{"osc2_pos",0.5f},{"osc2_fine",-7},{"osc2_level",-6},
                 {"sub_on",1},{"sub_oct",-1},{"sub_level",-10},
                 {"f_cut",3500},{"f_res",0.55f},{"f_env",0.5f},{"f_drive",0.3f},
                 {"a_a",0.005f},{"a_d",0.5f},{"a_s",0.7f},{"a_r",0.4f},
                 {"glide",0.04f},{"mono",1},
                 {"vibe",0.2f},{"drift",0.4f},{"sat",0.35f},{"gain",-7}}},

            { "Resonant Lead", Lead,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",8},{"osc1_pos",0.6f},{"osc1_unison",3},{"osc1_detune",0.25f},
                 {"osc2_shape",1},{"osc2_pos",0.3f},{"osc2_coarse",-12},{"osc2_level",-9},
                 {"f_cut",5000},{"f_res",0.7f},{"f_env",0.4f},
                 {"a_a",0.005f},{"a_d",0.4f},{"a_s",0.8f},{"a_r",0.4f},
                 {"chorus",0.35f},{"phaser",0.25f},{"delay",0.3f},{"reverb",0.4f},
                 {"sat",0.25f},{"gain",-9}}},

            { "Octave Lead", Lead,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",1},
                 {"osc1_shape",1},{"osc1_pos",0.4f},
                 {"osc2_shape",1},{"osc2_pos",0.4f},{"osc2_coarse",12},{"osc2_level",-6},
                 {"osc3_shape",1},{"osc3_pos",0.4f},{"osc3_coarse",-12},{"osc3_level",-9},
                 {"f_cut",7000},{"f_res",0.35f},{"f_env",0.3f},
                 {"a_a",0.005f},{"a_d",0.4f},{"a_s",0.8f},{"a_r",0.5f},
                 {"chorus",0.4f},{"delay",0.35f},{"reverb",0.45f},{"gain",-10}}},

            { "Bright Lead", Lead,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",6},{"osc1_pos",0.85f},{"osc1_unison",3},{"osc1_detune",0.15f},
                 {"osc2_shape",1},{"osc2_pos",0.5f},{"osc2_fine",4},{"osc2_unison",3},{"osc2_detune",0.2f},{"osc2_level",-9},
                 {"f_cut",12000},{"f_res",0.25f},
                 {"a_a",0.003f},{"a_d",0.3f},{"a_s",0.85f},{"a_r",0.4f},
                 {"chorus",0.3f},{"delay",0.3f},{"reverb",0.45f},{"gain",-10}}},

            { "Pulse Lead", Lead,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",2},{"osc1_pos",0.6f},{"osc1_unison",5},{"osc1_detune",0.35f},
                 {"osc2_shape",2},{"osc2_pos",0.3f},{"osc2_coarse",-12},{"osc2_level",-9},
                 {"f_cut",6500},{"f_res",0.5f},{"f_env",0.35f},
                 {"a_a",0.005f},{"a_d",0.35f},{"a_s",0.8f},{"a_r",0.5f},
                 {"lfo2_rate",2.0f},{"lfo2_amt",0.25f},
                 {"chorus",0.4f},{"delay",0.35f},{"reverb",0.4f},{"gain",-9}}},

            // ============================================================
            // PAD
            // ============================================================
            { "Synthwave Pad", Pad,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",1},
                 {"osc1_shape",3},{"osc1_pos",0.5f},{"osc1_unison",5},{"osc1_detune",0.5f},
                 {"osc2_shape",6},{"osc2_pos",0.3f},{"osc2_coarse",7},{"osc2_unison",3},{"osc2_detune",0.4f},{"osc2_level",-9},
                 {"osc3_shape",3},{"osc3_pos",0.7f},{"osc3_coarse",-12},{"osc3_unison",3},{"osc3_detune",0.6f},{"osc3_level",-12},
                 {"f_cut",4500},{"f_res",0.25f},
                 {"a_a",0.8f},{"a_d",1.5f},{"a_s",0.85f},{"a_r",2.0f},
                 {"lfo1_rate",0.8f},{"lfo1_amt",0.3f},{"lfo2_rate",0.3f},{"lfo2_amt",0.3f},
                 {"grit",0.05f},{"vibe",0.1f},{"drift",0.4f},{"sat",0.15f},
                 {"chorus",0.6f},{"delay",0.3f},{"reverb",0.65f},{"gain",-10}}},

            { "Strings Pad", Pad,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",1},
                 {"osc1_shape",1},{"osc1_pos",0.5f},{"osc1_unison",7},{"osc1_detune",0.4f},
                 {"osc2_shape",1},{"osc2_pos",0.5f},{"osc2_fine",7},{"osc2_unison",5},{"osc2_detune",0.35f},{"osc2_level",-8},
                 {"osc3_shape",1},{"osc3_pos",0.5f},{"osc3_coarse",-12},{"osc3_unison",3},{"osc3_detune",0.5f},{"osc3_level",-10},
                 {"f_cut",5000},{"f_res",0.2f},
                 {"a_a",0.6f},{"a_d",1.0f},{"a_s",0.9f},{"a_r",1.5f},
                 {"vibe",0.1f},{"drift",0.4f},
                 {"chorus",0.55f},{"reverb",0.6f},{"delay",0.2f},{"gain",-11}}},

            { "Choir Pad", Pad,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",3},{"osc1_pos",0.3f},{"osc1_unison",7},{"osc1_detune",0.4f},
                 {"osc2_shape",3},{"osc2_pos",0.7f},{"osc2_fine",7},{"osc2_unison",5},{"osc2_detune",0.3f},{"osc2_level",-9},
                 {"f_cut",4200},{"f_res",0.2f},
                 {"a_a",1.2f},{"a_d",1.5f},{"a_s",0.9f},{"a_r",2.5f},
                 {"lfo1_rate",0.4f},{"lfo1_amt",0.2f},
                 {"vibe",0.2f},{"drift",0.5f},
                 {"chorus",0.5f},{"reverb",0.75f},{"gain",-12}}},

            { "Warm Pad", Pad,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",0},{"osc1_pos",0.6f},{"osc1_unison",3},{"osc1_detune",0.3f},
                 {"osc2_shape",6},{"osc2_pos",0.2f},{"osc2_coarse",-12},{"osc2_unison",3},{"osc2_detune",0.4f},{"osc2_level",-10},
                 {"f_cut",3500},{"f_res",0.2f},
                 {"a_a",1.5f},{"a_d",2.0f},{"a_s",0.85f},{"a_r",3.0f},
                 {"sat",0.2f},{"vibe",0.15f},{"drift",0.45f},
                 {"chorus",0.45f},{"reverb",0.7f},{"delay",0.2f},{"gain",-11}}},

            { "Glass Pad", Pad,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",1},
                 {"osc1_shape",7},{"osc1_pos",0.4f},{"osc1_unison",5},{"osc1_detune",0.3f},
                 {"osc2_shape",4},{"osc2_pos",0.3f},{"osc2_coarse",12},{"osc2_unison",3},{"osc2_detune",0.25f},{"osc2_level",-12},
                 {"osc3_shape",6},{"osc3_pos",0.6f},{"osc3_coarse",-12},{"osc3_level",-12},
                 {"f_cut",6500},{"f_res",0.2f},
                 {"a_a",0.9f},{"a_d",1.4f},{"a_s",0.85f},{"a_r",2.2f},
                 {"lfo2_rate",0.6f},{"lfo2_amt",0.25f},
                 {"chorus",0.5f},{"reverb",0.75f},{"delay",0.3f},{"delay_time",0.6f},{"gain",-12}}},

            { "Lush Pad", Pad,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",1},
                 {"osc1_shape",1},{"osc1_pos",0.5f},{"osc1_unison",7},{"osc1_detune",0.55f},
                 {"osc2_shape",3},{"osc2_pos",0.5f},{"osc2_coarse",7},{"osc2_unison",5},{"osc2_detune",0.45f},{"osc2_level",-8},
                 {"osc3_shape",6},{"osc3_pos",0.4f},{"osc3_coarse",12},{"osc3_unison",3},{"osc3_detune",0.4f},{"osc3_level",-12},
                 {"f_cut",4800},{"f_res",0.25f},
                 {"a_a",1.0f},{"a_d",2.0f},{"a_s",0.9f},{"a_r",2.5f},
                 {"lfo1_rate",0.5f},{"lfo1_amt",0.25f},
                 {"chorus",0.7f},{"phaser",0.2f},{"delay",0.35f},{"reverb",0.7f},{"gain",-12}}},

            { "Sweep Pad", Pad,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",1},{"osc1_pos",0.5f},{"osc1_unison",5},{"osc1_detune",0.4f},
                 {"osc2_shape",6},{"osc2_pos",0.3f},{"osc2_coarse",-12},{"osc2_unison",3},{"osc2_detune",0.5f},{"osc2_level",-10},
                 {"f_cut",1800},{"f_res",0.4f},{"f_env",0.7f},
                 {"a_a",0.8f},{"a_d",2.5f},{"a_s",0.6f},{"a_r",2.5f},
                 {"m_a",1.5f},{"m_d",3.0f},{"m_s",0.8f},{"m_r",2.0f},
                 {"chorus",0.55f},{"phaser",0.3f},{"reverb",0.7f},{"delay",0.3f},{"gain",-11}}},

            { "Soft Pad", Pad,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",0},{"osc1_pos",0.0f},{"osc1_unison",3},{"osc1_detune",0.2f},
                 {"osc2_shape",0},{"osc2_pos",0.3f},{"osc2_coarse",7},{"osc2_unison",3},{"osc2_detune",0.3f},{"osc2_level",-9},
                 {"f_cut",3500},{"f_res",0.2f},
                 {"a_a",1.5f},{"a_d",1.5f},{"a_s",0.9f},{"a_r",2.5f},
                 {"vibe",0.15f},{"drift",0.4f},
                 {"chorus",0.4f},{"reverb",0.7f},{"gain",-12}}},

            // ============================================================
            // PLUCK
            // ============================================================
            { "Pluck", Pluck,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",4},{"osc1_pos",0.5f},
                 {"osc2_shape",5},{"osc2_pos",0.3f},{"osc2_coarse",12},{"osc2_level",-8},
                 {"f_cut",8000},{"f_res",0.3f},{"f_env",0.7f},
                 {"a_a",0.001f},{"a_d",0.4f},{"a_s",0.0f},{"a_r",0.3f},
                 {"m_a",0.001f},{"m_d",0.2f},{"m_s",0.0f},{"m_r",0.2f},
                 {"p_env_amt",4},{"p_env_decay",0.05f},
                 {"chorus",0.2f},{"delay",0.25f},{"delay_time",0.3f},{"reverb",0.4f},{"gain",-6}}},

            { "Clean Pluck", Pluck,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",1},{"osc1_pos",0.5f},
                 {"osc2_shape",0},{"osc2_pos",0.3f},{"osc2_coarse",12},{"osc2_level",-9},
                 {"f_cut",10000},{"f_res",0.25f},{"f_env",0.5f},
                 {"a_a",0.001f},{"a_d",0.3f},{"a_s",0.0f},{"a_r",0.25f},
                 {"m_a",0.001f},{"m_d",0.2f},{"m_s",0.0f},{"m_r",0.2f},
                 {"chorus",0.25f},{"delay",0.3f},{"delay_time",0.375f},{"reverb",0.45f},{"gain",-7}}},

            { "FM Pluck", Pluck,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",5},{"osc1_pos",0.7f},
                 {"osc2_shape",5},{"osc2_pos",0.3f},{"osc2_coarse",12},{"osc2_level",-9},
                 {"f_cut",9000},{"f_res",0.3f},{"f_env",0.6f},
                 {"a_a",0.001f},{"a_d",0.35f},{"a_s",0.0f},{"a_r",0.3f},
                 {"m_a",0.001f},{"m_d",0.18f},{"m_s",0.0f},{"m_r",0.2f},
                 {"p_env_amt",6},{"p_env_decay",0.04f},
                 {"chorus",0.25f},{"delay",0.3f},{"reverb",0.45f},{"gain",-8}}},

            { "Wood Pluck", Pluck,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",4},{"osc1_pos",0.7f},
                 {"osc2_shape",6},{"osc2_pos",0.2f},{"osc2_coarse",12},{"osc2_level",-9},
                 {"f_cut",6000},{"f_res",0.4f},{"f_env",0.5f},
                 {"a_a",0.001f},{"a_d",0.5f},{"a_s",0.0f},{"a_r",0.4f},
                 {"sat",0.2f},
                 {"chorus",0.2f},{"delay",0.25f},{"reverb",0.4f},{"gain",-7}}},

            { "Bell Pluck", Pluck,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",4},{"osc1_pos",0.4f},
                 {"osc2_shape",7},{"osc2_pos",0.5f},{"osc2_coarse",12},{"osc2_level",-9},
                 {"f_cut",10000},{"f_res",0.25f},{"f_env",0.5f},
                 {"a_a",0.001f},{"a_d",0.7f},{"a_s",0.0f},{"a_r",0.5f},
                 {"chorus",0.3f},{"delay",0.3f},{"delay_time",0.5f},{"delay_fb",0.5f},{"reverb",0.55f},{"gain",-8}}},

            { "Soft Pluck", Pluck,
                {{"osc1_on",1},{"osc2_on",0},{"osc3_on",0},
                 {"osc1_shape",0},{"osc1_pos",0.2f},{"osc1_unison",3},{"osc1_detune",0.15f},
                 {"f_cut",6500},{"f_res",0.25f},{"f_env",0.4f},
                 {"a_a",0.002f},{"a_d",0.4f},{"a_s",0.0f},{"a_r",0.35f},
                 {"chorus",0.3f},{"delay",0.25f},{"reverb",0.5f},{"gain",-7}}},

            { "Sharp Pluck", Pluck,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",2},{"osc1_pos",0.3f},
                 {"osc2_shape",1},{"osc2_pos",0.5f},{"osc2_fine",7},{"osc2_level",-9},
                 {"f_cut",8500},{"f_res",0.5f},{"f_env",0.7f},
                 {"a_a",0.001f},{"a_d",0.25f},{"a_s",0.0f},{"a_r",0.2f},
                 {"m_a",0.001f},{"m_d",0.15f},{"m_s",0.0f},{"m_r",0.15f},
                 {"sat",0.2f},
                 {"delay",0.25f},{"reverb",0.4f},{"gain",-7}}},

            // ============================================================
            // KEYS
            // ============================================================
            { "Vapor Keys", Keys,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",0},{"osc1_pos",0.0f},
                 {"osc2_shape",6},{"osc2_pos",0.4f},{"osc2_coarse",12},{"osc2_level",-12},
                 {"f_cut",7000},{"f_res",0.2f},
                 {"a_a",0.005f},{"a_d",0.6f},{"a_s",0.8f},{"a_r",0.6f},
                 {"chorus",0.5f},{"delay",0.4f},{"delay_time",0.5f},{"delay_fb",0.5f},
                 {"reverb",0.55f},{"vibe",0.15f},{"drift",0.3f},{"gain",-8}}},

            { "Electric Keys", Keys,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",4},{"osc1_pos",0.6f},
                 {"osc2_shape",0},{"osc2_pos",0.5f},{"osc2_fine",4},{"osc2_level",-9},
                 {"f_cut",6500},{"f_res",0.2f},
                 {"a_a",0.005f},{"a_d",0.6f},{"a_s",0.6f},{"a_r",0.5f},
                 {"sat",0.2f},{"vibe",0.1f},
                 {"chorus",0.45f},{"phaser",0.2f},{"delay",0.3f},{"reverb",0.5f},{"gain",-9}}},

            { "Vintage Stack", Keys,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",1},
                 {"osc1_shape",1},{"osc1_pos",0.4f},{"osc1_unison",3},{"osc1_detune",0.2f},
                 {"osc2_shape",2},{"osc2_pos",0.5f},{"osc2_fine",-3},{"osc2_level",-7},
                 {"osc3_shape",0},{"osc3_pos",0.0f},{"osc3_coarse",-12},{"osc3_level",-9},
                 {"f_cut",5000},{"f_res",0.25f},
                 {"a_a",0.01f},{"a_d",0.7f},{"a_s",0.7f},{"a_r",0.5f},
                 {"vibe",0.2f},{"drift",0.45f},{"sat",0.25f},
                 {"chorus",0.4f},{"delay",0.25f},{"reverb",0.45f},{"gain",-9}}},

            { "Soft Rhodes", Keys,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",4},{"osc1_pos",0.3f},
                 {"osc2_shape",0},{"osc2_pos",0.0f},{"osc2_coarse",12},{"osc2_level",-12},
                 {"f_cut",4500},{"f_res",0.2f},
                 {"a_a",0.005f},{"a_d",0.8f},{"a_s",0.5f},{"a_r",0.5f},
                 {"a_vel",0.7f},
                 {"chorus",0.4f},{"reverb",0.5f},{"delay",0.2f},{"gain",-9}}},

            { "Bright Keys", Keys,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",6},{"osc1_pos",0.7f},
                 {"osc2_shape",0},{"osc2_pos",0.0f},{"osc2_coarse",12},{"osc2_level",-12},
                 {"f_cut",10000},{"f_res",0.25f},
                 {"a_a",0.005f},{"a_d",0.5f},{"a_s",0.7f},{"a_r",0.4f},
                 {"chorus",0.4f},{"delay",0.3f},{"delay_time",0.375f},{"reverb",0.5f},{"gain",-9}}},

            { "Pure Sine", Keys,
                {{"osc1_on",1},{"osc2_on",0},{"osc3_on",0},
                 {"osc1_shape",0},{"osc1_pos",0.0f},
                 {"sub_on",1},{"sub_oct",-1},{"sub_level",-15},{"sub_shape",0},
                 {"f_cut",12000},{"f_res",0.15f},
                 {"a_a",0.005f},{"a_d",0.4f},{"a_s",0.85f},{"a_r",0.5f},
                 {"chorus",0.25f},{"reverb",0.45f},{"gain",-7}}},

            { "Mellow Keys", Keys,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",0},{"osc1_pos",0.4f},
                 {"osc2_shape",6},{"osc2_pos",0.2f},{"osc2_coarse",-12},{"osc2_level",-10},
                 {"f_cut",4500},{"f_res",0.2f},
                 {"a_a",0.01f},{"a_d",0.7f},{"a_s",0.7f},{"a_r",0.6f},
                 {"vibe",0.15f},{"drift",0.35f},{"sat",0.15f},
                 {"chorus",0.45f},{"reverb",0.5f},{"delay",0.2f},{"gain",-9}}},

            // ============================================================
            // BELL
            // ============================================================
            { "Glass Bell", Bell,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",7},{"osc1_pos",0.4f},
                 {"osc2_shape",4},{"osc2_pos",0.5f},{"osc2_coarse",12},{"osc2_level",-10},
                 {"f_cut",10000},{"f_res",0.2f},
                 {"a_a",0.001f},{"a_d",1.5f},{"a_s",0.0f},{"a_r",1.0f},
                 {"chorus",0.3f},{"reverb",0.6f},{"delay",0.25f},{"delay_time",0.5f},{"gain",-9}}},

            { "Crystal Bell", Bell,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",1},
                 {"osc1_shape",4},{"osc1_pos",0.3f},
                 {"osc2_shape",4},{"osc2_pos",0.6f},{"osc2_coarse",12},{"osc2_level",-9},
                 {"osc3_shape",7},{"osc3_pos",0.5f},{"osc3_coarse",19},{"osc3_level",-12},
                 {"f_cut",12000},{"f_res",0.2f},
                 {"a_a",0.001f},{"a_d",2.5f},{"a_s",0.0f},{"a_r",1.5f},
                 {"chorus",0.4f},{"delay",0.35f},{"delay_time",0.5f},{"delay_fb",0.55f},
                 {"reverb",0.7f},{"gain",-10}}},

            { "Toy Bell", Bell,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",4},{"osc1_pos",0.8f},
                 {"osc2_shape",4},{"osc2_pos",0.3f},{"osc2_coarse",24},{"osc2_level",-12},
                 {"f_cut",9000},{"f_res",0.2f},
                 {"a_a",0.001f},{"a_d",0.8f},{"a_s",0.0f},{"a_r",0.6f},
                 {"chorus",0.3f},{"delay",0.3f},{"reverb",0.5f},{"gain",-8}}},

            { "Tine Bell", Bell,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",4},{"osc1_pos",0.5f},
                 {"osc2_shape",5},{"osc2_pos",0.5f},{"osc2_coarse",12},{"osc2_level",-10},
                 {"f_cut",8500},{"f_res",0.25f},
                 {"a_a",0.001f},{"a_d",1.2f},{"a_s",0.0f},{"a_r",0.8f},
                 {"p_env_amt",2},{"p_env_decay",0.03f},
                 {"chorus",0.35f},{"delay",0.3f},{"reverb",0.55f},{"gain",-9}}},

            { "Music Box", Bell,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",4},{"osc1_pos",0.6f},
                 {"osc2_shape",7},{"osc2_pos",0.4f},{"osc2_coarse",19},{"osc2_level",-9},
                 {"f_cut",11000},{"f_res",0.2f},
                 {"a_a",0.001f},{"a_d",1.0f},{"a_s",0.0f},{"a_r",0.7f},
                 {"chorus",0.4f},{"delay",0.3f},{"delay_time",0.375f},{"reverb",0.65f},{"gain",-9}}},

            // ============================================================
            // FX / DRONE
            // ============================================================
            { "Hover Drone", FX,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",1},
                 {"osc1_shape",6},{"osc1_pos",0.3f},{"osc1_unison",7},{"osc1_detune",0.7f},
                 {"osc2_shape",3},{"osc2_pos",0.7f},{"osc2_coarse",7},{"osc2_unison",5},{"osc2_detune",0.5f},{"osc2_level",-8},
                 {"osc3_shape",8},{"osc3_pos",0.5f},{"osc3_coarse",-12},{"osc3_unison",3},{"osc3_detune",0.6f},{"osc3_level",-10},
                 {"noise_on",1},{"noise_color",1},{"noise_level",-30},
                 {"f_cut",5000},{"f_res",0.3f},
                 {"a_a",2.0f},{"a_d",2.0f},{"a_s",1.0f},{"a_r",4.0f},
                 {"lfo1_rate",0.15f},{"lfo1_amt",0.4f},
                 {"chorus",0.7f},{"reverb",0.8f},{"delay",0.3f},
                 {"drift",0.6f},{"vibe",0.2f},{"gain",-12}}},

            { "Riser", FX,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",1},{"osc1_pos",0.5f},{"osc1_unison",7},{"osc1_detune",0.6f},
                 {"osc2_shape",6},{"osc2_pos",0.5f},{"osc2_unison",5},{"osc2_detune",0.5f},{"osc2_level",-8},
                 {"noise_on",1},{"noise_color",0},{"noise_level",-20},
                 {"f_cut",500},{"f_res",0.4f},{"f_env",1.0f},
                 {"a_a",0.5f},{"a_d",4.0f},{"a_s",0.9f},{"a_r",1.5f},
                 {"m_a",4.0f},{"m_d",4.0f},{"m_s",1.0f},{"m_r",1.0f},
                 {"phaser",0.5f},{"reverb",0.7f},{"delay",0.4f},{"gain",-12}}},

            { "Atmosphere", FX,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",1},
                 {"osc1_shape",3},{"osc1_pos",0.5f},{"osc1_unison",7},{"osc1_detune",0.7f},
                 {"osc2_shape",7},{"osc2_pos",0.5f},{"osc2_coarse",7},{"osc2_unison",5},{"osc2_detune",0.5f},{"osc2_level",-9},
                 {"osc3_shape",8},{"osc3_pos",0.7f},{"osc3_coarse",12},{"osc3_unison",3},{"osc3_detune",0.5f},{"osc3_level",-12},
                 {"f_cut",6000},{"f_res",0.3f},
                 {"a_a",2.5f},{"a_d",3.0f},{"a_s",0.85f},{"a_r",4.0f},
                 {"lfo1_rate",0.2f},{"lfo1_amt",0.5f},{"lfo2_rate",0.1f},{"lfo2_amt",0.4f},
                 {"chorus",0.7f},{"phaser",0.3f},{"reverb",0.85f},{"delay",0.4f},
                 {"drift",0.6f},{"vibe",0.2f},{"gain",-13}}},

            { "Texture", FX,
                {{"osc1_on",1},{"osc2_on",0},{"osc3_on",0},
                 {"osc1_shape",8},{"osc1_pos",0.7f},{"osc1_unison",7},{"osc1_detune",0.7f},
                 {"noise_on",1},{"noise_color",1},{"noise_level",-15},
                 {"f_cut",4000},{"f_res",0.5f},
                 {"a_a",1.0f},{"a_d",2.0f},{"a_s",0.8f},{"a_r",3.0f},
                 {"lfo1_rate",0.3f},{"lfo1_amt",0.6f},
                 {"phaser",0.5f},{"chorus",0.5f},{"reverb",0.75f},{"delay",0.4f},
                 {"grit",0.3f},{"drift",0.5f},{"gain",-13}}},

            { "Whoosh", FX,
                {{"osc1_on",0},{"osc2_on",0},{"osc3_on",0},
                 {"noise_on",1},{"noise_color",0},{"noise_level",-6},
                 {"f_cut",2000},{"f_res",0.6f},
                 {"a_a",0.8f},{"a_d",1.5f},{"a_s",0.0f},{"a_r",1.2f},
                 {"m_a",0.5f},{"m_d",1.5f},{"m_s",0.0f},{"m_r",1.0f},
                 {"phaser",0.7f},{"reverb",0.7f},{"delay",0.3f},{"gain",-10}}},

            { "Sci-Fi Sweep", FX,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",8},{"osc1_pos",0.0f},
                 {"osc2_shape",5},{"osc2_pos",0.5f},{"osc2_coarse",-12},{"osc2_level",-9},
                 {"f_cut",1000},{"f_res",0.7f},{"f_env",1.0f},
                 {"a_a",0.5f},{"a_d",3.0f},{"a_s",0.8f},{"a_r",1.5f},
                 {"m_a",2.0f},{"m_d",3.0f},{"m_s",1.0f},{"m_r",1.5f},
                 {"lfo1_rate",0.5f},{"lfo1_amt",0.6f},
                 {"phaser",0.5f},{"reverb",0.7f},{"delay",0.45f},{"gain",-11}}},

            // ============================================================
            // ARP (lean into the arpeggiator)
            // ============================================================
            { "Arp Stab", Arp,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",1},{"osc1_pos",0.3f},{"osc1_unison",3},{"osc1_detune",0.25f},
                 {"osc2_shape",2},{"osc2_pos",0.5f},{"osc2_coarse",12},{"osc2_level",-9},
                 {"f_cut",6000},{"f_res",0.45f},{"f_env",0.5f},
                 {"a_a",0.001f},{"a_d",0.2f},{"a_s",0.0f},{"a_r",0.15f},
                 {"m_a",0.001f},{"m_d",0.15f},{"m_s",0.0f},{"m_r",0.15f},
                 {"arp_on",1},{"arp_mode",0},{"arp_div",2},{"arp_octaves",2},{"arp_gate",0.5f},
                 {"chorus",0.3f},{"delay",0.4f},{"delay_sync",1},{"delay_div",2},{"delay_fb",0.5f},
                 {"reverb",0.5f},{"gain",-9}}},

            { "Arp Sequence", Arp,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",2},{"osc1_pos",0.4f},
                 {"osc2_shape",1},{"osc2_pos",0.4f},{"osc2_fine",7},{"osc2_level",-9},
                 {"f_cut",7000},{"f_res",0.4f},{"f_env",0.4f},
                 {"a_a",0.001f},{"a_d",0.15f},{"a_s",0.0f},{"a_r",0.1f},
                 {"m_a",0.001f},{"m_d",0.12f},{"m_s",0.0f},{"m_r",0.1f},
                 {"arp_on",1},{"arp_mode",2},{"arp_div",1},{"arp_octaves",2},{"arp_gate",0.4f},{"arp_swing",0.1f},
                 {"chorus",0.35f},{"delay",0.35f},{"delay_sync",1},{"delay_div",2},{"reverb",0.5f},{"gain",-9}}},

            { "Arp Pluck", Arp,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",4},{"osc1_pos",0.4f},
                 {"osc2_shape",7},{"osc2_pos",0.5f},{"osc2_coarse",12},{"osc2_level",-10},
                 {"f_cut",9000},{"f_res",0.3f},{"f_env",0.5f},
                 {"a_a",0.001f},{"a_d",0.3f},{"a_s",0.0f},{"a_r",0.25f},
                 {"arp_on",1},{"arp_mode",2},{"arp_div",1},{"arp_octaves",2},{"arp_gate",0.35f},
                 {"chorus",0.35f},{"delay",0.4f},{"delay_sync",1},{"delay_div",2},{"delay_fb",0.55f},
                 {"reverb",0.6f},{"gain",-10}}},

            { "Arp Bass", Arp,
                {{"osc1_on",1},{"osc2_on",0},{"osc3_on",0},
                 {"osc1_shape",1},{"osc1_pos",0.0f},
                 {"sub_on",1},{"sub_oct",-1},{"sub_level",-9},
                 {"f_cut",1500},{"f_res",0.5f},{"f_env",0.6f},
                 {"a_a",0.001f},{"a_d",0.18f},{"a_s",0.0f},{"a_r",0.12f},
                 {"m_a",0.001f},{"m_d",0.15f},{"m_s",0.0f},{"m_r",0.1f},
                 {"arp_on",1},{"arp_mode",0},{"arp_div",1},{"arp_octaves",1},{"arp_gate",0.45f},
                 {"sat",0.3f},{"gain",-5}}},

            { "Arp Pad", Arp,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",6},{"osc1_pos",0.5f},{"osc1_unison",3},{"osc1_detune",0.25f},
                 {"osc2_shape",1},{"osc2_pos",0.4f},{"osc2_coarse",12},{"osc2_unison",3},{"osc2_detune",0.3f},{"osc2_level",-10},
                 {"f_cut",5500},{"f_res",0.3f},{"f_env",0.3f},
                 {"a_a",0.05f},{"a_d",0.5f},{"a_s",0.4f},{"a_r",0.6f},
                 {"arp_on",1},{"arp_mode",2},{"arp_div",2},{"arp_octaves",3},{"arp_gate",0.7f},{"arp_latch",1},
                 {"chorus",0.5f},{"delay",0.4f},{"delay_sync",1},{"delay_div",3},{"reverb",0.65f},{"gain",-11}}},

            { "Arp Random", Arp,
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",5},{"osc1_pos",0.6f},
                 {"osc2_shape",7},{"osc2_pos",0.4f},{"osc2_coarse",12},{"osc2_level",-10},
                 {"f_cut",8000},{"f_res",0.35f},{"f_env",0.4f},
                 {"a_a",0.001f},{"a_d",0.2f},{"a_s",0.0f},{"a_r",0.2f},
                 {"arp_on",1},{"arp_mode",5},{"arp_div",1},{"arp_octaves",3},{"arp_gate",0.4f},
                 {"chorus",0.3f},{"delay",0.4f},{"delay_sync",1},{"delay_div",2},{"delay_fb",0.5f},
                 {"reverb",0.55f},{"gain",-10}}},
        };
        return list;
    }
}
