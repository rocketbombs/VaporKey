#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

// Multi-frame, mip-mapped (band-limited per octave) wavetable.
class Wavetable
{
public:
    static constexpr int kFrameSize  = 2048;
    static constexpr int kNumFrames  = 8;
    static constexpr int kNumMips    = 10;

    struct Frame
    {
        std::array<std::array<float, kFrameSize>, kNumMips> mips {};
    };

    Wavetable() = default;

    template <typename Gen>
    void build (Gen frameGen)
    {
        for (int f = 0; f < kNumFrames; ++f)
        {
            std::array<float, kFrameSize> base {};
            frameGen (f, base);
            buildMipsFromTimeDomain (frames[(size_t) f], base);
        }
    }

    // Build a wavetable from a mono audio buffer. Treats the buffer as a
    // sequence of fixed-size frames (Serum convention: kFrameSize samples per
    // frame). Up to kNumFrames frames are extracted from the start of the
    // buffer; the last available frame is replicated when fewer are present.
    // If the buffer is shorter than one frame the data is zero-padded into a
    // single frame and the rest are copies of it.
    void buildFromMonoAudio (const float* samples, int numSamples);

    inline float sample (float position01, float phase01, int mip) const noexcept
    {
        const float fpos = juce::jlimit (0.0f, (float) (kNumFrames - 1), position01 * (kNumFrames - 1));
        const int   f0   = (int) fpos;
        const int   f1   = juce::jmin (kNumFrames - 1, f0 + 1);
        const float fx   = fpos - (float) f0;

        const auto& a = frames[(size_t) f0].mips[(size_t) mip];
        const auto& b = frames[(size_t) f1].mips[(size_t) mip];

        const float p = phase01 * (float) kFrameSize;
        const int   i0 = (int) p;
        const int   i1 = (i0 + 1) & (kFrameSize - 1);
        const float ix = p - (float) i0;

        const float sa = a[(size_t) i0] + (a[(size_t) i1] - a[(size_t) i0]) * ix;
        const float sb = b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * ix;
        return sa + (sb - sa) * fx;
    }

    static int chooseMip (float incrementPerSample) noexcept
    {
        const float ratio = juce::jmax (1.0e-6f, std::abs (incrementPerSample));
        const int idx = (int) std::floor (std::log2 (ratio * (float) kFrameSize / 2.0f));
        return juce::jlimit (0, kNumMips - 1, idx);
    }

private:
    static void buildMipsFromTimeDomain (Frame& out, const std::array<float, kFrameSize>& timeDomain);
    std::array<Frame, kNumFrames> frames {};
};

class WavetableLibrary
{
public:
    // 'Custom' selects a per-oscillator user-loaded wavetable owned by the
    // processor. The library itself does not store a Custom table.
    enum Shape { Basic = 0, Saws, Squares, Vocal, Bell, Digital, Harmonic, Glass, Reso, Custom, NumShapes };

    static WavetableLibrary& get();

    const Wavetable& getTable (int shape) const noexcept
    {
        return tables[(size_t) juce::jlimit (0, (int) Custom - 1, shape)];
    }

    static const char* shapeName (int shape) noexcept;

private:
    WavetableLibrary();
    std::array<Wavetable, (size_t) Custom> tables;
};
