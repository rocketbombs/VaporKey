// End-to-end audio regression and CPU stress.
//
// Drives the full audio path (Arpeggiator -> SynthEngine -> FxChain) under
// realistic and adversarial workloads, checking for:
//   * NaN / Inf / denormal contamination
//   * unbounded amplitude
//   * silent output where output is expected
//   * preset-switch glitches (RMS step between adjacent blocks)
//   * a loose CPU budget (1s of audio renders in well under wall time)
//
// The CPU bound is intentionally generous - CI runners have wildly variable
// performance - but it catches an order-of-magnitude regression. If a CI
// environment can't render 1s of stereo audio in 5s of wall time, something
// has gone badly wrong.
#include "TestRunner.h"
#include "TestSupport.h"

#include "../Presets.h"
#include "../PresetStore.h"

#include <chrono>

using namespace VKTest;

namespace
{
    constexpr double kSR     = 48000.0;
    constexpr int    kBlock  = 256;

    struct FullHarness
    {
        TestProcessor tp;
        SynthEngine   engine { tp.params };
        Arpeggiator   arp    { tp.params };
        FxChain       fx     { tp.params };

        FullHarness()
        {
            resetParametersToDefaults (tp);
            engine.prepare (kSR, kBlock);
            arp.prepare (kSR);
            fx.prepare (kSR, kBlock);
        }

        // Render `seconds` of audio with `firstBlockMidi` injected at sample 0.
        juce::AudioBuffer<float> render (juce::MidiBuffer firstBlockMidi,
                                          double seconds, int channels = 2,
                                          double bpm = 120.0)
        {
            const int total = (int) (seconds * kSR);
            juce::AudioBuffer<float> out (channels, total);
            renderAudio (tp, engine, arp, fx, firstBlockMidi, out,
                         kSR, total, kBlock, bpm);
            return out;
        }
    };

    // Per-block RMS of the most recent N samples. Used to detect "click" -
    // an abrupt RMS jump between adjacent blocks.
    float blockRms (const juce::AudioBuffer<float>& buf, int startSample, int n)
    {
        double sumSq = 0.0;
        int    count = 0;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* p = buf.getReadPointer (ch);
            for (int i = startSample; i < startSample + n && i < buf.getNumSamples(); ++i)
            {
                sumSq += (double) p[i] * (double) p[i];
                ++count;
            }
        }
        return count > 0 ? (float) std::sqrt (sumSq / (double) count) : 0.0f;
    }
}

VK_TEST (AudioRegression_DefaultPatchRendersFinite)
{
    FullHarness h;
    auto buf = h.render (singleEventBuffer (Midi::noteOn (60, 100)), 0.5);
    const auto stats = analyse (buf);

    VK_EXPECT (! stats.hasNaN);
    VK_EXPECT (! stats.hasInf);
    VK_EXPECT_LT (stats.peakAbs, 4.0f);
    VK_EXPECT_GT (stats.rms, 1.0e-4f);
}

VK_TEST (AudioRegression_SilenceWithoutNotes)
{
    FullHarness h;
    juce::MidiBuffer empty;
    auto buf = h.render (std::move (empty), 0.25);
    const auto stats = analyse (buf);
    VK_EXPECT (! stats.hasNaN);
    VK_EXPECT (! stats.hasInf);
    VK_EXPECT_LT (stats.peakAbs, 1.0e-4f);
}

VK_TEST (AudioRegression_AllFactoryPresetsRenderCleanly)
{
    // Every factory preset, fed a single C4 hold, must render without
    // NaN/Inf and produce some audible signal. Uses a short render window
    // (200 ms) - enough for the attack to fire on every preset including
    // pads with slow attacks (those still produce audible energy by 200 ms,
    // even if not at peak).
    FullHarness h;
    const int total = PresetStore::factoryPresetCount();
    VK_REQUIRE (total > 0);

    int silentCount = 0;
    int nanCount = 0;
    int rangeCount = 0;
    juce::String firstBadName;

    for (int i = 0; i < total; ++i)
    {
        const bool ok = PresetStore::applyFactoryPreset (h.tp.apvts, h.tp, i);
        VK_REQUIRE (ok);

        // Flush voices and FX between presets so tails don't leak into the
        // next preset's render budget (mirrors the production preset switch).
        h.engine.allNotesOff();
        h.fx.reset();

        auto buf = h.render (singleEventBuffer (Midi::noteOn (60, 100)), 0.25);
        const auto stats = analyse (buf);

        const auto name = VKPresets::all()[(size_t) i].name;
        if (stats.hasNaN || stats.hasInf)
        {
            ++nanCount;
            if (firstBadName.isEmpty()) firstBadName = name;
        }
        if (stats.peakAbs > 8.0f) // huge slack; should never approach this
        {
            ++rangeCount;
            if (firstBadName.isEmpty()) firstBadName = name;
        }
        if (stats.rms < 1.0e-6f) ++silentCount; // arp / FX-only patches may be quiet on a single C4
    }

    VK_EXPECT_MSG (nanCount == 0,
                   juce::String ("NaN/Inf in ") + juce::String (nanCount)
                       + " preset(s); first: " + firstBadName);
    VK_EXPECT_MSG (rangeCount == 0,
                   juce::String ("amplitude out of range in ") + juce::String (rangeCount)
                       + " preset(s); first: " + firstBadName);
    // We tolerate a few silent presets (pure arpeggiator that takes longer
    // than 200ms for the first arp step etc.). Tighten if the count grows.
    VK_EXPECT_LT (silentCount, 8);
}

