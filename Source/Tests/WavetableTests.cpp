// Wavetable tests - all factory wavetables built by WavetableLibrary
// and the buildFromMonoAudio path used by the .wav importer. The mip-mapping
// is the most subtle bit: voices read mips at run time, the wrong mip
// produces audible aliasing or overly dull output.
//
// Coverage:
//   * factory tables produce non-zero, bounded output across position / phase
//   * mip selection (chooseMip) is monotonic and clamped at the limits
//   * higher-mip frames have less high-frequency content than mip 0
//   * buildFromMonoAudio handles the boundary cases (empty, short, exact-fit)
//   * frames are independent (frame N doesn't bleed into frame M after build)
//
// These are deterministic - no audio thread, no timing.
#include "TestRunner.h"

#include "../Wavetable.h"
#include <cmath>

using namespace VKTest;

namespace
{
    // RMS over a frame. The factory tables are normalised so a healthy frame
    // sits at ~0.4 RMS for a sine, much higher for a saw.
    float rmsOfFrame (const Wavetable& wt, float position, int mip)
    {
        constexpr int N = 1024;
        double sumSq = 0.0;
        for (int i = 0; i < N; ++i)
        {
            const float ph = (float) i / (float) N;
            const float v  = wt.sample (position, ph, mip);
            sumSq += (double) v * (double) v;
        }
        return (float) std::sqrt (sumSq / (double) N);
    }

    // Crude high-frequency energy: alternate-sample difference RMS. A pure
    // sine at low frequency has near-zero alt-diff; a saw or square has a
    // lot. Higher mips zero high partials, so this should drop monotonically
    // (or stay flat once below the harmonic count).
    float altDiffRms (const Wavetable& wt, float position, int mip)
    {
        constexpr int N = 1024;
        double sumSq = 0.0;
        float prev = wt.sample (position, 0.0f, mip);
        for (int i = 1; i < N; ++i)
        {
            const float ph = (float) i / (float) N;
            const float v  = wt.sample (position, ph, mip);
            const float d  = v - prev;
            sumSq += (double) d * (double) d;
            prev = v;
        }
        return (float) std::sqrt (sumSq / (double) (N - 1));
    }
}

VK_TEST (Wavetable_LibrarySingletonStable)
{
    auto& a = WavetableLibrary::get();
    auto& b = WavetableLibrary::get();
    VK_EXPECT (&a == &b);

    // Every factory shape should resolve to a real table. Custom is special-
    // cased everywhere (per-oscillator user table), so skip it here.
    for (int s = 0; s < WavetableLibrary::NumShapes; ++s)
    {
        if (s == WavetableLibrary::Custom) continue;
        const Wavetable& wt = a.getTable (s);
        const float v = wt.sample (0.0f, 0.25f, 0);
        VK_EXPECT (! std::isnan (v));
        VK_EXPECT (! std::isinf (v));
    }
}

VK_TEST (Wavetable_FactoryShapesProduceAudibleOutput)
{
    auto& lib = WavetableLibrary::get();
    for (int s = 0; s < WavetableLibrary::NumShapes; ++s)
    {
        if (s == WavetableLibrary::Custom) continue;
        const Wavetable& wt = lib.getTable (s);
        const float r0 = rmsOfFrame (wt, 0.0f, 0);
        const float r1 = rmsOfFrame (wt, 1.0f, 0);
        VK_EXPECT_MSG (r0 > 0.05f,
            juce::String ("shape ") + WavetableLibrary::shapeName (s)
                + " frame 0 is silent (rms=" + juce::String (r0, 4) + ")");
        VK_EXPECT_MSG (r1 > 0.05f,
            juce::String ("shape ") + WavetableLibrary::shapeName (s)
                + " frame last is silent (rms=" + juce::String (r1, 4) + ")");
    }
}

VK_TEST (Wavetable_FactoryShapesAreBounded)
{
    auto& lib = WavetableLibrary::get();
    constexpr int N = 256;
    for (int s = 0; s < WavetableLibrary::NumShapes; ++s)
    {
        if (s == WavetableLibrary::Custom) continue;
        const Wavetable& wt = lib.getTable (s);
        for (int frame = 0; frame < Wavetable::kNumFrames; ++frame)
        {
            const float pos = (float) frame / (float) (Wavetable::kNumFrames - 1);
            for (int i = 0; i < N; ++i)
            {
                const float ph = (float) i / (float) N;
                const float v  = wt.sample (pos, ph, 0);
                // build() normalises peaks to 0.99; allow a small slop for
                // bilinear interpolation between neighbouring frames.
                VK_EXPECT_MSG (std::abs (v) <= 1.05f,
                    juce::String ("shape ") + WavetableLibrary::shapeName (s)
                        + " out of bounds at pos=" + juce::String (pos, 3)
                        + " ph=" + juce::String (ph, 3)
                        + " v=" + juce::String (v, 4));
                if (! (std::abs (v) <= 1.05f)) break;
            }
        }
    }
}

