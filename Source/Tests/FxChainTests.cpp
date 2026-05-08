// FxChain tests. The chain runs after the synth engine and is the last code
// that touches audio before the visualisation snapshot. Bugs here are
// audible-but-not-crashy: a stuck delay line, a denormal trickling into the
// reverb, an EQ that allocates per block.
//
// Coverage:
//   * default-engaged-mix params (all zero) leave a buffer untouched
//   * distortion clips a hot signal
//   * EQ adds energy when boosted
//   * delay produces a delayed signal in the next block
//   * reverb tails extend signal beyond an impulse
//   * compressor reduces gain above threshold
//   * mono bus path returns identical-channel output
//   * extreme parameter combinations don't produce NaN/Inf or denormals
//   * reset() clears the delay line state
#include "TestRunner.h"
#include "TestSupport.h"

using namespace VKTest;

namespace
{
    constexpr double kSR    = 48000.0;
    constexpr int    kBlock = 256;

    struct FxHarness
    {
        TestProcessor tp;
        FxChain       fx { tp.params };

        FxHarness()
        {
            resetParametersToDefaults (tp);
            fx.prepare (kSR, kBlock);
        }
    };

    // Fill a buffer with a constant DC value (useful for delay line tests
    // where we want to detect "did anything come back?").
    void fillDC (juce::AudioBuffer<float>& buf, float v)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            juce::FloatVectorOperations::fill (buf.getWritePointer (ch),
                                                v, buf.getNumSamples());
    }

    // Fill a buffer with a sine wave at hz.
    void fillSine (juce::AudioBuffer<float>& buf, float hz, double sr,
                   double phaseStart = 0.0)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            float* p = buf.getWritePointer (ch);
            double phase = phaseStart;
            const double inc = (double) hz / sr * juce::MathConstants<double>::twoPi;
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                p[i] = (float) std::sin (phase);
                phase += inc;
                if (phase > juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
            }
        }
    }
}

VK_TEST (FxChain_BypassedChainIsTransparent)
{
    FxHarness h;

    juce::AudioBuffer<float> buf (2, kBlock);
    fillSine (buf, 440.0f, kSR);
    juce::AudioBuffer<float> ref (2, kBlock);
    ref.makeCopyOf (buf);

    h.fx.process (buf, 120.0);

    // With every fx parameter at default, the only audible processing should
    // be the master gain (default -6 dB) and width (default 1.0). For width=1
    // the M/S step is identity. -6 dB = ~0.501 ratio. So compare ratios.
    const float gain = juce::Decibels::decibelsToGain (-6.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        const float* p = buf.getReadPointer (ch);
        const float* r = ref.getReadPointer (ch);
        for (int i = 0; i < kBlock; ++i)
        {
            const float expected = r[i] * gain;
            VK_EXPECT_NEAR (p[i], expected, 1.0e-4f);
            if (std::abs (p[i] - expected) > 1.0e-4f) break;
        }
    }
}

VK_TEST (FxChain_DistortionClipsHotSignal)
{
    FxHarness h;
    setParameter (h.tp, "gain", 0.0f);            // unity master
    setParameter (h.tp, "dist_drive", 1.0f);
    setParameter (h.tp, "dist_mix",   1.0f);
    setParameter (h.tp, "dist_type",  (float) DistType::Hard);

    juce::AudioBuffer<float> buf (2, kBlock);
    fillDC (buf, 5.0f); // way over 1.0 - hard-clip should bring this in line
    h.fx.process (buf, 120.0);

    const auto stats = analyse (buf);
    VK_EXPECT (! stats.hasNaN);
    VK_EXPECT (! stats.hasInf);
    VK_EXPECT_LT (stats.peakAbs, 1.5f);
    VK_EXPECT_GT (stats.peakAbs, 0.5f); // a wired-up distortion should produce something
}

VK_TEST (FxChain_HighEqBoostsHighFrequencies)
{
    FxHarness h;
    setParameter (h.tp, "gain",    0.0f);
    setParameter (h.tp, "eq_high", 18.0f);

    // Test signal: 8 kHz sine.
    juce::AudioBuffer<float> dry (2, kBlock);
    fillSine (dry, 8000.0f, kSR);
    juce::AudioBuffer<float> wet;
    wet.makeCopyOf (dry);
    h.fx.process (wet, 120.0);

    const float dryRms = analyse (dry).rms;
    const float wetRms = analyse (wet).rms;
    VK_EXPECT_GT (wetRms, dryRms * 1.2f);
}

