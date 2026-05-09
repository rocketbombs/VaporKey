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
    // Shape enum. Indices 0..9 are FROZEN for preset compatibility with v0.4 -
    // the factory preset bank stores them as ints, and any user state save also
    // pickled the integer index. New shapes are appended past the 'Custom'
    // sentinel, which still sits at 9.
    //
    // 'Custom' selects a per-oscillator user-loaded wavetable owned by the
    // processor. The library itself does not store a Custom table - the slot
    // at index 9 in 'tables' is left default-constructed and never read (the
    // voice and the on-screen wavetable display both special-case Custom).
    enum Shape : int
    {
        // ---- v0.4 indices, frozen ----
        Basic    = 0,
        Saws     = 1,
        Squares  = 2,
        Vocal    = 3,
        Bell     = 4,
        Digital  = 5,
        Harmonic = 6,
        Glass    = 7,
        Reso     = 8,
        Custom   = 9,

        // ---- v0.5 additions (appended; indices stable from here on) ----
        Sync     = 10,    // hard-sync sweep
        RingMod  = 11,    // ring-modulated sine pair
        Wavefold = 12,    // symmetric wavefolder
        Vowels   = 13,    // A->E->I->O->U morph
        Choir    = 14,    // mixed-formant ensemble
        Whisper  = 15,    // high-formant breathy
        Organ    = 16,    // Hammond drawbars
        Pluck    = 17,    // exponentially-damped harmonics (Karplus/Strong-ish)
        Sawteeth = 18,    // odd harmonics with high-freq rolloff
        EvenOdd  = 19,    // even-vs-odd harmonic balance
        Tine     = 20,    // electric piano (Rhodes-ish inharmonic)
        Mallet   = 21,    // tuned percussion (vibraphone-ish)
        FMStack  = 22,    // 3-operator stacked FM
        Bitcrush = 23,    // sine quantised to fewer bits

        NumShapes
    };

    // Visual grouping for the shape selector. Keep in sync with
    // categoriesAndShapes() below.
    enum Category : int { Analog = 0, VocalCat, HarmonicCat, Inharmonic, Digital_, Special, NumCategories };

    struct CategoryEntry { Category category; juce::String name; juce::Array<int> shapes; };

    // Returns the curated category->shapes grouping used by the OSC page's
    // shape selector. Index order inside a category reflects the suggested
    // browse order, NOT the enum order.
    static const std::vector<CategoryEntry>& categoriesAndShapes();

    static WavetableLibrary& get();

    const Wavetable& getTable (int shape) const noexcept
    {
        const int s = juce::jlimit (0, (int) NumShapes - 1, shape);
        // Custom has no library entry - callers must special-case it. If we
        // somehow get here with shape == Custom, fall back to Basic so we
        // still produce audible output.
        return tables[(size_t) (s == (int) Custom ? (int) Basic : s)];
    }

    static const char* shapeName (int shape) noexcept;

private:
    WavetableLibrary();
    // Sized to NumShapes; the slot at index Custom (9) is intentionally
    // unused (default-constructed, silent).
    std::array<Wavetable, (size_t) NumShapes> tables;
};
