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
    reverbFx.setSampleRate (sampleRate);

    juce::dsp::ProcessSpec mono { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    eqLowL.prepare (mono); eqLowR.prepare (mono);
    eqMidL.prepare (mono); eqMidR.prepare (mono);
    eqHighL.prepare (mono); eqHighR.prepare (mono);

    delaySmoothedL.reset (sampleRate, 0.05);
    delaySmoothedR.reset (sampleRate, 0.05);

    // Force EQ coefficients to be rebuilt on the first block at the new rate.
    eqLowDirty = eqMidDirty = eqHighDirty = true;
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
    reverbFx.reset();
}

void FxChain::process (juce::AudioBuffer<float>& buffer, double currentBpm)
{
    // Like the voice render, FX assumes a stereo buffer; the processor
    // renders into an internal stereo scratch for mono output buses and
    // mixes down at the boundary, so distortion / EQ / delay / width never
    // run with aliased L == R (which would double-process the same samples
    // and break the mono signal).
    jassert (buffer.getNumChannels() == 2);

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    const int n = buffer.getNumSamples();

    // Distortion
    {
        const float dMix = *params.distMix;
        const float drv  = *params.distDrive;
        if (dMix > 0.001f && drv > 0.0001f)
        {
            const int type = (int) (params.distType->load() + 0.5f);
            const float wet = dMix;
            const float dry = 1.0f - dMix;
            for (int i = 0; i < n; ++i)
            {
                L[i] = L[i] * dry + distort (L[i], type, drv) * wet;
                R[i] = R[i] * dry + distort (R[i], type, drv) * wet;
            }
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
            if (eqLowDirty || std::abs (lowG - prevEqLowG) > kEqEps)
            {
                *eqLowL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 200.0f, 0.707f, juce::Decibels::decibelsToGain (lowG));
                *eqLowR.coefficients = *eqLowL.coefficients;
                prevEqLowG = lowG;
                eqLowDirty = false;
            }
            if (eqMidDirty || std::abs (midG - prevEqMidG) > kEqEps || std::abs (midF - prevEqMidF) > kEqEps)
            {
                *eqMidL.coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, midF, 0.8f, juce::Decibels::decibelsToGain (midG));
                *eqMidR.coefficients = *eqMidL.coefficients;
                prevEqMidG = midG;
                prevEqMidF = midF;
                eqMidDirty = false;
            }
            if (eqHighDirty || std::abs (highG - prevEqHighG) > kEqEps)
            {
                *eqHighL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 5000.0f, 0.707f, juce::Decibels::decibelsToGain (highG));
                *eqHighR.coefficients = *eqHighL.coefficients;
                prevEqHighG = highG;
                eqHighDirty = false;
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

    // Reverb
    {
        const float rMix = *params.reverb;
        if (rMix > 0.001f)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = juce::jlimit (0.0f, 1.0f, params.reverbSize->load());
            rp.damping  = juce::jlimit (0.0f, 1.0f, params.reverbDamp->load());
            rp.wetLevel = rMix * 0.5f;
            rp.dryLevel = 1.0f;
            rp.width    = 1.0f;
            reverbFx.setParameters (rp);
            reverbFx.processStereo (L, R, n);
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