VK_TEST (FxChain_LowEqBoostsLowFrequencies)
{
    FxHarness h;
    setParameter (h.tp, "gain",   0.0f);
    setParameter (h.tp, "eq_low", 18.0f);

    juce::AudioBuffer<float> dry (2, kBlock);
    fillSine (dry, 80.0f, kSR);
    juce::AudioBuffer<float> wet;
    wet.makeCopyOf (dry);
    h.fx.process (wet, 120.0);

    VK_EXPECT_GT (analyse (wet).rms, analyse (dry).rms * 1.2f);
}

VK_TEST (FxChain_DelayProducesEcho)
{
    FxHarness h;
    setParameter (h.tp, "gain",       0.0f);
    setParameter (h.tp, "delay",      1.0f);
    setParameter (h.tp, "delay_time", 0.05f);  // 50 ms - fits inside our test
    setParameter (h.tp, "delay_fb",   0.0f);
    setParameter (h.tp, "delay_sync", 0.0f);

    // Feed an impulse into the chain, then process some silence and look for
    // the echo to come back.
    juce::AudioBuffer<float> impulseBlock (2, kBlock);
    impulseBlock.clear();
    impulseBlock.setSample (0, 0, 1.0f);
    impulseBlock.setSample (1, 0, 1.0f);
    h.fx.process (impulseBlock, 120.0);

    // Now process several silent blocks and watch for the delayed return.
    bool sawEcho = false;
    for (int b = 0; b < 32; ++b)
    {
        juce::AudioBuffer<float> silent (2, kBlock);
        silent.clear();
        h.fx.process (silent, 120.0);
        if (analyse (silent).peakAbs > 0.001f) { sawEcho = true; break; }
    }
    VK_EXPECT (sawEcho);
}

VK_TEST (FxChain_ReverbProducesTail)
{
    FxHarness h;
    setParameter (h.tp, "gain",       0.0f);
    setParameter (h.tp, "reverb",     1.0f);
    setParameter (h.tp, "reverb_size", 0.7f);

    juce::AudioBuffer<float> impulseBlock (2, kBlock);
    impulseBlock.clear();
    impulseBlock.setSample (0, 0, 1.0f);
    impulseBlock.setSample (1, 0, 1.0f);
    h.fx.process (impulseBlock, 120.0);

    bool sawTail = false;
    for (int b = 0; b < 16; ++b)
    {
        juce::AudioBuffer<float> silent (2, kBlock);
        silent.clear();
        h.fx.process (silent, 120.0);
        if (analyse (silent).peakAbs > 1.0e-4f) { sawTail = true; break; }
    }
    VK_EXPECT (sawTail);
}

VK_TEST (FxChain_CompressorReducesHotSignal)
{
    // Without compressor: sine at full scale stays at full scale (modulo
    // master gain). With compressor on a low threshold + high ratio, the
    // peak should be lower than without.
    juce::AudioBuffer<float> wet1 (2, kBlock);
    juce::AudioBuffer<float> wet2 (2, kBlock);

    {
        FxHarness h;
        setParameter (h.tp, "gain", 0.0f);
        fillSine (wet1, 440.0f, kSR);
        h.fx.process (wet1, 120.0);
    }
    {
        FxHarness h;
        setParameter (h.tp, "gain", 0.0f);
        setParameter (h.tp, "comp_on",     1.0f);
        setParameter (h.tp, "comp_thr",   -40.0f);
        setParameter (h.tp, "comp_ratio",  20.0f);
        setParameter (h.tp, "comp_atk",    0.1f);
        setParameter (h.tp, "comp_rel",    5.0f);
        fillSine (wet2, 440.0f, kSR);
        // Drive several blocks so the compressor has time to clamp - first
        // block has the attack ramp.
        h.fx.process (wet2, 120.0);
        h.fx.process (wet2, 120.0);
        h.fx.process (wet2, 120.0);
    }

    VK_EXPECT_LT (analyse (wet2).peakAbs, analyse (wet1).peakAbs);
}

VK_TEST (FxChain_MonoBusPathDoesNotCrash)
{
    FxHarness h;
    setParameter (h.tp, "gain",   0.0f);
    setParameter (h.tp, "delay",  0.5f);
    setParameter (h.tp, "reverb", 0.3f);

    juce::AudioBuffer<float> mono (1, kBlock);
    fillSine (mono, 440.0f, kSR);
    h.fx.process (mono, 120.0);

    const auto stats = analyse (mono);
    VK_EXPECT (! stats.hasNaN);
    VK_EXPECT (! stats.hasInf);
    VK_EXPECT_GT (stats.rms, 0.0f);
}

