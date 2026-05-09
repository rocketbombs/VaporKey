#include "Wavetable.h"
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

// Each shape's per-frame generator lives as a free function below rather than
// as an inline lambda inside the constructor. Keeping them separated dodges
// an MSVC LTCG internal-compiler-error that triggers when the constructor
// holds too many lambda bodies in one translation unit
// (fatal error C1001 from p2\main.cpp during whole-program optimisation).
// CMakeLists.txt additionally disables /GL on this TU because the bug
// re-surfaced at link-time codegen even with the lambdas factored out.
namespace
{
    using Buf  = std::array<float, Wavetable::kFrameSize>;
    constexpr int kN = Wavetable::kFrameSize;
    constexpr int kFrames = Wavetable::kNumFrames;
    const float kTwoPi = juce::MathConstants<float>::twoPi;

    // Lerp helper for frame interpolation across discrete vowel positions.
    inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

    // Basic: sine -> triangle -> saw -> square as you sweep position.
    void buildBasic (int f, Buf& b)
    {
        const float pos = (float) f / (float) (kFrames - 1);
        for (int n = 0; n < kN; ++n)
        {
            const float t = (float) n / (float) kN;
            const float sine  = std::sin (kTwoPi * t);
            const float tri   = 4.0f * std::abs (t - 0.5f) - 1.0f;
            const float saw   = 2.0f * t - 1.0f;
            const float sq    = (t < 0.5f ? 1.0f : -1.0f);
            float v;
            if (pos < 0.333f)      { float x = pos / 0.333f;             v = sine * (1 - x) + tri * x; }
            else if (pos < 0.666f) { float x = (pos - 0.333f) / 0.333f;  v = tri  * (1 - x) + saw * x; }
            else                   { float x = (pos - 0.666f) / 0.334f;  v = saw  * (1 - x) + sq  * x; }
            b[(size_t) n] = v;
        }
    }

    // Saws: super-saw cluster widens with position.
    void buildSaws (int f, Buf& b)
    {
        const float spread = (float) f / (float) (kFrames - 1) * 0.04f;
        for (int n = 0; n < kN; ++n)
        {
            float v = 0.0f;
            for (int k = -3; k <= 3; ++k)
            {
                const float det = 1.0f + spread * (float) k;
                float t = std::fmod ((float) n / (float) kN * det, 1.0f);
                v += 2.0f * t - 1.0f;
            }
            b[(size_t) n] = v / 7.0f;
        }
    }

    // Squares: square -> PWM toward narrow pulse.
    void buildSquares (int f, Buf& b)
    {
        const float duty = 0.5f - 0.4f * ((float) f / (float) (kFrames - 1));
        for (int n = 0; n < kN; ++n)
        {
            const float t = (float) n / (float) kN;
            b[(size_t) n] = (t < duty) ? 1.0f : -1.0f;
        }
    }

