#include "SynthVoice.h"
#include "PluginProcessor.h"

WTVoice::WTVoice (SynthParams& p) : params (p) {}

void WTVoice::prepare (double sampleRate)
{
    sr = sampleRate;
    ampEnv.setSampleRate (sampleRate);
    modEnv.setSampleRate (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };
    filterL.prepare (spec);
    filterR.prepare (spec);
    filterL.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filterR.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
}

void WTVoice::startNote (int midiNote, float velocity, juce::SynthesiserSound*, int)
{
    baseFreq = (float) juce::MidiMessage::getMidiNoteInHertz (midiNote);
    velocityGain = 0.3f + 0.7f * velocity;
    noteHeld = true;

    ampP = { *params.aA, *params.aD, *params.aS, *params.aR };
    modP = { *params.mA, *params.mD, *params.mS, *params.mR };
    ampEnv.setParameters (ampP);
    modEnv.setParameters (modP);
    ampEnv.noteOn();
    modEnv.noteOn();

    for (auto& o : osc) { o.phase = 0.0f; o.driftPhase = rng.nextFloat(); for (auto& u : o.uPhases) u = rng.nextFloat(); }
    filterL.reset(); filterR.reset();
}

void WTVoice::stopNote (float, bool allowTailOff)
{
    noteHeld = false;
    if (allowTailOff)
    {
        ampEnv.noteOff();
        modEnv.noteOff();
    }
    else
    {
        clearCurrentNote();
        ampEnv.reset();
        modEnv.reset();
    }
}