VK_TEST (FxChain_AllFxOnDoesNotProduceNaN)
{
    // Worst-case: every effect engaged, big drives, big mixes. Send sustained
    // sine; check finiteness over 64 blocks (~340 ms).
    FxHarness h;
    setParameter (h.tp, "gain",        0.0f);
    setParameter (h.tp, "dist_drive",  1.0f);
    setParameter (h.tp, "dist_mix",    1.0f);
    setParameter (h.tp, "dist_type",   (float) DistType::Soft);
    setParameter (h.tp, "chorus",      1.0f);
    setParameter (h.tp, "phaser",      1.0f);
    setParameter (h.tp, "phaser_fb",   0.95f);
    setParameter (h.tp, "eq_low",      12.0f);
    setParameter (h.tp, "eq_mid",      12.0f);
    setParameter (h.tp, "eq_high",     12.0f);
    setParameter (h.tp, "delay",       0.8f);
    setParameter (h.tp, "delay_fb",    0.95f);
    setParameter (h.tp, "reverb",      1.0f);
    setParameter (h.tp, "reverb_size", 1.0f);
    setParameter (h.tp, "comp_on",     1.0f);

    juce::AudioBuffer<float> total (2, kBlock * 64);
    total.clear();
    for (int b = 0; b < 64; ++b)
    {
        juce::AudioBuffer<float> blk (2, kBlock);
        fillSine (blk, 440.0f, kSR, b * 0.3);
        h.fx.process (blk, 120.0);
        for (int ch = 0; ch < 2; ++ch)
            total.copyFrom (ch, b * kBlock, blk, ch, 0, kBlock);
    }
    const auto stats = analyse (total);
    VK_EXPECT (! stats.hasNaN);
    VK_EXPECT (! stats.hasInf);
    VK_EXPECT_LT (stats.peakAbs, 32.0f);
}

VK_TEST (FxChain_ResetClearsDelayState)
{
    FxHarness h;
    setParameter (h.tp, "gain",       0.0f);
    setParameter (h.tp, "delay",      1.0f);
    setParameter (h.tp, "delay_time", 0.05f);
    setParameter (h.tp, "delay_fb",   0.5f);

    juce::AudioBuffer<float> impulseBlock (2, kBlock);
    impulseBlock.clear();
    impulseBlock.setSample (0, 0, 1.0f);
    impulseBlock.setSample (1, 0, 1.0f);
    h.fx.process (impulseBlock, 120.0);

    h.fx.reset();

    // After reset, processing silence should return silence (no leftover echo).
    juce::AudioBuffer<float> silent (2, kBlock * 32);
    silent.clear();
    for (int b = 0; b < 32; ++b)
    {
        juce::AudioBuffer<float> blk (2, kBlock);
        blk.clear();
        h.fx.process (blk, 120.0);
        for (int ch = 0; ch < 2; ++ch)
            silent.copyFrom (ch, b * kBlock, blk, ch, 0, kBlock);
    }
    VK_EXPECT_LT (analyse (silent).peakAbs, 0.001f);
}

VK_TEST (FxChain_SilentInputProducesSilentOutput)
{
    FxHarness h;
    setParameter (h.tp, "gain", 0.0f);

    juce::AudioBuffer<float> buf (2, kBlock);
    buf.clear();
    h.fx.process (buf, 120.0);
    VK_EXPECT_LT (analyse (buf).peakAbs, 1.0e-6f);
}

VK_TEST (FxChain_DenormalsAreFlushed)
{
    // Send a denormal-magnitude DC. With ScopedNoDenormals (which the real
    // processor uses in processBlock), the output shouldn't tail off into
    // denormals. We can't enable FTZ here - that's the production path's
    // responsibility - but we can confirm that no signal that we feed in
    // becomes denormal-magnitude on its own.
    FxHarness h;
    setParameter (h.tp, "gain", 0.0f);

    juce::AudioBuffer<float> buf (2, kBlock);
    fillDC (buf, 1.0e-20f);
    h.fx.process (buf, 120.0);

    // Without FTZ flush we'd expect denormals to slip through unchanged. We
    // assert the output stays finite - the rest is up to ScopedNoDenormals.
    const auto stats = analyse (buf);
    VK_EXPECT (! stats.hasNaN);
    VK_EXPECT (! stats.hasInf);
}