    // Vocal: shifted-formant sweep (legacy timbre, kept for preset compat).
    void buildVocal (int f, Buf& b)
    {
        const float shift = 1.0f + (float) f * 0.25f;
        const float formants[3] = { 1.0f * shift, 2.6f * shift, 4.1f * shift };
        const float amps[3]     = { 1.0f, 0.6f, 0.35f };
        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            float v = 0.0f;
            for (int k = 1; k < 32; ++k)
            {
                float a = 1.0f / (float) k;
                float resp = 0.0f;
                for (int q = 0; q < 3; ++q)
                {
                    float d = (float) k - formants[q];
                    resp += amps[q] / (1.0f + d * d * 0.25f);
                }
                v += a * resp * std::sin (k * t);
            }
            b[(size_t) n] = v;
        }
    }

    // Bell: inharmonic partials, decaying with frame.
    void buildBell (int f, Buf& b)
    {
        const float decay = 1.0f - 0.6f * ((float) f / (float) (kFrames - 1));
        const float ratios[6] = { 1.0f, 2.76f, 5.4f, 8.93f, 13.34f, 18.64f };
        const float amps[6]   = { 1.0f, 0.7f, 0.55f, 0.4f, 0.3f, 0.2f };
        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            float v = 0.0f;
            for (int k = 0; k < 6; ++k)
                v += amps[k] * std::pow (decay, (float) k) * std::sin (ratios[k] * t);
            b[(size_t) n] = v;
        }
    }

    // Digital: single-modulator FM with growing index.
    void buildDigital (int f, Buf& b)
    {
        const float idx = 0.5f + 3.0f * ((float) f / (float) (kFrames - 1));
        const float ratio = 2.0f;
        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            b[(size_t) n] = std::sin (t + idx * std::sin (ratio * t));
        }
    }

    // Harmonic: tilt the harmonic series (-1 = dark, +1 = bright).
    void buildHarmonic (int f, Buf& b)
    {
        const float tilt = -1.0f + 2.0f * ((float) f / (float) (kFrames - 1));
        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            float v = 0.0f;
            for (int k = 1; k < 64; ++k)
                v += std::pow ((float) k, tilt) / (float) k * std::sin (k * t);
            b[(size_t) n] = v;
        }
    }

    // Glass: clustered partials.
    void buildGlass (int f, Buf& b)
    {
        const float spread = 1.0f + (float) f * 0.18f;
        const float partials[6] = { 1.0f, 1.5f * spread, 2.0f * spread, 3.5f, 5.5f, 8.0f };
        const float amps[6]     = { 1.0f, 0.5f, 0.7f, 0.3f, 0.25f, 0.15f };
        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            float v = 0.0f;
            for (int k = 0; k < 6; ++k)
                v += amps[k] * std::sin (partials[k] * t);
            b[(size_t) n] = v;
        }
    }

    // Reso: emphasized formant near varying frequency.
    void buildReso (int f, Buf& b)
    {
        const float center = 3.0f + 12.0f * ((float) f / (float) (kFrames - 1));
        const float q = 1.5f;
        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            float v = 0.0f;
            for (int k = 1; k < 64; ++k)
            {
                const float dx = ((float) k - center) / q;
                const float resp = 1.0f / (1.0f + dx * dx);
                v += (resp * 1.5f / (float) k) * std::sin (k * t);
            }
            b[(size_t) n] = v;
        }
    }

    // Sync: hard-sync sweep. A "slave" saw is reset every master cycle, but
    // its own internal frequency (the sync ratio) climbs with the position
    // parameter. Frame 0 = no sync (clean saw); frame 7 = ratio 4.5
    // (classic "pew" hard-sync timbre).
    void buildSync (int f, Buf& b)
    {
        const float ratio = 1.0f + 3.5f * ((float) f / (float) (kFrames - 1));
        for (int n = 0; n < kN; ++n)
        {
            const float master = (float) n / (float) kN;          // 0..1 over the cycle
            const float slave  = std::fmod (master * ratio, 1.0f);
            b[(size_t) n] = 2.0f * slave - 1.0f;
        }
    }

    // RingMod: product of two sines whose frequency ratio sweeps from 1 to
    // 7.5. Produces sum-and-difference partials around each harmonic - lots
    // of bell-like/metallic content without needing inharmonic ratios.
    void buildRingMod (int f, Buf& b)
    {
        const float ratio = 1.0f + 6.5f * ((float) f / (float) (kFrames - 1));
        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            b[(size_t) n] = std::sin (t) * std::sin (ratio * t);
        }
    }

    // Wavefold: sine through a triangle-style wavefolder of increasing drive.
    // Frame 0 = drive 1 (clean sine); frame 7 = drive ~5 (multiple folds,
    // very rich harmonic content). Uses a piecewise-linear fold rather than
    // the smoother sin-fold so the sound is bitey, not soft.
    void buildWavefold (int f, Buf& b)
    {
        const float drive = 1.0f + 4.0f * ((float) f / (float) (kFrames - 1));
        for (int n = 0; n < kN; ++n)
        {
            float s = std::sin (kTwoPi * (float) n / (float) kN) * drive;
            float v = std::fmod (s + 1.0f, 4.0f);
            if (v < 0) v += 4.0f;
            b[(size_t) n] = std::abs (v - 2.0f) - 1.0f;
        }
    }

    // Vowels: A -> E -> I -> O -> U morph using formant frequencies from
    // standard speech-research tables, normalised to a ~100 Hz fundamental
    // (i.e. formant 730 Hz becomes harmonic-index 7.3).
    void buildVowels (int f, Buf& b)
    {
        // Vowel formant tables (F1, F2, F3 at f0=100Hz, expressed as
        // harmonic indices). Source: classic Peterson & Barney 1952
        // vowel chart, lightly rounded.
        constexpr float vfmt[5][3] = {
            { 7.30f, 10.90f, 24.40f }, // A
            { 5.30f, 18.40f, 24.80f }, // E
            { 2.70f, 22.90f, 30.10f }, // I
            { 5.70f,  8.40f, 24.10f }, // O
            { 3.00f,  8.70f, 22.40f }, // U
        };
        constexpr float vamp[3] = { 1.0f, 0.55f, 0.30f };

        const float pos = (float) f / (float) (kFrames - 1) * 4.0f; // 0..4 across vowels
        const int   v0  = juce::jmin (3, (int) pos);
        const float vt  = pos - (float) v0;

        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            float v = 0.0f;
            for (int k = 1; k < 48; ++k)
            {
                float resp = 0.0f;
                for (int q = 0; q < 3; ++q)
                {
                    const float fq = lerp (vfmt[v0][q], vfmt[v0 + 1][q], vt);
                    const float dx = ((float) k - fq) / 1.6f;
                    resp += vamp[q] / (1.0f + dx * dx);
                }
                v += resp / (float) k * std::sin ((float) k * t);
            }
            b[(size_t) n] = v;
        }
    }

    // Choir: lush mixed-formant timbre that gets brighter across frames.
    // Average of 'A' and 'O' formants (so it sits halfway between open and
    // round), and the upper harmonic count grows with position.
    void buildChoir (int f, Buf& b)
    {
        constexpr float fmt[3] = { 6.5f, 9.7f, 24.2f };
        constexpr float amp[3] = { 1.0f, 0.55f, 0.30f };
        const float bright = 0.5f + 0.5f * ((float) f / (float) (kFrames - 1));
        const int   maxK   = juce::jmin (48, 18 + (int) (30.0f * bright));

        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            float v = 0.0f;
            for (int k = 1; k < maxK; ++k)
            {
                float resp = 0.0f;
                for (int q = 0; q < 3; ++q)
                {
                    const float dx = ((float) k - fmt[q]) / 1.8f;
                    resp += amp[q] / (1.0f + dx * dx);
                }
                v += resp / (float) k * std::sin ((float) k * t);
            }
            b[(size_t) n] = v;
        }
    }

    // Whisper: high-formant breathy timbre. Energy concentrated in the
    // 30-40th harmonic range. Lower harmonics are attenuated linearly so
    // the fundamental doesn't dominate.
    void buildWhisper (int f, Buf& b)
    {
        const float center = 28.0f + 12.0f * ((float) f / (float) (kFrames - 1));
        const float q = 5.0f;
        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            float v = 0.0f;
            for (int k = 1; k < 60; ++k)
            {
                const float dx = ((float) k - center) / q;
                float resp = 1.0f / (1.0f + dx * dx);
                if (k < 18) resp *= ((float) k / 18.0f); // damp low end
                v += resp / std::sqrt ((float) k) * std::sin ((float) k * t);
            }
            b[(size_t) n] = v;
        }
    }

    // Organ: Hammond-style drawbar additive. 9 drawbars at the canonical
    // ratios (sub-octave, sub-fifth, fundamental, octaves, harmonics).
    // Frame 0 = focused "8800 0000 0" (fundamental + 1st octave); frame 7
    // = full "8888 8888 8" (all drawbars open) for the classic full-organ
    // sound.
    void buildOrgan (int f, Buf& b)
    {
        constexpr float ratios[9]    = { 0.5f, 1.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f };
        constexpr float startAmps[9] = { 0.0f, 0.0f, 1.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        constexpr float endAmps[9]   = { 0.8f, 0.8f, 1.0f, 0.8f, 0.8f, 0.8f, 0.7f, 0.6f, 0.4f };
        const float blend = (float) f / (float) (kFrames - 1);
        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            float v = 0.0f;
            for (int k = 0; k < 9; ++k)
            {
                const float a = lerp (startAmps[k], endAmps[k], blend);
                v += a * std::sin (ratios[k] * t);
            }
            b[(size_t) n] = v;
        }
    }

    // Pluck: harmonic series with exponential decay, Karplus-Strong-ish.
    // Higher frame = less decay = brighter (more high-harmonic content).
    void buildPluck (int f, Buf& b)
    {
        const float decay = 0.30f - 0.27f * ((float) f / (float) (kFrames - 1));
        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            float v = 0.0f;
            for (int k = 1; k < 48; ++k)
                v += std::exp (-decay * (float) k) / (float) k * std::sin ((float) k * t);
            b[(size_t) n] = v;
        }
    }

    // Sawteeth: odd-harmonic saw with a high-frequency rolloff. Sounds
    // like a "warm" or "padded" saw - the missing even harmonics give it
    // a hollow square-ish core, and the rolloff softens the top end.
    // Higher frame = less rolloff (brighter).
    void buildSawteeth (int f, Buf& b)
    {
        const float rolloff = 0.06f - 0.055f * ((float) f / (float) (kFrames - 1));
        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            float v = 0.0f;
            for (int k = 1; k < 64; k += 2) // odd harmonics only
                v += std::exp (-rolloff * (float) k) / (float) k * std::sin ((float) k * t);
            b[(size_t) n] = v;
        }
    }

    // Even/Odd: morphs the balance between odd-only and even-only harmonics.
    // Frame 0 = pure odd harmonics (square-flavoured); frame 7 = pure even
    // (sounds an octave higher). The middle frames have both, like a saw.
    void buildEvenOdd (int f, Buf& b)
    {
        const float oddW  = 1.0f - (float) f / (float) (kFrames - 1);
        const float evenW =        (float) f / (float) (kFrames - 1);
        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            float v = 0.0f;
            for (int k = 1; k < 40; ++k)
            {
                const float w = (k % 2 == 1) ? oddW : evenW;
                v += w / (float) k * std::sin ((float) k * t);
            }
            b[(size_t) n] = v;
        }
    }

    // Tine: electric piano (Rhodes-like). Slightly inharmonic ratios with
    // a strong "clang" partial that decays away across frames. Frame 0 =
    // bright tine attack; frame 7 = mellow body without the clang.
    void buildTine (int f, Buf& b)
    {
        constexpr float ratios[5]   = { 1.0f, 2.005f, 3.07f, 6.0f, 12.0f };
        constexpr float ampsBright[5] = { 1.0f, 0.40f, 0.55f, 0.70f, 0.40f };
        constexpr float ampsMellow[5] = { 1.0f, 0.55f, 0.20f, 0.08f, 0.02f };
        const float blend = (float) f / (float) (kFrames - 1);
        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            float v = 0.0f;
            for (int k = 0; k < 5; ++k)
            {
                const float a = lerp (ampsBright[k], ampsMellow[k], blend);
                v += a * std::sin (ratios[k] * t);
            }
            b[(size_t) n] = v;
        }
    }

    // Mallet: tuned percussion (vibraphone/marimba). Square-plate vibration
    // mode ratios. The fundamental is constant; the upper modes grow with
    // the brightness frame parameter.
    void buildMallet (int f, Buf& b)
    {
        constexpr float ratios[4] = { 1.0f, 4.0f, 9.4f, 17.5f };
        const float bright = (float) f / (float) (kFrames - 1);
        const float amps[4] = { 1.0f, 0.45f * (0.4f + 0.6f * bright),
                                      0.25f * (0.3f + 0.7f * bright),
                                      0.12f * (0.2f + 0.8f * bright) };
        for (int n = 0; n < kN; ++n)
        {
            const float t = kTwoPi * (float) n / (float) kN;
            float v = 0.0f;
            for (int k = 0; k < 4; ++k)
                v += amps[k] * std::sin (ratios[k] * t);
            b[(size_t) n] = v;
        }
    }

    // FM Stack: 3-operator stacked FM. carrier is modulated by op1, which is
    // itself modulated by op2. Frame 0 = mild (low indices); frame 7 =
    // chaotic (high indices, very rich harmonic content).
    void buildFMStack (int f, Buf& b)
    {
        const float pos  = (float) f / (float) (kFrames - 1);
        const float idx1 = 1.0f + 4.0f * pos;
        const float idx2 = 0.8f + 2.5f * pos;
        const float r1   = 2.0f;
        const float r2   = 3.0f;
        for (int n = 0; n < kN; ++n)
        {
            const float t      = kTwoPi * (float) n / (float) kN;
            const float subMod = idx2 * std::sin (r2 * t);
            const float mod    = std::sin (r1 * t + subMod);
            b[(size_t) n] = std::sin (t + idx1 * mod);
        }
    }

    // Bitcrush: sine quantised to a sweepable bit depth. Frame 0 = ~12 bits
    // (essentially clean); frame 7 = ~3 bits (heavy stair-step quantisation
    // with a buzzy harmonic pattern).
    void buildBitcrush (int f, Buf& b)
    {
        const float pos = (float) f / (float) (kFrames - 1);
        const float bits = 12.0f - 9.0f * pos;
        const float steps = std::pow (2.0f, bits) * 0.5f;
        for (int n = 0; n < kN; ++n)
        {
            const float s = std::sin (kTwoPi * (float) n / (float) kN);
            b[(size_t) n] = std::round (s * steps) / steps;
        }
    }
}

WavetableLibrary::WavetableLibrary()
{
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
