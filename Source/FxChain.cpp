#include "FxChain.h"

namespace
{
    inline float distort (float x, int type, float drive)
    {
        const float d = 1.0f + drive * 9.0f;
        const float xd = x * d;
        switch (type)
        {
            case DistType::Soft:
            {
                const float x2 = xd * xd;
                return xd * (27.0f + x2) / (27.0f + 9.0f * x2);
            }
            case DistType::Hard:
                return juce::jlimit (-1.0f, 1.0f, xd);
            case DistType::Fold:
            {
                float v = std::fmod (xd + 1.0f, 4.0f);
                if (v < 0) v += 4.0f;
                return std::abs (v - 2.0f) - 1.0f;
            }
            case DistType::Bit:
            {
                const float steps = std::pow (2.0f, 8.0f - drive * 6.5f);
                return std::round (xd * steps) / steps;
            }
        }
        return xd;
    }
}

FxChain::FxChain (SynthParams& sp) : params (sp) {}

void FxChain::prepare (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    chorusFx.prepare (spec); chorusFx.setCentreDelay (7.0f); chorusFx.setFeedback (0.2f);
    phaserFx.prepare (spec);
    compFx.prepare (spec);
    delayL.prepare (spec); delayL.setMaximumDelayInSamples ((int) (sampleRate * 2.0));
    delayR.prepare (spec); delayR.setMaximumDelayInSamples ((int) (sampleRate * 2.0));
    delayL.reset(); delayR.reset();
    plate.prepare (sampleRate);

    juce::dsp::ProcessSpec mono { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    eqLowL.prepare (mono); eqLowR.prepare (mono);
    eqMidL.prepare (mono); eqMidR.prepare (mono);
    eqHighL.prepare (mono); eqHighR.prepare (mono);

    delaySmoothedL.reset (sampleRate, 0.05);
    delaySmoothedR.reset (sampleRate, 0.05);

    // 2-stage oversampler = 4x. Polyphase IIR halfband: zero reported
    // latency, sub-sample group delay (good enough for distortion - we're
    // shaping the harmonic content, the group-delay smear is way below
    // any audible threshold). FIR equiripple would be linear-phase but
    // would introduce a fixed sample latency we'd have to advertise to
    // the host; the synth has no need for that.
    distOversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2,                                                             // channels
        2,                                                             // factor: 2 stages = 4x
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true,                                                          // max quality
        false);                                                        // useIntegerLatency
    distOversampler->initProcessing ((size_t) samplesPerBlock);
    distOversampler->reset();

    // Force EQ coefficients to be rebuilt on the first block at the new rate.
    prevEqLowG = prevEqMidG = prevEqMidF = prevEqHighG = 1.0e9f;

    // Pre-size the mono-bus stereo scratch so the audio thread never grows it.
    stereoScratch.setSize (2, samplesPerBlock, false, false, false);
    stereoScratch.clear();
}

void FxChain::reset()
{
    delayL.reset();
    delayR.reset();
    delaySmoothedL.setCurrentAndTargetValue (0.0f);
    delaySmoothedR.setCurrentAndTargetValue (0.0f);

    chorusFx.reset();
    phaserFx.reset();
    compFx.reset();
    eqLowL.reset();  eqLowR.reset();
    eqMidL.reset();  eqMidR.reset();
    eqHighL.reset(); eqHighR.reset();
    plate.reset();
    if (distOversampler) distOversampler->reset();
}