static inline float fastTanh (float x) noexcept
{
    // Pade-style soft clip; cheap and pleasant.
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

void WTVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (! isVoiceActive()) return;

    auto& lib = WavetableLibrary::get();

    const float gritAmt  = *params.grit;
    const float vibeAmt  = *params.vibe;
    const float driftAmt = *params.drift;
    const float satAmt   = *params.sat;

    const float fCut  = *params.fCut;
    const float fRes  = *params.fRes;
    const float fEnv  = *params.fEnv;
    const int   fType = (int) *params.fType;

    filterL.setType ((juce::dsp::StateVariableTPTFilterType) fType);
    filterR.setType ((juce::dsp::StateVariableTPTFilterType) fType);
    filterL.setResonance (fRes);
    filterR.setResonance (fRes);

    const int numCh = juce::jmin (2, outputBuffer.getNumChannels());
    auto* outL = outputBuffer.getWritePointer (0, startSample);
    auto* outR = numCh > 1 ? outputBuffer.getWritePointer (1, startSample) : outL;

    // Pre-compute per-osc params
    struct OP {
        bool on; int shape; float pos; float lin; float panL, panR;
        float ratio; int unison; float det;
    };
    OP op[3];
    for (int i = 0; i < 3; ++i)
    {
        op[i].on     = *params.osc[i].on > 0.5f;
        op[i].shape  = (int) *params.osc[i].shape;
        op[i].pos    = juce::jlimit (0.0f, 1.0f, params.osc[i].position->load());
        op[i].lin    = juce::Decibels::decibelsToGain (params.osc[i].level->load());
        const float pan = juce::jlimit (-1.0f, 1.0f, params.osc[i].pan->load());
        op[i].panL   = std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
        op[i].panR   = std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
        op[i].ratio  = std::pow (2.0f, (params.osc[i].coarse->load() + params.osc[i].fine->load() * 0.01f) / 12.0f);
        op[i].unison = juce::jlimit (1, 7, (int) params.osc[i].unison->load());
        op[i].det    = params.osc[i].detune->load() * 0.04f; // up to ~4%
    }

    const float lfo1Inc = (*params.lfo1Rate) / (float) sr;
    const float lfo2Inc = (*params.lfo2Rate) / (float) sr;
    const float lfo1Amt = *params.lfo1Amt;
    const float lfo2Amt = *params.lfo2Amt;

    for (int s = 0; s < numSamples; ++s)
    {
        const float ampE = ampEnv.getNextSample();
        const float modE = modEnv.getNextSample();

        // LFOs
        lfoPhase[0] += lfo1Inc; if (lfoPhase[0] >= 1.0f) lfoPhase[0] -= 1.0f;
        lfoPhase[1] += lfo2Inc; if (lfoPhase[1] >= 1.0f) lfoPhase[1] -= 1.0f;
        const float lfo1 = std::sin (lfoPhase[0] * juce::MathConstants<float>::twoPi);
        const float lfo2 = std::sin (lfoPhase[1] * juce::MathConstants<float>::twoPi);

        // Filter cutoff modulation
        float cutoffHz = fCut * std::pow (2.0f, fEnv * 4.0f * modE + lfo1Amt * lfo1 * 2.0f);
        cutoffHz = juce::jlimit (20.0f, 18000.0f, cutoffHz);
        filterL.setCutoffFrequency (cutoffHz);
        filterR.setCutoffFrequency (cutoffHz);

        float sumL = 0.0f, sumR = 0.0f;

        for (int i = 0; i < 3; ++i)
        {
            if (! op[i].on) continue;
            const auto& wt = lib.getTable (op[i].shape);

            // Drift LFO per oscillator (very slow, decorrelated)
            osc[i].driftPhase += 0.07f / (float) sr; if (osc[i].driftPhase >= 1.0f) osc[i].driftPhase -= 1.0f;
            const float drift = std::sin (osc[i].driftPhase * juce::MathConstants<float>::twoPi) * driftAmt * 0.005f;

            const float baseHz = baseFreq * op[i].ratio * (1.0f + drift);
            const float pos = juce::jlimit (0.0f, 1.0f, op[i].pos + lfo2Amt * lfo2 * 0.3f);

            const int U = op[i].unison;
            const float spread = op[i].det;
            float oL = 0.0f, oR = 0.0f;

            for (int u = 0; u < U; ++u)
            {
                const float voiceN = (U == 1) ? 0.0f : ((float) u / (float) (U - 1) - 0.5f) * 2.0f;
                const float det = 1.0f + voiceN * spread;
                const float hz = baseHz * det;
                const float inc = hz / (float) sr;

                // Phase jitter (grit): small per-sample randomization, scaled by frequency
                const float jitter = (rng.nextFloat() - 0.5f) * gritAmt * 0.002f;

                osc[i].uPhases[(size_t) u] += inc + jitter;
                while (osc[i].uPhases[(size_t) u] >= 1.0f) osc[i].uPhases[(size_t) u] -= 1.0f;
                while (osc[i].uPhases[(size_t) u] <  0.0f) osc[i].uPhases[(size_t) u] += 1.0f;

                const int mip = Wavetable::chooseMip (inc);
                const float v = wt.sample (pos, osc[i].uPhases[(size_t) u], mip);

                const float pan = (U == 1) ? 0.0f : voiceN; // unison spread
                const float pl = std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
                const float pr = std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
                oL += v * pl;
                oR += v * pr;
            }

            const float invU = 1.0f / std::sqrt ((float) U);
            sumL += oL * invU * op[i].lin * op[i].panL;
            sumR += oR * invU * op[i].lin * op[i].panR;
        }

        // Background "vibe" hiss
        if (vibeAmt > 0.0f)
        {
            const float n = (rng.nextFloat() - 0.5f) * vibeAmt * 0.05f;
            sumL += n; sumR += n * 0.7f;
        }

        // Filter
        sumL = filterL.processSample (0, sumL);
        sumR = filterR.processSample (0, sumR);

        // Saturation
        if (satAmt > 0.0f)
        {
            const float drive = 1.0f + satAmt * 4.0f;
            sumL = fastTanh (sumL * drive) / drive * (1.0f + satAmt * 0.5f);
            sumR = fastTanh (sumR * drive) / drive * (1.0f + satAmt * 0.5f);
        }

        const float g = ampE * velocityGain;
        outL[s] += sumL * g;
        outR[s] += sumR * g;

        if (! ampEnv.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}
