#pragma once
#include <JuceHeader.h>
#include <memory>
#include "Wavetable.h"

// =============================================================================
// Parameter manifest, modulation enums, and the runtime parameter snapshot.
//
// Everything in this header is data: the enums describing the choices a user
// can make, and the SynthParams struct that hands the audio thread cached
// pointers to the APVTS atomic floats. The two free functions in the
// Parameters namespace build the APVTS layout and populate SynthParams from a
// constructed APVTS.
// =============================================================================

// Modulation destinations addressable by macros and MIDI sources.
namespace ModDest {
    enum Dest {
        None = 0,
        Cutoff,
        Reso,
        Osc1Pos, Osc2Pos, Osc3Pos,
        Osc1Lvl, Osc2Lvl, Osc3Lvl,
        Osc1Det, Osc2Det, Osc3Det,
        Lfo1Rate, Lfo2Rate,
        DelayMix, ReverbMix, ChorusMix, PhaserMix,
        DistDrive, Width,
        NumDests
    };
    inline juce::StringArray names()
    {
        return { "Off", "Cutoff", "Resonance",
                 "Osc1 Pos", "Osc2 Pos", "Osc3 Pos",
                 "Osc1 Lvl", "Osc2 Lvl", "Osc3 Lvl",
                 "Osc1 Det", "Osc2 Det", "Osc3 Det",
                 "LFO1 Rate", "LFO2 Rate",
                 "Delay Mix", "Reverb Mix", "Chorus Mix", "Phaser Mix",
                 "Dist Drive", "Width" };
    }
}

namespace LfoShape { enum { Sine = 0, Tri, SawUp, SawDown, Square, SH, NumShapes };
    inline juce::StringArray names() { return { "Sine", "Tri", "Saw+", "Saw-", "Square", "S&H" }; }
}

namespace SubShape { enum { Sine = 0, Square, Tri, NumShapes };
    inline juce::StringArray names() { return { "Sine", "Square", "Tri" }; }
}

namespace NoiseColor { enum { White = 0, Pink, Brown, NumColors };
    inline juce::StringArray names() { return { "White", "Pink", "Brown" }; }
}

namespace DistType { enum { Soft = 0, Hard, Fold, Bit, NumTypes };
    inline juce::StringArray names() { return { "Soft", "Hard", "Fold", "Bit" }; }
}

namespace ArpMode {
    enum { Up = 0, Down, UpDown, DownUp, AsPlayed, Random, NumModes };
    inline juce::StringArray names() { return { "Up", "Down", "Up/Down", "Down/Up", "As Played", "Random" }; }
}

inline juce::StringArray syncDivNames()
{
    return { "1/32", "1/16", "1/8", "1/4D", "1/4", "1/2", "1/1", "2/1" };
}
inline double syncDivToBeats (int idx)
{
    static const double v[] = { 0.125, 0.25, 0.5, 0.75, 1.0, 2.0, 4.0, 8.0 };
    return v[juce::jlimit (0, 7, idx)];
}

// Live snapshot of every APVTS parameter the audio thread cares about. Pointers
// are cached once (in Parameters::cache) so the per-sample render path can
// dereference an atomic<float> directly instead of looking up by ID every block.
struct SynthParams
{
    struct OscP {
        std::atomic<float>* on{};
        std::atomic<float>* shape{};
        std::atomic<float>* position{};
        std::atomic<float>* level{};
        std::atomic<float>* pan{};
        std::atomic<float>* coarse{};
        std::atomic<float>* fine{};
        std::atomic<float>* unison{};
        std::atomic<float>* detune{};
        std::atomic<float>* phase{}; // start phase 0..1, -1 = free
    };
    OscP osc[3];

    // Sub osc
    std::atomic<float>* subOn{};
    std::atomic<float>* subShape{};
    std::atomic<float>* subOct{}; // -1 or -2
    std::atomic<float>* subLevel{};

    // Noise
    std::atomic<float>* noiseOn{};
    std::atomic<float>* noiseColor{};
    std::atomic<float>* noiseLevel{};

    // Filter
    std::atomic<float>* fCut{}; std::atomic<float>* fRes{};
    std::atomic<float>* fEnv{}; std::atomic<float>* fType{};
    std::atomic<float>* fDrive{}; std::atomic<float>* fKey{}; // key tracking 0..1

    // Envelopes
    std::atomic<float>* aA{}; std::atomic<float>* aD{}; std::atomic<float>* aS{}; std::atomic<float>* aR{};
    std::atomic<float>* mA{}; std::atomic<float>* mD{}; std::atomic<float>* mS{}; std::atomic<float>* mR{};
    std::atomic<float>* aVel{}; // amp velocity sense 0..1
    std::atomic<float>* fVel{}; // filter env velocity sense 0..1