void FxChain::process (juce::AudioBuffer<float>& buffer, double currentBpm)
{
    const int numCh = buffer.getNumChannels();
    if (numCh < 1) return;

    if (numCh == 1)
    {
        // Mono bus: broadcast the input into a stereo scratch buffer, run the
        // stereo FX path on the scratch, then sum back to mono. This keeps
        // the FX behaving as designed (independent L/R EQ and distortion,
        // ping-pong delay, M/S width) and the final mono fold is a single
        // average rather than the doubled / scrambled output you'd get if
        // R aliased to L on the input buffer.
        const int n = buffer.getNumSamples();
        if (stereoScratch.getNumSamples() < n) return;

        auto* src = buffer.getReadPointer (0);
        auto* sL  = stereoScratch.getWritePointer (0);
        auto* sR  = stereoScratch.getWritePointer (1);
        juce::FloatVectorOperations::copy (sL, src, n);
        juce::FloatVectorOperations::copy (sR, src, n);

        float* views[2] = { sL, sR };
        juce::AudioBuffer<float> stereoView (views, 2, n);
        processStereo (stereoView, currentBpm);

        auto* dst = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i)
            dst[i] = 0.5f * (sL[i] + sR[i]);
        return;
    }

    processStereo (buffer, currentBpm);
}

void FxChain::processDistortionOversampled (juce::AudioBuffer<float>& buffer,
                                            int type, float drive, float mix)
{
    // Hot loop runs at 4x sr inside the upsampled block: hard-clip / fold /
    // bit-crush all generate broadband harmonics that would alias hard at
    // base sr. Soft-clip aliases less but still benefits, and we keep the
    // oversampler engaged unconditionally so the chain's phase response
    // doesn't step when the user automates dist_mix.
    juce::dsp::AudioBlock<float> block (buffer);
    auto upBlock = distOversampler->processSamplesUp (block);

    const int numCh = (int) upBlock.getNumChannels();
    const int upN   = (int) upBlock.getNumSamples();
    const float wet = mix;
    const float dry = 1.0f - mix;

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* p = upBlock.getChannelPointer ((size_t) ch);
        for (int i = 0; i < upN; ++i)
            p[i] = p[i] * dry + distort (p[i], type, drive) * wet;
    }

    distOversampler->processSamplesDown (block);
}