VK_TEST (Wavetable_AllShapesHaveDistinctNames)
{
    juce::StringArray names;
    for (int s = 0; s < WavetableLibrary::NumShapes; ++s)
    {
        const juce::String name = WavetableLibrary::shapeName (s);
        VK_EXPECT_MSG (name.isNotEmpty(),
            juce::String ("shape ") + juce::String (s) + " has empty name");
        VK_EXPECT_MSG (name != "?",
            juce::String ("shape ") + juce::String (s) + " has placeholder name");
        VK_EXPECT_MSG (! names.contains (name),
            juce::String ("duplicate shape name: ") + name);
        names.add (name);
    }
}

VK_TEST (Wavetable_CategoriesCoverEveryShape)
{
    // Every non-? shape (including Custom) should appear in exactly one
    // category in categoriesAndShapes(). Otherwise the OSC page selector
    // can either miss shapes (silently unreachable) or list one twice.
    const auto& cats = WavetableLibrary::categoriesAndShapes();
    juce::Array<int> seen;
    for (const auto& cat : cats)
        for (int s : cat.shapes)
        {
            VK_EXPECT_MSG (! seen.contains (s),
                juce::String ("shape ") + WavetableLibrary::shapeName (s)
                    + " appears in more than one category");
            seen.add (s);
        }

    for (int s = 0; s < WavetableLibrary::NumShapes; ++s)
    {
        VK_EXPECT_MSG (seen.contains (s),
            juce::String ("shape ") + WavetableLibrary::shapeName (s)
                + " (index " + juce::String (s) + ") missing from categoriesAndShapes()");
    }
}

VK_TEST (Wavetable_HighMipsHaveLessHighFrequency)
{
    // Squares is the cleanest test of the mip-mapping path: a true square
    // wave has equal-amplitude harmonics across the spectrum and a
    // sample-rate-aligned discontinuity at the frame boundary, so dropping
    // bins above N/2^m monotonically reduces our alt-diff RMS estimator.
    // (The Saws factory shape uses 7 detuned saw cycles per frame, which
    // spreads spectral energy and lets per-mip peak normalisation amplify
    // mid-mip residuals back over mip 0 - it tests the synthesis recipe more
    // than the band-limiting itself, so it isn't a clean fixture for this
    // assertion.)
    auto& lib = WavetableLibrary::get();
    const Wavetable& sq = lib.getTable (WavetableLibrary::Squares);

    const float hf0 = altDiffRms (sq, 0.0f, 0);
    const float hf3 = altDiffRms (sq, 0.0f, 3);
    const float hf6 = altDiffRms (sq, 0.0f, 6);

    VK_EXPECT_MSG (hf3 <= hf0 + 1.0e-6f,
        juce::String ("mip3 should have <= high-freq energy of mip0; got ")
            + juce::String (hf3, 4) + " vs " + juce::String (hf0, 4));
    VK_EXPECT_MSG (hf6 <= hf3 + 1.0e-6f,
        juce::String ("mip6 should have <= high-freq energy of mip3; got ")
            + juce::String (hf6, 4) + " vs " + juce::String (hf3, 4));
}

VK_TEST (Wavetable_ChooseMipIsMonotonic)
{
    // For increasing playback rates, chooseMip() must never return a
    // *smaller* mip index than for a slower rate. (Slower = more harmonics
    // safe, lower mip; faster = drop harmonics, higher mip.)
    int prev = -1;
    for (float r = 1.0f / 4096.0f; r < 1.0f; r *= 1.25f)
    {
        const int mip = Wavetable::chooseMip (r);
        VK_EXPECT_MSG (mip >= prev,
            juce::String ("chooseMip not monotonic at r=") + juce::String (r, 6)
                + ": prev=" + juce::String (prev) + " mip=" + juce::String (mip));
        prev = mip;
    }
}

VK_TEST (Wavetable_ChooseMipClampsToBounds)
{
    VK_EXPECT_EQ (Wavetable::chooseMip (0.0f), 0);
    VK_EXPECT_EQ (Wavetable::chooseMip (-1.0f), Wavetable::chooseMip (1.0f));
    VK_EXPECT (Wavetable::chooseMip (0.000001f) == 0);
    const int top = Wavetable::chooseMip (1.0e6f);
    VK_EXPECT_EQ (top, Wavetable::kNumMips - 1);
}