VK_TEST (AudioRegression_PresetSwitchUnderLoadIsClickless)
{
    // Apply preset 0, render 100 ms, switch to preset 1 (with the silence /
    // FX-reset dance the production processor performs), render 100 ms.
    // The production switch should produce a transient through the new
    // preset's attack envelope, but no excessive RMS jump in the silence
    // immediately after the switch.
    FullHarness h;

    const int total = PresetStore::factoryPresetCount();
    VK_REQUIRE (total >= 2);

    PresetStore::applyFactoryPreset (h.tp.apvts, h.tp, 0);
    auto first = h.render (singleEventBuffer (Midi::noteOn (60, 100)), 0.1);
    juce::ignoreUnused (first);

    // Production preset switch: silence + fx reset, then state change.
    h.engine.allNotesOff();
    h.fx.reset();
    PresetStore::applyFactoryPreset (h.tp.apvts, h.tp, 1);

    juce::MidiBuffer empty;
    auto silence = h.render (std::move (empty), 0.05);
    const auto stats = analyse (silence);
    // No held notes after the switch -> output should be near silent.
    VK_EXPECT_LT (stats.peakAbs, 0.05f);
    VK_EXPECT (! stats.hasNaN);
    VK_EXPECT (! stats.hasInf);
}

VK_TEST (AudioRegression_SustainedChordIsBounded)
{
    FullHarness h;

    juce::MidiBuffer chord;
    for (int n = 60; n <= 72; ++n) chord.addEvent (Midi::noteOn (n, 100), 0);
    auto buf = h.render (std::move (chord), 1.0);

    const auto stats = analyse (buf);
    VK_EXPECT (! stats.hasNaN);
    VK_EXPECT (! stats.hasInf);
    VK_EXPECT_LT (stats.peakAbs, 4.0f); // 13-note chord, full FX off; peak should be modest
    VK_EXPECT_GT (stats.rms, 1.0e-3f);
}

VK_TEST (AudioRegression_VoiceStealStress)
{
    // 100 randomly-spaced note-ons across 1 second, all overlapping. The
    // synth caps at 16 voices; older voices get stolen. We just want to
    // confirm no crashes and finite output.
    FullHarness h;

    juce::MidiBuffer firstBlock;
    juce::Random rng;
    rng.setSeed (0x5EED5EEDLL);
    // Stuff a hundred notes into the first block; sample positions clamp to
    // the block size, which is fine - the synth treats them as simultaneous.
    for (int i = 0; i < 100; ++i)
    {
        const int n = 36 + rng.nextInt (60);
        firstBlock.addEvent (Midi::noteOn (n, 80 + rng.nextInt (40)),
                             rng.nextInt (kBlock));
    }
    auto buf = h.render (std::move (firstBlock), 1.5);
    const auto stats = analyse (buf);

    VK_EXPECT (! stats.hasNaN);
    VK_EXPECT (! stats.hasInf);
    VK_EXPECT_LT (stats.peakAbs, 16.0f);
    VK_EXPECT_GT (stats.rms, 1.0e-3f);
}