void FxChain::processStereo (juce::AudioBuffer<float>& buffer, double currentBpm)
{
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    const int n = buffer.getNumSamples();

    // Distortion (4x oversampled). Bypassing the oversampler when distortion
    // is off would make automating dist_mix audibly step the chain's phase
    // response, so we always run through it; if mix * drive is effectively
    // zero we just skip the distort() call and pay the (tiny) IIR halfband
    // up/down cost.
    {
        const float dMix = *params.distMix;
        const float drv  = *params.distDrive;
        if (dMix > 0.001f && drv > 0.0001f)
        {
            const int type = (int) (params.distType->load() + 0.5f);
            processDistortionOversampled (buffer, type, drv, dMix);
        }
    }

    // Chorus
    {
        const float chMix = *params.chorus;
        if (chMix > 0.001f)
        {
            chorusFx.setMix (chMix);
            chorusFx.setRate (*params.chorusRate);
            chorusFx.setDepth (*params.chorusDepth);
            juce::dsp::AudioBlock<float> blk (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (blk);
            chorusFx.process (ctx);
        }
    }

    // Phaser
    {
        const float phMix = *params.phaser;
        if (phMix > 0.001f)
        {
            phaserFx.setMix (phMix);
            phaserFx.setRate (*params.phaserRate);
            phaserFx.setDepth (*params.phaserDepth);
            phaserFx.setFeedback (*params.phaserFb);
            phaserFx.setCentreFrequency (1300.0f);
            juce::dsp::AudioBlock<float> blk (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (blk);
            phaserFx.process (ctx);
        }
    }

    // EQ (3-band: low shelf, peak mid, high shelf). The make* helpers each
    // allocate a ReferenceCountedObject under the hood, so we only call them
    // when an input parameter actually changes.
    {
        const float lowG  = *params.eqLow;
        const float midG  = *params.eqMid;
        const float midF  = *params.eqMidF;
        const float highG = *params.eqHigh;
        if (std::abs (lowG) + std::abs (midG) + std::abs (highG) > 0.05f)
        {
            constexpr float kEqEps = 1.0e-6f;
            if (std::abs (lowG - prevEqLowG) > kEqEps)
            {
                *eqLowL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 200.0f, 0.707f, juce::Decibels::decibelsToGain (lowG));
                *eqLowR.coefficients = *eqLowL.coefficients;
                prevEqLowG = lowG;
            }
            if (std::abs (midG - prevEqMidG) > kEqEps || std::abs (midF - prevEqMidF) > kEqEps)
            {
                *eqMidL.coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, midF, 0.8f, juce::Decibels::decibelsToGain (midG));
                *eqMidR.coefficients = *eqMidL.coefficients;
                prevEqMidG = midG;
                prevEqMidF = midF;
            }
            if (std::abs (highG - prevEqHighG) > kEqEps)
            {
                *eqHighL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 5000.0f, 0.707f, juce::Decibels::decibelsToGain (highG));
                *eqHighR.coefficients = *eqHighL.coefficients;
                prevEqHighG = highG;
            }

            for (int i = 0; i < n; ++i)
            {
                L[i] = eqHighL.processSample (eqMidL.processSample (eqLowL.processSample (L[i])));
                R[i] = eqHighR.processSample (eqMidR.processSample (eqLowR.processSample (R[i])));
            }
        }
    }

    // Stereo delay (ping-pong-ish), tempo sync optional
    {
        const float dMix = *params.delay;
        if (dMix > 0.001f)
        {
            float dt;
            if (*params.delaySync > 0.5f)
            {
                const int divIdx = (int) (params.delayDiv->load() + 0.5f);
                const double beats = syncDivToBeats (divIdx);
                dt = (float) (beats * 60.0 / juce::jmax (20.0, currentBpm));
                dt = juce::jlimit (0.005f, 1.5f, dt);
            }
            else
            {
                dt = juce::jlimit (0.005f, 1.5f, params.delayTime->load());
            }
            const float fb = juce::jlimit (0.0f, 0.95f, params.delayFb->load());
            delaySmoothedL.setTargetValue ((float) (dt * sr));
            delaySmoothedR.setTargetValue ((float) (dt * sr * 1.07f));
            for (int i = 0; i < n; ++i)
            {
                delayL.setDelay (delaySmoothedL.getNextValue());
                delayR.setDelay (delaySmoothedR.getNextValue());
                const float dlOut = delayL.popSample (0);
                const float drOut = delayR.popSample (0);
                delayL.pushSample (0, L[i] + drOut * fb);
                delayR.pushSample (0, R[i] + dlOut * fb);
                L[i] += dlOut * dMix;
                R[i] += drOut * dMix;
            }
        }
    }

    // Reverb (Dattorro plate). The plate runs sample-by-sample because the
    // tank's cross-coupled feedback is per-sample - the previous juce::Reverb
    // ran a block-at-a-time but inside it does the same per-sample work, so
    // the wall-clock cost is similar.
    {
        const float rMix = *params.reverb;
        if (rMix > 0.001f)
        {
            plate.setSize    (juce::jlimit (0.0f, 1.0f, params.reverbSize->load()));
            plate.setDamping (juce::jlimit (0.0f, 1.0f, params.reverbDamp->load()));

            const float wet = rMix;
            for (int i = 0; i < n; ++i)
            {
                float wL, wR;
                plate.process (L[i], R[i], wL, wR);
                L[i] += wL * wet;
                R[i] += wR * wet;
            }
        }
    }

    // Master width (M/S) and gain
    {
        const float w = juce::jlimit (0.0f, 2.0f, params.width->load() + params.modSum[ModDest::Width] * 0.5f);
        const float g = juce::Decibels::decibelsToGain (params.gain->load());
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            const float s = 0.5f * (L[i] - R[i]) * w;
            L[i] = (m + s) * g;
            R[i] = (m - s) * g;
        }
    }

    // Compressor (post)
    if (*params.compOn > 0.5f)
    {
        compFx.setThreshold (*params.compThr);
        compFx.setRatio (*params.compRatio);
        compFx.setAttack (*params.compAtk);
        compFx.setRelease (*params.compRel);
        juce::dsp::AudioBlock<float> blk (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        compFx.process (ctx);
        const float mk = juce::Decibels::decibelsToGain (params.compMakeup->load());
        if (std::abs (mk - 1.0f) > 0.001f) buffer.applyGain (mk);
    }
}
