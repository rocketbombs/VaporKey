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

inline float WTVoice::fastTanh (float x) noexcept
{
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

float WTVoice::nextLfo (int shape, float phase, float& shStateVal, float& shTimerVal,
                        float incPerSample, juce::Random& rng)
{
    using namespace juce;
    switch (shape)
    {
        case LfoShape::Sine:    return std::sin (phase * MathConstants<float>::twoPi);
        case LfoShape::Tri:     return 4.0f * std::abs (phase - 0.5f) - 1.0f;
        case LfoShape::SawUp:   return phase * 2.0f - 1.0f;
        case LfoShape::SawDown: return 1.0f - phase * 2.0f;
        case LfoShape::Square:  return phase < 0.5f ? 1.0f : -1.0f;
        case LfoShape::SH:
        {
            shTimerVal += incPerSample;
            if (shTimerVal >= 1.0f) { shTimerVal -= std::floor (shTimerVal); shStateVal = rng.nextFloat() * 2.0f - 1.0f; }
            return shStateVal;
        }
        default: return 0.0f;
    }
}

void WTVoice::startNote (int midiNote, float velocity, juce::SynthesiserSound*, int /*pwPos*/)
{
    currentNote = midiNote;
    baseFreqTarget = (float) juce::MidiMessage::getMidiNoteInHertz (midiNote);
    if (baseFreqCurrent <= 0.0f) baseFreqCurrent = baseFreqTarget;

    velocityNorm = velocity;
    noteHeld = true;

    juce::ADSR::Parameters ampP { *params.aA, *params.aD, *params.aS, *params.aR };
    juce::ADSR::Parameters modP { *params.mA, *params.mD, *params.mS, *params.mR };
    ampEnv.setParameters (ampP);
    modEnv.setParameters (modP);
    ampEnv.noteOn();
    modEnv.noteOn();

    // Pitch env: re-init level to 1.0; decay coef set to reach ~1% in pEnvDecay
    {
        const float dec = juce::jmax (0.001f, params.pEnvDecay->load());
        pEnvDecayCoef = std::exp (std::log (0.01f) / (dec * (float) sr));
        pEnvLevel = 1.0f;
    }

    // Glide coefficient: how quickly current freq approaches target each sample.
    {
        const float gt = juce::jmax (0.0f, params.glide->load());
        if (gt <= 0.0001f) { glideCoef = 1.0f; baseFreqCurrent = baseFreqTarget; }
        else
        {
            // 63% of the way each "time constant" — so ~5τ for full glide.
            glideCoef = 1.0f - std::exp (-1.0f / (gt * (float) sr * 0.2f));
        }
    }

    // Start phases
    for (int i = 0; i < 3; ++i)
    {
        const float ph = juce::jlimit (-1.0f, 1.0f, params.osc[i].phase->load());
        for (auto& u : osc[i].uPhases) u = (ph < 0.0f) ? rng.nextFloat() : ph;
        osc[i].driftPhase = rng.nextFloat();
    }
    subPhase = 0.0f;
    for (auto& v : pinkB) v = 0.0f;
    brownState = 0.0f;

    filterL.reset(); filterR.reset();
}

void WTVoice::retargetNote (int midiNote, float velocity, bool retriggerEnvelopes)
{
    currentNote = midiNote;
    baseFreqTarget = (float) juce::MidiMessage::getMidiNoteInHertz (midiNote);
    velocityNorm = velocity;
    noteHeld = true;

    if (retriggerEnvelopes)
    {
        juce::ADSR::Parameters ampP { *params.aA, *params.aD, *params.aS, *params.aR };
        juce::ADSR::Parameters modP { *params.mA, *params.mD, *params.mS, *params.mR };
        ampEnv.setParameters (ampP);
        modEnv.setParameters (modP);
        ampEnv.noteOn();
        modEnv.noteOn();
        pEnvLevel = 1.0f;
    }

    const float gt = juce::jmax (0.0f, params.glide->load());
    if (gt <= 0.0001f) { glideCoef = 1.0f; baseFreqCurrent = baseFreqTarget; }
    else               { glideCoef = 1.0f - std::exp (-1.0f / (gt * (float) sr * 0.2f)); }
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
        currentNote = -1;
    }
}

// Helper: read AudioParameterChoice safely.
static inline int rawChoice (const std::atomic<float>* p) { return (int) (p->load() + 0.5f); }