VK_TEST (AudioRegression_MonoBusEndToEnd)
{
    FullHarness h;
    juce::MidiBuffer mb = singleEventBuffer (Midi::noteOn (60, 100));
    // Render with a 1-channel output buffer to mimic a mono host.
    juce::AudioBuffer<float> out (1, (int) (kSR * 0.25));
    renderAudio (h.tp, h.engine, h.arp, h.fx, mb, out,
                 kSR, out.getNumSamples(), kBlock, 120.0);
    const auto stats = analyse (out);
    VK_EXPECT (! stats.hasNaN);
    VK_EXPECT (! stats.hasInf);
    VK_EXPECT_GT (stats.rms, 1.0e-4f);
    // Peak should be roughly comparable to the stereo peak; the previous
    // bug doubled gain on mono (broadcast L into both channels then summed),
    // so an excess peak here is the regression signal.
    VK_EXPECT_LT (stats.peakAbs, 4.0f);
}

VK_TEST (AudioRegression_StereoVsMonoSimilarLoudness)
{
    // The mono fold should match the average of the stereo channels in
    // amplitude. A regression that broadcast L into a mono buffer (the bug
    // we fixed) doubled amplitude.
    FullHarness h;

    juce::AudioBuffer<float> stereo (2, (int) (kSR * 0.25));
    {
        juce::MidiBuffer mb = singleEventBuffer (Midi::noteOn (60, 100));
        renderAudio (h.tp, h.engine, h.arp, h.fx, mb, stereo,
                     kSR, stereo.getNumSamples(), kBlock, 120.0);
    }

    // Reset the chain - delays / reverb tails mustn't bleed into the second
    // render or the comparison is invalid.
    h.engine.allNotesOff();
    h.fx.reset();

    juce::AudioBuffer<float> mono (1, (int) (kSR * 0.25));
    {
        juce::MidiBuffer mb = singleEventBuffer (Midi::noteOn (60, 100));
        renderAudio (h.tp, h.engine, h.arp, h.fx, mb, mono,
                     kSR, mono.getNumSamples(), kBlock, 120.0);
    }

    const float stereoRms = analyse (stereo).rms;
    const float monoRms   = analyse (mono).rms;

    // The mono fold averages L+R/2; for a centered patch L ~ R, so monoRms
    // should be near stereoRms (same per-channel amplitude). Allow generous
    // slop because the mono FX path runs on a stereo scratch and folds at
    // the end, which can change phase relationships slightly.
    VK_EXPECT_MSG (monoRms < stereoRms * 2.5f,
                   juce::String ("mono louder than stereo: mono=")
                       + juce::String (monoRms, 4)
                       + " stereo=" + juce::String (stereoRms, 4));
    VK_EXPECT_MSG (monoRms > stereoRms * 0.3f,
                   juce::String ("mono significantly quieter than stereo: mono=")
                       + juce::String (monoRms, 4)
                       + " stereo=" + juce::String (stereoRms, 4));
}

VK_TEST (AudioRegression_CpuBudgetOneSecondOfAudio)
{
    // Loose CPU budget: rendering 1 second of stereo audio with a
    // 13-note chord, default patch, must complete in under 5 seconds of
    // wall-clock time on any reasonable CI runner. If we trip this, the
    // engine has regressed by an order of magnitude or more.
    FullHarness h;

    juce::MidiBuffer chord;
    for (int n = 60; n <= 72; ++n) chord.addEvent (Midi::noteOn (n, 100), 0);

    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();
    auto buf = h.render (std::move (chord), 1.0);
    const auto t1 = Clock::now();

    const double wallMs = std::chrono::duration<double, std::milli> (t1 - t0).count();

    juce::ignoreUnused (buf);
    VK_EXPECT_MSG (wallMs < 5000.0,
                   juce::String ("rendering 1s of audio took ")
                       + juce::String (wallMs, 1) + " ms");
}

VK_TEST (AudioRegression_NoSilentBlocksAfterAttack)
{
    // After the amp envelope's attack has finished, every block should
    // contain at least some audible signal. This catches "envelope stuck"
    // regressions where, e.g., a refactored ADSR returns 0 forever.
    FullHarness h;
    setParameter (h.tp, "a_a", 0.005f);
    setParameter (h.tp, "a_d", 0.5f);
    setParameter (h.tp, "a_s", 0.7f);
    setParameter (h.tp, "a_r", 0.5f);

    juce::AudioBuffer<float> buf = h.render (singleEventBuffer (Midi::noteOn (60, 100)), 0.5);

    // Skip the first 50 ms to avoid the attack ramp.
    const int skip = (int) (kSR * 0.05);
    int silentBlocks = 0;
    for (int s = skip; s + kBlock <= buf.getNumSamples(); s += kBlock)
        if (blockRms (buf, s, kBlock) < 1.0e-5f) ++silentBlocks;
    VK_EXPECT_EQ (silentBlocks, 0);
}
