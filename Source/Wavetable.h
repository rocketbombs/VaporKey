#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

// Multi-frame, mip-mapped (band-limited per octave) wavetable.
// Each table holds N frames (e.g. 8) you can morph between via 'position' [0..1].
// For each frame we generate one band-limited copy per octave to avoid aliasing.
class Wavetable
{
public:
    static constexpr int kFrameSize  = 2048;
    static constexpr int kNumFrames  = 8;
    static constexpr int kNumMips    = 10; // covers ~10 octaves

    struct Frame
    {
        // mips[0] = highest harmonic count (low pitch). mips[kNumMips-1] = sine-only (high pitch).
        std::array<std::array<float, kFrameSize>, kNumMips> mips {};
    };

    Wavetable() = default;

    // Build a wavetable from a generator that returns one full-band cycle for a given frame index in [0..kNumFrames-1].
    template <typename Gen>
    void build (Gen frameGen)
    {
        for (int f = 0; f < kNumFrames; ++f)
        {
            std::array<float, kFrameSize> base {};
            frameGen (f, base);
            // Run an FFT to get harmonics, then re-synthesize each mip with limited harmonics.
            buildMipsFromTimeDomain (frames[f], base);
        }
    }

    // Linear sample with morph between two adjacent frames + linear sample within frame.
    inline float sample (float position01, float phase01, int mip) const noexcept
    {
        const float fpos = juce::jlimit (0.0f, (float) (kNumFrames - 1), position01 * (kNumFrames - 1));
        const int   f0   = (int) fpos;
        const int   f1   = juce::jmin (kNumFrames - 1, f0 + 1);
        const float fx   = fpos - (float) f0;

        const auto& a = frames[f0].mips[mip];
        const auto& b = frames[f1].mips[mip];

        const float p = phase01 * (float) kFrameSize;
        const int   i0 = (int) p;
        const int   i1 = (i0 + 1) & (kFrameSize - 1);
        const float ix = p - (float) i0;

        const float sa = a[i0] + (a[i1] - a[i0]) * ix;
        const float sb = b[i0] + (b[i1] - b[i0]) * ix;
        return sa + (sb - sa) * fx;
    }

    static int chooseMip (float incrementPerSample) noexcept
    {
        // Higher increment -> fewer harmonics needed.
        // mip 0 keeps everything; each step halves harmonic count.
        const float ratio = juce::jmax (1.0e-6f, incrementPerSample);
        const int idx = (int) std::floor (std::log2 (ratio * (float) kFrameSize / 2.0f));
        return juce::jlimit (0, kNumMips - 1, idx);
    }

private:
    static void buildMipsFromTimeDomain (Frame& out, const std::array<float, kFrameSize>& timeDomain);

    std::array<Frame, kNumFrames> frames {};
};

// Library of factory wavetables built once at startup.
class WavetableLibrary
{
public:
    enum Shape { Basic = 0, Saws, Squares, Vocal, Bell, Digital, NumShapes };

    static WavetableLibrary& get();

    const Wavetable& getTable (int shape) const noexcept
    {
        return tables[(size_t) juce::jlimit (0, (int) NumShapes - 1, shape)];
    }

    static const char* shapeName (int shape) noexcept;

private:
    WavetableLibrary();
    std::array<Wavetable, NumShapes> tables;
};
