#pragma once
#include <JuceHeader.h>

namespace VKPresets
{
    struct PV { const char* id; float v; };

    struct Preset {
        const char* name;
        std::vector<PV> values;
    };

    inline const std::vector<Preset>& all()
    {
        static const std::vector<Preset> list = {
            { "Init",
                {{"osc1_on",1},{"osc2_on",0},{"osc3_on",0},{"osc1_shape",0},{"osc1_pos",0},
                 {"f_cut",12000},{"f_res",0.3f},{"a_a",0.005f},{"a_d",0.4f},{"a_s",0.7f},{"a_r",0.3f},
                 {"grit",0},{"vibe",0},{"drift",0.1f},{"sat",0.05f},
                 {"chorus",0},{"delay",0},{"reverb",0},{"gain",-6}}},

            { "Vapor Lead",
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

            { "Synthwave Pad",
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",1},
                 {"osc1_shape",3},{"osc1_pos",0.5f},{"osc1_unison",5},{"osc1_detune",0.5f},
                 {"osc2_shape",6},{"osc2_pos",0.3f},{"osc2_coarse",7},{"osc2_unison",3},{"osc2_detune",0.4f},{"osc2_level",-9},
                 {"osc3_shape",3},{"osc3_pos",0.7f},{"osc3_coarse",-12},{"osc3_unison",3},{"osc3_detune",0.6f},{"osc3_level",-12},
                 {"f_cut",4500},{"f_res",0.25f},
                 {"a_a",0.8f},{"a_d",1.5f},{"a_s",0.85f},{"a_r",2.0f},
                 {"lfo1_rate",0.8f},{"lfo1_amt",0.3f},{"lfo2_rate",0.3f},{"lfo2_amt",0.3f},
                 {"grit",0.05f},{"vibe",0.1f},{"drift",0.4f},{"sat",0.15f},
                 {"chorus",0.6f},{"delay",0.3f},{"reverb",0.65f},{"gain",-10}}},

            { "Neon Bass",
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",1},{"osc1_pos",0.2f},{"osc1_unison",1},
                 {"osc2_shape",2},{"osc2_pos",0.0f},{"osc2_coarse",0},{"osc2_level",-6},
                 {"sub_on",1},{"sub_oct",-1},{"sub_level",-6},
                 {"f_cut",1800},{"f_res",0.55f},{"f_env",0.6f},{"f_drive",0.3f},
                 {"a_a",0.001f},{"a_d",0.25f},{"a_s",0.6f},{"a_r",0.15f},
                 {"m_a",0.001f},{"m_d",0.3f},{"m_s",0.0f},{"m_r",0.2f},
                 {"sat",0.35f},{"gain",-4}}},

            { "Pluck",
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",4},{"osc1_pos",0.5f},
                 {"osc2_shape",5},{"osc2_pos",0.3f},{"osc2_coarse",12},{"osc2_level",-8},
                 {"f_cut",8000},{"f_res",0.3f},{"f_env",0.7f},
                 {"a_a",0.001f},{"a_d",0.4f},{"a_s",0.0f},{"a_r",0.3f},
                 {"m_a",0.001f},{"m_d",0.2f},{"m_s",0.0f},{"m_r",0.2f},
                 {"p_env_amt",4},{"p_env_decay",0.05f},
                 {"chorus",0.2f},{"delay",0.25f},{"delay_time",0.3f},{"reverb",0.4f},{"gain",-6}}},

            { "Glass Bell",
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",7},{"osc1_pos",0.4f},
                 {"osc2_shape",4},{"osc2_pos",0.5f},{"osc2_coarse",12},{"osc2_level",-10},
                 {"f_cut",10000},{"f_res",0.2f},
                 {"a_a",0.001f},{"a_d",1.5f},{"a_s",0.0f},{"a_r",1.0f},
                 {"chorus",0.3f},{"reverb",0.6f},{"delay",0.25f},{"delay_time",0.5f},{"gain",-9}}},

            { "Aggro Lead",
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",1},
                 {"osc1_shape",2},{"osc1_pos",0.3f},{"osc1_unison",7},{"osc1_detune",0.55f},
                 {"osc2_shape",2},{"osc2_pos",0.5f},{"osc2_coarse",-12},{"osc2_unison",3},{"osc2_detune",0.3f},
                 {"osc3_shape",1},{"osc3_pos",0.7f},{"osc3_coarse",7},{"osc3_unison",3},{"osc3_detune",0.4f},{"osc3_level",-9},
                 {"f_cut",8500},{"f_res",0.5f},{"f_env",0.3f},{"f_drive",0.4f},
                 {"a_a",0.002f},{"a_d",0.3f},{"a_s",0.8f},{"a_r",0.4f},
                 {"dist_drive",0.4f},{"dist_mix",0.6f},
                 {"phaser",0.3f},{"chorus",0.3f},{"delay",0.3f},{"reverb",0.45f},
                 {"sat",0.3f},{"gain",-10}}},

            { "Wobble",
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",5},{"osc1_pos",0.5f},{"osc1_unison",3},{"osc1_detune",0.3f},
                 {"osc2_shape",1},{"osc2_pos",0.4f},{"osc2_coarse",-12},{"osc2_level",-6},
                 {"sub_on",1},{"sub_oct",-1},{"sub_level",-9},
                 {"f_cut",1200},{"f_res",0.7f},{"f_drive",0.4f},
                 {"lfo1_rate",4.0f},{"lfo1_shape",2},{"lfo1_amt",0.85f},
                 {"a_a",0.002f},{"a_d",0.3f},{"a_s",0.9f},{"a_r",0.3f},
                 {"sat",0.4f},{"gain",-6}}},

            { "Vapor Keys",
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",0},
                 {"osc1_shape",0},{"osc1_pos",0.0f},
                 {"osc2_shape",6},{"osc2_pos",0.4f},{"osc2_coarse",12},{"osc2_level",-12},
                 {"f_cut",7000},{"f_res",0.2f},
                 {"a_a",0.005f},{"a_d",0.6f},{"a_s",0.8f},{"a_r",0.6f},
                 {"chorus",0.5f},{"delay",0.4f},{"delay_time",0.5f},{"delay_fb",0.5f},
                 {"reverb",0.55f},{"vibe",0.15f},{"drift",0.3f},{"gain",-8}}},

            { "Hover Drone",
                {{"osc1_on",1},{"osc2_on",1},{"osc3_on",1},
                 {"osc1_shape",6},{"osc1_pos",0.3f},{"osc1_unison",7},{"osc1_detune",0.7f},
                 {"osc2_shape",3},{"osc2_pos",0.7f},{"osc2_coarse",7},{"osc2_unison",5},{"osc2_detune",0.5f},{"osc2_level",-8},
                 {"osc3_shape",8},{"osc3_pos",0.5f},{"osc3_coarse",-12},{"osc3_unison",3},{"osc3_detune",0.6f},{"osc3_level",-10},
                 {"noise_on",1},{"noise_color",1},{"noise_level",-30},
                 {"f_cut",5000},{"f_res",0.3f},
                 {"a_a",2.0f},{"a_d",2.0f},{"a_s",1.0f},{"a_r",4.0f},
                 {"lfo1_rate",0.15f},{"lfo1_amt",0.4f},
                 {"chorus",0.7f},{"reverb",0.8f},{"delay",0.3f},
                 {"drift",0.6f},{"vibe",0.2f},{"gain",-12}}}
        };
        return list;
    }
}