    // Pitch envelope (decay only)
    std::atomic<float>* pEnvAmt{}; // semis -24..24
    std::atomic<float>* pEnvDecay{}; // sec

    // LFOs
    std::atomic<float>* lfo1Shape{}; std::atomic<float>* lfo1Rate{}; std::atomic<float>* lfo1Amt{};
    std::atomic<float>* lfo1Sync{};  std::atomic<float>* lfo1Div{};
    std::atomic<float>* lfo2Shape{}; std::atomic<float>* lfo2Rate{}; std::atomic<float>* lfo2Amt{};
    std::atomic<float>* lfo2Sync{};  std::atomic<float>* lfo2Div{};

    // Glide / mono
    std::atomic<float>* glide{};   // 0..2 sec
    std::atomic<float>* mono{};    // 0/1
    std::atomic<float>* legato{};  // 0/1
    std::atomic<float>* bendRange{}; // semis 1..24

    // Analog warmth
    std::atomic<float>* grit{}; std::atomic<float>* vibe{};
    std::atomic<float>* drift{}; std::atomic<float>* sat{};

    // FX
    std::atomic<float>* distDrive{}; std::atomic<float>* distMix{}; std::atomic<float>* distType{};
    std::atomic<float>* chorus{}; std::atomic<float>* chorusRate{}; std::atomic<float>* chorusDepth{};
    std::atomic<float>* phaser{}; std::atomic<float>* phaserRate{}; std::atomic<float>* phaserDepth{}; std::atomic<float>* phaserFb{};
    std::atomic<float>* eqLow{}; std::atomic<float>* eqMid{}; std::atomic<float>* eqMidF{}; std::atomic<float>* eqHigh{};
    std::atomic<float>* delay{}; std::atomic<float>* delayTime{}; std::atomic<float>* delayFb{};
    std::atomic<float>* delaySync{}; std::atomic<float>* delayDiv{};
    std::atomic<float>* reverb{}; std::atomic<float>* reverbSize{}; std::atomic<float>* reverbDamp{};
    std::atomic<float>* compThr{}; std::atomic<float>* compRatio{}; std::atomic<float>* compAtk{}; std::atomic<float>* compRel{}; std::atomic<float>* compMakeup{}; std::atomic<float>* compOn{};

    // Master
    std::atomic<float>* gain{}; std::atomic<float>* width{};

    // Macros
    static constexpr int kNumMacros = 4;
    std::atomic<float>* macroVal[kNumMacros] {};
    std::atomic<float>* macroDest[kNumMacros] {};
    std::atomic<float>* macroAmt[kNumMacros] {};

    // MIDI-driven modulation sources (the "value" comes from incoming MIDI).
    // Each carries a destination + amount, just like a macro.
    std::atomic<float>* mwDest{}; std::atomic<float>* mwAmt{};
    std::atomic<float>* atDest{}; std::atomic<float>* atAmt{};

    // Arpeggiator
    std::atomic<float>* arpOn{};
    std::atomic<float>* arpMode{};
    std::atomic<float>* arpDiv{};
    std::atomic<float>* arpOctaves{};
    std::atomic<float>* arpGate{};
    std::atomic<float>* arpSwing{};
    std::atomic<float>* arpLatch{};

    // Per-oscillator user wavetables. Updated atomically from the message thread
    // (drag-and-drop / file chooser); voices read with std::atomic_load.
    std::shared_ptr<Wavetable> customTables[3];

    // Live MIDI state from processor (per voice reads these atomics).
    std::atomic<float>  pitchBendSemis { 0.0f };
    std::atomic<float>  modWheel { 0.0f };
    std::atomic<float>  aftertouch { 0.0f };

    // Written once per block by the processor before voices render.
    std::atomic<double> bpm { 120.0 };

    // Computed per-block macro contributions to each destination (-1..+1 sum).
    float modSum[ModDest::NumDests] {};
};

// Audio-reactive UI state. The audio thread writes; the editor's timers read.
// Plain floats are fine for the scope ring buffer (occasional tearing is
// invisible at 60Hz repaint); the index uses a release-store so the editor
// always sees a consistent "most recent sample index".
struct VisData
{
    static constexpr int kScopeSize = 1024;     // power of two for cheap wrap
    static constexpr int kScopeMask = kScopeSize - 1;

    std::atomic<float> peakL { 0.0f };
    std::atomic<float> peakR { 0.0f };
    std::atomic<float> rms   { 0.0f };

    float                 scope[kScopeSize] {};
    std::atomic<uint32_t> scopeWrite { 0 };
};

namespace Parameters
{
    // Build the full APVTS layout. Called once during processor construction.
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Populate SynthParams::* atomic-pointer fields from a constructed APVTS.
    // Asserts on missing IDs in debug; safe to call once after the APVTS is
    // built and never again.
    void cache (juce::AudioProcessorValueTreeState& apvts, SynthParams& out);
}