// Apply mod sums (per destination) to a base value, scaled by full range portion.
static inline float modOffset (const SynthParams& sp, int dest, float scaleToBase)
{
    return sp.modSum[dest] * scaleToBase;
}

void WTVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (! isVoiceActive()) return;

    auto& lib = WavetableLibrary::get();

    const float gritAmt  = *params.grit;
    const float vibeAmt  = *params.vibe;
    const float driftAmt = *params.drift;
    const float satAmt   = *params.sat;

    const float pBendSemi = params.pitchBendSemis.load();
    const float pBendFactor = std::pow (2.0f, pBendSemi / 12.0f);
    const float pEnvAmtSemi = params.pEnvAmt->load();

    const float fCutBase = juce::jlimit (20.0f, 18000.0f, params.fCut->load() + modOffset (params, ModDest::Cutoff, 8000.0f));
    const float fRes  = juce::jlimit (0.05f, 1.5f, params.fRes->load() + modOffset (params, ModDest::Reso, 0.6f));
    const float fEnv  = *params.fEnv;
    const float fDrive = *params.fDrive;
    const float fKey   = *params.fKey;
    const float fVel   = *params.fVel;
    const int   fType  = rawChoice (params.fType);

    filterL.setType (static_cast<juce::dsp::StateVariableTPTFilterType> (fType));
    filterR.setType (static_cast<juce::dsp::StateVariableTPTFilterType> (fType));
    filterL.setResonance (fRes);
    filterR.setResonance (fRes);

    const int numCh = juce::jmin (2, outputBuffer.getNumChannels());
    auto* outL = outputBuffer.getWritePointer (0, startSample);
    auto* outR = numCh > 1 ? outputBuffer.getWritePointer (1, startSample) : outL;

    struct OP {
        bool on; int shape; float pos; float lin; float panL, panR;
        float ratio; int unison; float det;
        const Wavetable* table;
    };
    // Resolve per-osc wavetable pointer once per block. Custom shapes use the
    // processor-owned shared_ptr (loaded atomically); fall back to Basic when
    // no user wavetable has been loaded yet.
    std::shared_ptr<Wavetable> customSnap[3];
    OP op[3];
    for (int i = 0; i < 3; ++i)
    {
        op[i].on     = *params.osc[i].on > 0.5f;
        op[i].shape  = rawChoice (params.osc[i].shape);
        if (op[i].shape == WavetableLibrary::Custom)
        {
            customSnap[i] = std::atomic_load (&params.customTables[i]);
            op[i].table = customSnap[i] ? customSnap[i].get()
                                        : &lib.getTable (WavetableLibrary::Basic);
        }
        else
        {
            op[i].table = &lib.getTable (op[i].shape);
        }
        op[i].pos    = juce::jlimit (0.0f, 1.0f, params.osc[i].position->load()
                                      + modOffset (params, ModDest::Osc1Pos + i, 1.0f));
        const float lvDb = params.osc[i].level->load() + modOffset (params, ModDest::Osc1Lvl + i, 12.0f);
        op[i].lin    = juce::Decibels::decibelsToGain (lvDb);
        const float pan = juce::jlimit (-1.0f, 1.0f, params.osc[i].pan->load());
        op[i].panL   = std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
        op[i].panR   = std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
        op[i].ratio  = std::pow (2.0f, (params.osc[i].coarse->load() + params.osc[i].fine->load() * 0.01f) / 12.0f);
        op[i].unison = juce::jlimit (1, 7, (int) params.osc[i].unison->load());
        const float detv = juce::jlimit (0.0f, 1.0f, params.osc[i].detune->load() + modOffset (params, ModDest::Osc1Det + i, 1.0f));
        op[i].det    = detv * 0.04f;
    }

    const bool subOn = *params.subOn > 0.5f;
    const int  subShape = rawChoice (params.subShape);
    const int  subOct = (int) params.subOct->load();
    const float subLin = juce::Decibels::decibelsToGain (params.subLevel->load());

    const bool noiseOn = *params.noiseOn > 0.5f;
    const int  noiseColor = rawChoice (params.noiseColor);
    const float noiseLin = juce::Decibels::decibelsToGain (params.noiseLevel->load());

    // LFO increments: tempo-synced when lfoX_sync is on, otherwise free-running Hz.
    const double bpm = params.bpm.load();
    float lfo1Inc, lfo2Inc;
    if (*params.lfo1Sync > 0.5f)
    {
        const int    divIdx    = (int) (params.lfo1Div->load() + 0.5f);
        const double periodSec = syncDivToBeats (divIdx) * 60.0 / juce::jmax (20.0, bpm);
        lfo1Inc = 1.0f / (float) (periodSec * sr);
    }
    else
    {
        lfo1Inc = juce::jlimit (0.05f, 30.0f, params.lfo1Rate->load()
                                    + modOffset (params, ModDest::Lfo1Rate, 10.0f)) / (float) sr;
    }
    if (*params.lfo2Sync > 0.5f)
    {
        const int    divIdx    = (int) (params.lfo2Div->load() + 0.5f);
        const double periodSec = syncDivToBeats (divIdx) * 60.0 / juce::jmax (20.0, bpm);
        lfo2Inc = 1.0f / (float) (periodSec * sr);
    }
    else
    {
        lfo2Inc = juce::jlimit (0.05f, 30.0f, params.lfo2Rate->load()
                                    + modOffset (params, ModDest::Lfo2Rate, 10.0f)) / (float) sr;
    }
    const float lfo1Amt = *params.lfo1Amt;
    const float lfo2Amt = *params.lfo2Amt;
    const int   lfo1Shape = rawChoice (params.lfo1Shape);
    const int   lfo2Shape = rawChoice (params.lfo2Shape);

    // Velocity scaling
    const float aVelSense = *params.aVel;
    const float ampVelGain = (1.0f - aVelSense) + aVelSense * velocityNorm;

    for (int s = 0; s < numSamples; ++s)
    {
        // Glide
        baseFreqCurrent += (baseFreqTarget - baseFreqCurrent) * glideCoef;

        const float ampE = ampEnv.getNextSample();
        const float modE = modEnv.getNextSample();

        // Pitch env
        pEnvLevel *= pEnvDecayCoef;
        const float pitchEnvFactor = std::pow (2.0f, (pEnvLevel * pEnvAmtSemi) / 12.0f);

        // LFOs
        lfoPhase[0] += lfo1Inc; if (lfoPhase[0] >= 1.0f) lfoPhase[0] -= 1.0f;
        lfoPhase[1] += lfo2Inc; if (lfoPhase[1] >= 1.0f) lfoPhase[1] -= 1.0f;
        const float lfo1 = nextLfo (lfo1Shape, lfoPhase[0], shState[0], shTimer[0], lfo1Inc * 4.0f, rng);
        const float lfo2 = nextLfo (lfo2Shape, lfoPhase[1], shState[1], shTimer[1], lfo2Inc * 4.0f, rng);

        // Filter cutoff: env + lfo + key tracking + velocity
        const float keyTrack = (currentNote - 60) * fKey * 50.0f; // ~50 Hz/semitone scaled (multiplicative-ish below)
        float cutoffHz = fCutBase * std::pow (2.0f, fEnv * 4.0f * modE * (1.0f - fVel + fVel * velocityNorm)
                                                + lfo1Amt * lfo1 * 2.0f
                                                + keyTrack * 0.001f);
        cutoffHz = juce::jlimit (20.0f, (float) (sr * 0.45), cutoffHz);
        filterL.setCutoffFrequency (cutoffHz);
        filterR.setCutoffFrequency (cutoffHz);

        float sumL = 0.0f, sumR = 0.0f;

        for (int i = 0; i < 3; ++i)
        {
            if (! op[i].on) continue;
            const auto& wt = *op[i].table;

            osc[i].driftPhase += 0.07f / (float) sr;
            if (osc[i].driftPhase >= 1.0f) osc[i].driftPhase -= 1.0f;
            const float drift = std::sin (osc[i].driftPhase * juce::MathConstants<float>::twoPi) * driftAmt * 0.005f;

            const float baseHz = baseFreqCurrent * op[i].ratio * pBendFactor * pitchEnvFactor * (1.0f + drift);
            const float pos = juce::jlimit (0.0f, 1.0f, op[i].pos + lfo2Amt * lfo2 * 0.3f);

            const int U = op[i].unison;
            const float spread = op[i].det;
            float oL = 0.0f, oR = 0.0f;

            // Pre-pick mip from base hz to avoid per-sample mip changes mid-block
            const int mip = Wavetable::chooseMip (baseHz / (float) sr);

            for (int u = 0; u < U; ++u)
            {
                const float voiceN = (U == 1) ? 0.0f : ((float) u / (float) (U - 1) - 0.5f) * 2.0f;
                const float det = 1.0f + voiceN * spread;
                const float hz = baseHz * det;
                const float inc = hz / (float) sr;

                const float jitter = (rng.nextFloat() - 0.5f) * gritAmt * 0.002f;
                osc[i].uPhases[(size_t) u] += inc + jitter;
                while (osc[i].uPhases[(size_t) u] >= 1.0f) osc[i].uPhases[(size_t) u] -= 1.0f;
                while (osc[i].uPhases[(size_t) u] <  0.0f) osc[i].uPhases[(size_t) u] += 1.0f;

                const float v = wt.sample (pos, osc[i].uPhases[(size_t) u], mip);

                const float pan = (U == 1) ? 0.0f : voiceN;
                const float pl = std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
                const float pr = std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
                oL += v * pl;
                oR += v * pr;
            }

            const float invU = 1.0f / std::sqrt ((float) U);
            sumL += oL * invU * op[i].lin * op[i].panL;
            sumR += oR * invU * op[i].lin * op[i].panR;
        }

        // Sub osc
        if (subOn)
        {
            const float octFactor = subOct >= 0 ? std::pow (2.0f, (float) subOct) : std::pow (2.0f, (float) subOct);
            const float subHz = baseFreqCurrent * pBendFactor * pitchEnvFactor * octFactor;
            const float inc = subHz / (float) sr;
            subPhase += inc;
            if (subPhase >= 1.0f) subPhase -= 1.0f;
            float v = 0.0f;
            switch (subShape)
            {
                case SubShape::Sine:   v = std::sin (subPhase * juce::MathConstants<float>::twoPi); break;
                case SubShape::Square: v = subPhase < 0.5f ? 1.0f : -1.0f; break;
                case SubShape::Tri:    v = 4.0f * std::abs (subPhase - 0.5f) - 1.0f; break;
            }
            const float g = subLin * 0.7f;
            sumL += v * g; sumR += v * g;
        }

        // Noise osc
        if (noiseOn)
        {
            float white = rng.nextFloat() * 2.0f - 1.0f;
            float n = white;
            if (noiseColor == NoiseColor::Pink)
            {
                // Voss-McCartney approximation
                pinkB[0] = 0.99886f * pinkB[0] + white * 0.0555179f;
                pinkB[1] = 0.99332f * pinkB[1] + white * 0.0750759f;
                pinkB[2] = 0.96900f * pinkB[2] + white * 0.1538520f;
                pinkB[3] = 0.86650f * pinkB[3] + white * 0.3104856f;
                pinkB[4] = 0.55000f * pinkB[4] + white * 0.5329522f;
                pinkB[5] = -0.7616f * pinkB[5] - white * 0.0168980f;
                n = pinkB[0]+pinkB[1]+pinkB[2]+pinkB[3]+pinkB[4]+pinkB[5]+pinkB[6]+white*0.5362f;
                pinkB[6] = white * 0.115926f;
                n *= 0.11f;
            }
            else if (noiseColor == NoiseColor::Brown)
            {
                brownState = juce::jlimit (-1.0f, 1.0f, brownState + white * 0.02f);
                n = brownState * 3.5f;
            }
            sumL += n * noiseLin;
            sumR += (n * 0.7f + (rng.nextFloat() * 2.0f - 1.0f) * 0.05f) * noiseLin;
        }

        // Background "vibe" hiss
        if (vibeAmt > 0.0f)
        {
            const float n = (rng.nextFloat() - 0.5f) * vibeAmt * 0.05f;
            sumL += n; sumR += n * 0.7f;
        }

        // Filter drive (pre-filter saturation)
        if (fDrive > 0.0f)
        {
            const float d = 1.0f + fDrive * 5.0f;
            sumL = fastTanh (sumL * d) / std::sqrt (d);
            sumR = fastTanh (sumR * d) / std::sqrt (d);
        }

        // Filter
        sumL = filterL.processSample (0, sumL);
        sumR = filterR.processSample (0, sumR);

        // Saturation (analog warmth)
        if (satAmt > 0.0f)
        {
            const float drive = 1.0f + satAmt * 4.0f;
            sumL = fastTanh (sumL * drive) / drive * (1.0f + satAmt * 0.5f);
            sumR = fastTanh (sumR * drive) / drive * (1.0f + satAmt * 0.5f);
        }

        const float g = ampE * ampVelGain;
        outL[s] += sumL * g;
        outR[s] += sumR * g;

        if (! ampEnv.isActive())
        {
            clearCurrentNote();
            currentNote = -1;
            break;
        }
    }
}
