#include "Wavetable.h"
#include "WavetableShapes.h"
#include <vector>

void Wavetable::buildMipsFromTimeDomain (Frame& out, const std::array<float, kFrameSize>& timeDomain)
{
    constexpr int N = kFrameSize;
    constexpr int order = 11; // 2^11 = 2048
    static_assert (1 << order == N, "kFrameSize must be 2^order");

    juce::dsp::FFT fft (order);

    // Real-only forward transform: buffer of size 2*N. Input in [0..N-1], rest zero.
    // Output: complex bins interleaved (real,imag) for k = 0..N-1, conjugate symmetric.
    std::vector<float> spectrum ((size_t) (N * 2), 0.0f);
    for (int n = 0; n < N; ++n) spectrum[(size_t) n] = timeDomain[(size_t) n];
    fft.performRealOnlyForwardTransform (spectrum.data());

    // For each mip, zero out bins above the harmonic cutoff and inverse transform.
    for (int m = 0; m < kNumMips; ++m)
    {
        const int maxHarm = juce::jmax (2, (N / 2) >> m);
        std::vector<float> work = spectrum;
        // Zero bins [maxHarm .. N/2] (and their conjugates handled by perform real-only inverse)
        for (int k = maxHarm; k <= N / 2; ++k)
        {
            work[(size_t) (2 * k)]     = 0.0f;
            work[(size_t) (2 * k + 1)] = 0.0f;
        }

        juce::dsp::FFT ifft (order);
        ifft.performRealOnlyInverseTransform (work.data());

        auto& wave = out.mips[(size_t) m];
        float peak = 0.0f;
        for (int n = 0; n < N; ++n)
        {
            wave[(size_t) n] = work[(size_t) n];
            peak = juce::jmax (peak, std::abs (wave[(size_t) n]));
        }
        // 1e-3 (~-60 dB) is below any audible content but well above 16-bit
        // quantisation residue. Without the higher floor a near-silent input
        // (e.g. a stereo wav whose channels cancel after the mono mix-down)
        // gets amplified to full scale.
        if (peak > 1.0e-3f)
            for (auto& v : wave) v *= 0.99f / peak;
    }
}

void Wavetable::buildFromMonoAudio (const float* samples, int numSamples)
{
    const int avail = juce::jmax (0, numSamples / kFrameSize);

    for (int f = 0; f < kNumFrames; ++f)
    {
        std::array<float, kFrameSize> base {};

        if (avail == 0)
        {
            const int n = juce::jmin (kFrameSize, numSamples);
            for (int i = 0; i < n; ++i) base[(size_t) i] = samples[i];
        }
        else
        {
            const int srcFrame = juce::jmin (f, avail - 1);
            const float* src = samples + srcFrame * kFrameSize;
            for (int i = 0; i < kFrameSize; ++i) base[(size_t) i] = src[i];
        }

        // Per-frame normalize to keep loud frames from dwarfing quiet ones.
        // Threshold matches buildMipsFromTimeDomain: anything quieter than
        // ~-60 dB is treated as silence and left untouched, so quantisation
        // residue from a phase-cancelling stereo wav does not get inflated
        // to full scale by the divide.
        float peak = 0.0f;
        for (float v : base) peak = juce::jmax (peak, std::abs (v));
        if (peak > 1.0e-3f)
            for (auto& v : base) v /= peak;

        buildMipsFromTimeDomain (frames[(size_t) f], base);
    }
}

WavetableLibrary& WavetableLibrary::get()
{
    static WavetableLibrary inst;
    return inst;
}

const char* WavetableLibrary::shapeName (int shape) noexcept
{
    switch (shape)
    {
        case Basic:    return "Basic";
        case Saws:     return "Saws";
        case Squares:  return "Squares";
        case Vocal:    return "Vocal";
        case Bell:     return "Bell";
        case Digital:  return "Digital";
        case Harmonic: return "Harmonic";
        case Glass:    return "Glass";
        case Reso:     return "Reso";
        case Custom:   return "Custom";
        case Sync:     return "Sync";
        case RingMod:  return "RingMod";
        case Wavefold: return "Wavefold";
        case Vowels:   return "Vowels";
        case Choir:    return "Choir";
        case Whisper:  return "Whisper";
        case Organ:    return "Organ";
        case Pluck:    return "Pluck";
        case Sawteeth: return "Sawteeth";
        case EvenOdd:  return "Even/Odd";
        case Tine:     return "Tine";
        case Mallet:   return "Mallet";
        case FMStack:  return "FM Stack";
        case Bitcrush: return "Bitcrush";
        default:       return "?";
    }
}

const std::vector<WavetableLibrary::CategoryEntry>& WavetableLibrary::categoriesAndShapes()
{
    // Curated visual grouping. The flat enum order doesn't reflect timbral
    // similarity (Custom sits at 9 between Reso and Sync for preset
    // compatibility, etc.), so the OSC page selector uses this layout to
    // surface related shapes near each other.
    static const std::vector<CategoryEntry> entries = {
        { Analog,       "Analog",     { Basic, Saws, Squares, Sync, RingMod, Wavefold } },
        { VocalCat,     "Vocal",      { Vocal, Vowels, Choir, Whisper } },
        { HarmonicCat,  "Harmonic",   { Harmonic, Organ, Pluck, Sawteeth, EvenOdd } },
        { Inharmonic,   "Inharmonic", { Bell, Tine, Mallet, Glass } },
        { Digital_,     "Digital",    { Digital, FMStack, Bitcrush } },
        { Special,      "Special",    { Reso, Custom } },
    };
    return entries;
}

WavetableLibrary::WavetableLibrary()
{
    using namespace WavetableShapes;

    tables[Basic]   .build (buildBasic);
    tables[Saws]    .build (buildSaws);
    tables[Squares] .build (buildSquares);
    tables[Vocal]   .build (buildVocal);
    tables[Bell]    .build (buildBell);
    tables[Digital] .build (buildDigital);
    tables[Harmonic].build (buildHarmonic);
    tables[Glass]   .build (buildGlass);
    tables[Reso]    .build (buildReso);

    // tables[Custom] is intentionally left default-constructed (zeroed). The
    // voice and display always special-case Custom to read from
    // SynthParams::customTables[i] instead.

    tables[Sync]    .build (buildSync);
    tables[RingMod] .build (buildRingMod);
    tables[Wavefold].build (buildWavefold);
    tables[Vowels]  .build (buildVowels);
    tables[Choir]   .build (buildChoir);
    tables[Whisper] .build (buildWhisper);
    tables[Organ]   .build (buildOrgan);
    tables[Pluck]   .build (buildPluck);
    tables[Sawteeth].build (buildSawteeth);
    tables[EvenOdd] .build (buildEvenOdd);
    tables[Tine]    .build (buildTine);
    tables[Mallet]  .build (buildMallet);
    tables[FMStack] .build (buildFMStack);
    tables[Bitcrush].build (buildBitcrush);
}