VK_TEST (Wavetable_BuildFromMonoAudioEmpty)
{
    // Zero-length input should not crash and should produce a deterministic
    // (silent) table. The table must still answer queries without NaN/Inf.
    Wavetable wt;
    const float* none = nullptr;
    wt.buildFromMonoAudio (none, 0);

    for (int p = 0; p < 5; ++p)
    {
        const float pos = (float) p / 4.0f;
        for (int i = 0; i < 16; ++i)
        {
            const float ph = (float) i / 16.0f;
            const float v  = wt.sample (pos, ph, 0);
            VK_EXPECT (! std::isnan (v));
            VK_EXPECT (! std::isinf (v));
        }
    }
}

VK_TEST (Wavetable_BuildFromMonoAudioShortInput)
{
    // Less than one frame: data should be zero-padded into frame 0 and the
    // remaining frames should be replicas. We can't easily inspect frames
    // directly so verify the table is queryable and bounded.
    std::vector<float> shortBuf ((size_t) (Wavetable::kFrameSize / 2));
    for (size_t i = 0; i < shortBuf.size(); ++i)
        shortBuf[i] = std::sin (juce::MathConstants<float>::twoPi
                                 * (float) i / (float) (Wavetable::kFrameSize / 2));

    Wavetable wt;
    wt.buildFromMonoAudio (shortBuf.data(), (int) shortBuf.size());

    const float r = rmsOfFrame (wt, 0.0f, 0);
    VK_EXPECT_GT (r, 0.05f);

    // All frames should match (single replicated frame), so any position
    // produces the same RMS.
    const float r1 = rmsOfFrame (wt, 1.0f, 0);
    VK_EXPECT_NEAR (r, r1, 0.05f);
}

VK_TEST (Wavetable_BuildFromMonoAudioMultiFrame)
{
    // Generate kNumFrames distinct frames; each frame is a different harmonic
    // so the build path should produce N distinct frames with measurably
    // different alt-diff RMS.
    const int frames = Wavetable::kNumFrames;
    std::vector<float> samples ((size_t) (frames * Wavetable::kFrameSize), 0.0f);
    for (int f = 0; f < frames; ++f)
    {
        const float harmonic = 1.0f + (float) f * 2.0f;
        for (int i = 0; i < Wavetable::kFrameSize; ++i)
        {
            const float t = (float) i / (float) Wavetable::kFrameSize;
            samples[(size_t) (f * Wavetable::kFrameSize + i)]
                = std::sin (juce::MathConstants<float>::twoPi * harmonic * t);
        }
    }

    Wavetable wt;
    wt.buildFromMonoAudio (samples.data(), (int) samples.size());

    const float lo = altDiffRms (wt, 0.0f, 0);
    const float hi = altDiffRms (wt, 1.0f, 0);
    // Frame 0 was a fundamental sine, frame last had the most harmonics.
    VK_EXPECT_GT (hi, lo);
}

VK_TEST (Wavetable_PositionInterpolatesBetweenFrames)
{
    // Build a wavetable where frame 0 is +1 (DC) and the remaining frames are
    // -1 (DC). Position 0 should sample +1, position 1 should sample -1, and
    // intermediate positions should fall in between. (After mip building the
    // DC bin is preserved at mip 0.)
    const int frames = Wavetable::kNumFrames;
    std::vector<float> samples ((size_t) (frames * Wavetable::kFrameSize), -1.0f);
    for (int i = 0; i < Wavetable::kFrameSize; ++i)
        samples[(size_t) i] = 1.0f;

    Wavetable wt;
    wt.buildFromMonoAudio (samples.data(), (int) samples.size());

    // Mip 0 with full bin set: peak normalisation will rescale to ~0.99 of
    // the absolute peak. Both frames are constants of equal magnitude so the
    // signs remain.
    const float at0 = wt.sample (0.0f, 0.5f, 0);
    const float at1 = wt.sample (1.0f, 0.5f, 0);
    VK_EXPECT_GT (at0, 0.0f);
    VK_EXPECT_LT (at1, 0.0f);

    // The interpolation between adjacent frames should be monotonic.
    const float a = wt.sample (0.05f, 0.5f, 0);
    const float b = wt.sample (0.5f / 7.0f, 0.5f, 0);
    const float c = wt.sample (1.5f / 7.0f, 0.5f, 0);
    VK_EXPECT (a >= b);
    VK_EXPECT (b >= c);
}
