#include "SynthVoice.h"
#include "Parameters.h"

WTVoice::WTVoice (SynthParams& p) : params (p)
{
    // Default-constructed juce::Random uses seed = 1 for every voice in every
    // plugin instance. Without this, all 16 voices fire identical noise / grit
    // / drift jitter and stack coherently (16x amplitude, hard-edged, sounds
    // aliased) instead of decorrelating into a smooth stochastic signal.
    rng.setSeedRandomly();
}

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

// Antiderivative of fastTanh:
//   f(x) = x*(27 + x^2)/(27 + 9*x^2) = 24x/(27 + 9x^2) + x/9
//   F(x) = (4/3) * ln(27 + 9*x^2) + x^2/18
// Constant of integration omitted (ADAA only uses F(a) - F(b)).
inline float WTVoice::fastTanhAntideriv (float x) noexcept
{
    const float x2 = x * x;
    return x2 * (1.0f / 18.0f)
         + (4.0f / 3.0f) * std::log (27.0f + 9.0f * x2);
}

// First-order Antiderivative Anti-Aliasing (Bilbao / Parker / Esqueda) for
// fastTanh. With a smooth nonlinearity f(x) and antiderivative F(x):
//   y[n] = (F(x[n]) - F(x[n-1])) / (x[n] - x[n-1])
// gives the average of f over the input interval, which is the
// continuous-time bandlimited output if x is treated as a piecewise-linear
// reconstruction. Per-sample cost: one log + a handful of FLOPs (vs. the
// MAD-only fastTanh) - cheap enough to run at the per-voice sample rate.
//
// When |x[n] - x[n-1]| is below epsilon the formula is 0/0; we fall back
// to f((x[n] + x[n-1]) / 2), the analytical limit (L'Hopital).
inline float WTVoice::fastTanhADAA (float x, float& xPrev) noexcept
{
    constexpr float kEps = 1.0e-5f;
    const float dx = x - xPrev;
    float y;
    if (std::abs (dx) > kEps)
    {
        y = (fastTanhAntideriv (x) - fastTanhAntideriv (xPrev)) / dx;
    }
    else
    {
        const float xm  = 0.5f * (x + xPrev);
        const float xm2 = xm * xm;
        y = xm * (27.0f + xm2) / (27.0f + 9.0f * xm2);
    }
    xPrev = x;
    return y;
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
    const bool legatoTransition = legatoSkipEnvRetrigger;
    legatoSkipEnvRetrigger = false;

    currentNote = midiNote;
    baseFreqTarget = (float) juce::MidiMessage::getMidiNoteInHertz (midiNote);
    if (baseFreqCurrent <= 0.0f) baseFreqCurrent = baseFreqTarget;

    velocityNorm = velocity;

    // For a legato transition we keep the existing envelope state so the note
    // glides smoothly without re-attacking. ADSR parameters still get refreshed
    // in case the patch changed mid-phrase.
    juce::ADSR::Parameters ampP { *params.aA, *params.aD, *params.aS, *params.aR };
    juce::ADSR::Parameters modP { *params.mA, *params.mD, *params.mS, *params.mR };
    ampEnv.setParameters (ampP);
    modEnv.setParameters (modP);

    if (! legatoTransition)
    {
        ampEnv.noteOn();
        modEnv.noteOn();

        // Pitch env: re-init level to 1.0; decay coef set to reach ~1% in pEnvDecay.
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

    if (! legatoTransition)
    {
        // Start phases - skipped on legato so the oscillators keep running
        // continuously through the pitch glide.
        for (int i = 0; i < 3; ++i)
        {
            const float ph = juce::jlimit (-1.0f, 1.0f, params.osc[i].phase->load());
            for (auto& u : osc[i].uPhases) u = (ph < 0.0f) ? rng.nextFloat() : ph;
            osc[i].driftPhase = rng.nextFloat();
        }
        subPhase = 0.0f;
        for (auto& v : pinkBL) v = 0.0f;
        for (auto& v : pinkBR) v = 0.0f;
        brownStateL = brownStateR = 0.0f;

        for (auto& v : oscModPrev) v = 0.0f;

        filterL.reset(); filterR.reset();
    }
}

void WTVoice::stopNote (float, bool allowTailOff)
{
    if (legatoArmed)
    {
        // Mono+legato hand-off: SynthEngine emitted a noteOff/noteOn pair at
        // the same sample position to glide on the same voice. JUCE doesn't
        // know about that, so it would route the upcoming noteOn to a fresh
        // idle voice (since this one is still in sustain) and the legato
        // flag would be wasted. We free this voice from JUCE's perspective
        // here - clearCurrentNote drops currentlyPlayingNote so findFreeVoice
        // returns this voice for the immediately-following noteOn - while
        // leaving every audio-relevant member (envelopes, oscillator phases,
        // filter, noise state) untouched. The legato-skip flag is set so
        // the matching startNote keeps the existing envelope state running.
        clearCurrentNote();
        legatoArmed = false;
        legatoSkipEnvRetrigger = true;
        return;
    }

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

    // Real mono code path: when the host bus is mono we sum the per-voice L/R
    // pair down to a single channel and write only outL. Aliasing outR to outL
    // and writing both would double the per-sample contribution and route
    // every osc/noise pan position through the same buffer, which read as
    // "louder + harsher" on a mono bus.
    const int numCh = juce::jmin (2, outputBuffer.getNumChannels());
    const bool isMono = (numCh == 1);
    auto* outL = outputBuffer.getWritePointer (0, startSample);
    auto* outR = isMono ? nullptr : outputBuffer.getWritePointer (1, startSample);

    struct OP {
        bool on; int shape; float pos; float lin; float panL, panR;
        float ratio; int unison; float det;
        float invSqrtU;
        std::array<float, 7> uPanL, uPanR;
        const Wavetable* table;

        // Cross-modulation routing, resolved once per block. modSrc < 0 means
        // no modulation (off, or self-routed - which we silently ignore to
        // prevent feedback). For FM, modAmt is pre-scaled to a phase-offset
        // depth in cycles. For Ring/AM, it's the dry/wet mix amount.
        int   modSrc = -1;
        int   modType = OscModType::FM;
        float modAmt = 0.0f;
        float fmDepth = 0.0f;     // phase-offset cycles when modType == FM
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

        // Pre-compute equal-power pan + detune ratios for each unison voice
        // once per block instead of recomputing trig per sample inside the
        // inner unison loop (the hottest path in the synth).
        op[i].invSqrtU = 1.0f / std::sqrt ((float) op[i].unison);
        for (int u = 0; u < op[i].unison; ++u)
        {
            const float voiceN = (op[i].unison == 1) ? 0.0f
                : ((float) u / (float) (op[i].unison - 1) - 0.5f) * 2.0f;
            const float pp = (voiceN + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            op[i].uPanL[(size_t) u] = std::cos (pp);
            op[i].uPanR[(size_t) u] = std::sin (pp);
        }

        // Resolve cross-modulation routing. The source enum is offset by 1
        // (Off = 0, Osc1 = 1...), so subtract 1 to get a 0-based osc index.
        // Self-routing is treated as Off to avoid 1-sample feedback loops.
        const int rawSrc = rawChoice (params.osc[i].modSrc);
        if (rawSrc > 0 && (rawSrc - 1) != i)
        {
            op[i].modSrc  = rawSrc - 1;
            op[i].modType = rawChoice (params.osc[i].modType);
            op[i].modAmt  = juce::jlimit (0.0f, 1.0f, params.osc[i].modAmt->load());
            // FM depth: max ±2 cycles of phase offset at full amount. That's
            // generous enough for chunky DX-style FM without being so deep
            // that the wavetable mip choice (computed from the carrier rate)
            // becomes wildly wrong.
            op[i].fmDepth = op[i].modAmt * 2.0f;
        }
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

        // Latched per-osc mono output for THIS sample, captured pre-level /
        // pre-pan so cross-modulation depth doesn't depend on the source's
        // mix gain or stereo placement. Written here, propagated into
        // oscModPrev at the bottom of the sample loop so the next sample's
        // destinations can read it.
        float oscModNow[3] { 0.0f, 0.0f, 0.0f };

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
            float oM = 0.0f;   // mono unison sum, used as modulator source

            // Pre-pick mip from base hz to avoid per-sample mip changes mid-block.
            // For FM we bump the mip up by an octave or two depending on depth -
            // FM sidebands extend the carrier's spectrum, and sampling from a
            // mip chosen for the base rate alone would alias hard. The bump
            // costs a touch of brightness in exchange for clean output across
            // the full modulation range.
            int mip = Wavetable::chooseMip (baseHz / (float) sr);

            // Cross-osc modulation inputs (1-sample-delayed source value).
            const bool  hasFM    = (op[i].modSrc >= 0) && (op[i].modType == OscModType::FM);
            const float modVal   = (op[i].modSrc >= 0) ? oscModPrev[op[i].modSrc] : 0.0f;
            const float fmOffset = hasFM ? modVal * op[i].fmDepth : 0.0f;
            if (hasFM)
            {
                // One octave for any FM, two at full depth (when sidebands
                // extend furthest). Anything finer-grained gets lost in the
                // 10-tap mip ladder.
                const int bump = (op[i].modAmt >= 0.7f) ? 2 : 1;
                mip = juce::jmin (Wavetable::kNumMips - 1, mip + bump);
            }

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

                // For FM, sample the wavetable at phase + modulator offset.
                // The accumulator itself only advances at the carrier rate -
                // PM-style, which is mathematically equivalent to FM for
                // band-limited modulators and lets us keep a stable phase
                // baseline across blocks.
                float ph = osc[i].uPhases[(size_t) u];
                if (hasFM)
                {
                    ph += fmOffset;
                    ph -= std::floor (ph);
                }

                const float v = wt.sample (pos, ph, mip);
                oM += v;
                oL += v * op[i].uPanL[(size_t) u];
                oR += v * op[i].uPanR[(size_t) u];
            }

            const float invU = op[i].invSqrtU;
            float oscL = oL * invU * op[i].lin;
            float oscR = oR * invU * op[i].lin;

            // Ring / AM are post-mix multiplicative effects on the carrier.
            //   Ring: classic four-quadrant multiply; mix = dry + amt * (dry*mod - dry).
            //   AM:   unipolar modulator (mod*0.5 + 0.5) shaping the carrier
            //         amplitude, mixed against dry by amt.
            // Both collapse to the dry signal at amt = 0, so a non-zero
            // modSrc with amt = 0 still sounds identical to bypass.
            if (op[i].modSrc >= 0 && op[i].modType != OscModType::FM)
            {
                const float wetGain = (op[i].modType == OscModType::Ring)
                                          ? modVal
                                          : (modVal * 0.5f + 0.5f);
                const float scale = 1.0f + op[i].modAmt * (wetGain - 1.0f);
                oscL *= scale;
                oscR *= scale;
            }

            sumL += oscL * op[i].panL;
            sumR += oscR * op[i].panR;

            // Mono unison sum, normalized like the L/R contributions, makes
            // a clean modulator signal in roughly [-1, +1] regardless of
            // the source osc's level and pan.
            oscModNow[i] = oM * invU;
        }

        for (int i = 0; i < 3; ++i) oscModPrev[i] = oscModNow[i];

        // Sub osc
        if (subOn)
        {
            const float octFactor = std::pow (2.0f, (float) subOct);
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

        // Noise osc - independent L/R chains keep the spectral character
        // (pink/brown) on both channels while still being decorrelated, instead
        // of the previous mono-pink + raw-white mix that injected high-frequency
        // hiss only on the right channel and read as aliasing.
        if (noiseOn)
        {
            const float wL = rng.nextFloat() * 2.0f - 1.0f;
            const float wR = rng.nextFloat() * 2.0f - 1.0f;
            float nL = wL, nR = wR;

            if (noiseColor == NoiseColor::Pink)
            {
                auto pinkStep = [] (float* b, float w) -> float
                {
                    // Voss-McCartney approximation.
                    b[0] = 0.99886f * b[0] + w * 0.0555179f;
                    b[1] = 0.99332f * b[1] + w * 0.0750759f;
                    b[2] = 0.96900f * b[2] + w * 0.1538520f;
                    b[3] = 0.86650f * b[3] + w * 0.3104856f;
                    b[4] = 0.55000f * b[4] + w * 0.5329522f;
                    b[5] = -0.7616f * b[5] - w * 0.0168980f;
                    const float out = b[0]+b[1]+b[2]+b[3]+b[4]+b[5]+b[6]+w*0.5362f;
                    b[6] = w * 0.115926f;
                    return out * 0.11f;
                };
                nL = pinkStep (pinkBL, wL);
                nR = pinkStep (pinkBR, wR);
            }
            else if (noiseColor == NoiseColor::Brown)
            {
                // Leaky integrator (Brownian motion + slow self-discharge) -
                // the previous random-walk-with-hard-clip drifted to the
                // rails and produced DC offset and clicks. The 0.999 leak
                // keeps DC bounded; the gain is calibrated so peak ~= 1.0.
                brownStateL = brownStateL * 0.999f + wL * 0.02f;
                brownStateR = brownStateR * 0.999f + wR * 0.02f;
                nL = brownStateL * 3.5f;
                nR = brownStateR * 3.5f;
            }

            sumL += nL * noiseLin;
            sumR += nR * noiseLin;
        }

        // Background "vibe" hiss - independent L/R samples and a per-voice
        // RNG (decorrelated by setSeedRandomly above) so chords no longer
        // produce a 16x-coherent hiss. The signal is shaped by the per-voice
        // filter just below, so it inherits the patch's tonality.
        if (vibeAmt > 0.0f)
        {
            const float wL = (rng.nextFloat() - 0.5f) * 2.0f;
            const float wR = (rng.nextFloat() - 0.5f) * 2.0f;
            const float g = vibeAmt * 0.025f;
            sumL += wL * g;
            sumR += wR * g;
        }

        // Filter drive (pre-filter saturation). ADAA-anti-aliased: the
        // tanh-shaped clipper generates harmonics that would otherwise
        // alias hard at modest drive levels with anything brighter than a
        // sine.
        if (fDrive > 0.0f)
        {
            const float d         = 1.0f + fDrive * 5.0f;
            const float invSqrtD  = 1.0f / std::sqrt (d);
            sumL = fastTanhADAA (sumL * d, adaaFiltDriveL) * invSqrtD;
            sumR = fastTanhADAA (sumR * d, adaaFiltDriveR) * invSqrtD;
        }

        // Filter
        sumL = filterL.processSample (0, sumL);
        sumR = filterR.processSample (0, sumR);

        // Saturation (analog warmth). Same ADAA treatment as the filter
        // drive - the post-filter signal has already been (mostly) tamed,
        // but heavy sat values still benefit.
        if (satAmt > 0.0f)
        {
            const float drive   = 1.0f + satAmt * 4.0f;
            const float scaleOut = (1.0f + satAmt * 0.5f) / drive;
            sumL = fastTanhADAA (sumL * drive, adaaSatL) * scaleOut;
            sumR = fastTanhADAA (sumR * drive, adaaSatR) * scaleOut;
        }

        const float g = ampE * ampVelGain;
        if (isMono)
        {
            outL[s] += (sumL + sumR) * 0.5f * g;
        }
        else
        {
            outL[s] += sumL * g;
            outR[s] += sumR * g;
        }

        if (! ampEnv.isActive())
        {
            clearCurrentNote();
            currentNote = -1;
            break;
        }
    }
}
