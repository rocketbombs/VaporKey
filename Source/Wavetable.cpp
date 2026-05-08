#include "Wavetable.h"

void Wavetable::buildMipsFromTimeDomain (Frame& out, const std::array<float, kFrameSize>& timeDomain)
{
    constexpr int N = kFrameSize;
    constexpr int order = 11; // 2^11 = 2048
    static_assert (1 << order == N, "kFrameSize must be 2^order");

    juce::dsp::FFT fft (order);

    // Real-only forward transform: buffer of size 2*N. Input in [0..N-1], rest zero.
    // Output: complex bins interleaved (real,imag) for k = 0..N-1, conjugate symmetric.
    std::vector<float> spectrum ((size_t) (N * 2), 0.0f);
    for (int n = 0; n < N; ++n) spectrum[(size_t) n] = timeDomain[(size_t) n];
    fft.performRealOnlyForwardTransform (spectrum.data());

    // For each mip, zero out bins above the harmonic cutoff and inverse transform.
    for (int m = 0; m < kNumMips; ++m)
    {
        const int maxHarm = juce::jmax (2, (N / 2) >> m);
        std::vector<float> work = spectrum;
        // Zero bins [maxHarm .. N/2] (and their conjugates handled by perform real-only inverse)
        for (int k = maxHarm; k <= N / 2; ++k)
        {
            work[(size_t) (2 * k)]     = 0.0f;
            work[(size_t) (2 * k + 1)] = 0.0f;
        }

        juce::dsp::FFT ifft (order);
        ifft.performRealOnlyInverseTransform (work.data());

        auto& wave = out.mips[(size_t) m];
        float peak = 0.0f;
        for (int n = 0; n < N; ++n)
        {
            wave[(size_t) n] = work[(size_t) n];
            peak = juce::jmax (peak, std::abs (wave[(size_t) n]));
        }
        // 1e-3 (~-60 dB) is below any audible content but well above 16-bit
        // quantisation residue. Without the higher floor a near-silent input
        // (e.g. a stereo wav whose channels cancel after the mono mix-down)
        // gets amplified to full scale.
        if (peak > 1.0e-3f)
            for (auto& v : wave) v *= 0.99f / peak;
    }
}

void Wavetable::buildFromMonoAudio (const float* samples, int numSamples)
{
    const int avail = juce::jmax (0, numSamples / kFrameSize);

    for (int f = 0; f < kNumFrames; ++f)
    {
        std::array<float, kFrameSize> base {};

        if (avail == 0)
        {
            const int n = juce::jmin (kFrameSize, numSamples);
            for (int i = 0; i < n; ++i) base[(size_t) i] = samples[i];
        }
        else
        {
            const int srcFrame = juce::jmin (f, avail - 1);
            const float* src = samples + srcFrame * kFrameSize;
            for (int i = 0; i < kFrameSize; ++i) base[(size_t) i] = src[i];
        }

        // Per-frame normalize to keep loud frames from dwarfing quiet ones.
        // Threshold matches buildMipsFromTimeDomain: anything quieter than
        // ~-60 dB is treated as silence and left untouched, so quantisation
        // residue from a phase-cancelling stereo wav does not get inflated
        // to full scale by the divide.
        float peak = 0.0f;
        for (float v : base) peak = juce::jmax (peak, std::abs (v));
        if (peak > 1.0e-3f)
            for (auto& v : base) v /= peak;

        buildMipsFromTimeDomain (frames[(size_t) f], base);
    }
}

WavetableLibrary& WavetableLibrary::get()
{
    static WavetableLibrary inst;
    return inst;
}

const char* WavetableLibrary::shapeName (int shape) noexcept
{
    switch (shape)
    {
        case Basic:    return "Basic";
        case Saws:     return "Saws";
        case Squares:  return "Squares";
        case Vocal:    return "Vocal";
        case Bell:     return "Bell";
        case Digital:  return "Digital";
        case Harmonic: return "Harmonic";
        case Glass:    return "Glass";
        case Reso:     return "Reso";
        case Custom:   return "Custom";
        default:       return "?";
    }
}

WavetableLibrary::WavetableLibrary()
{
    using Buf = std::array<float, Wavetable::kFrameSize>;
    constexpr int N = Wavetable::kFrameSize;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // Basic: sine -> triangle -> saw -> square as you sweep position.
    tables[Basic].build ([&](int f, Buf& b)
    {
        const float pos = (float) f / (float) (Wavetable::kNumFrames - 1);
        for (int n = 0; n < N; ++n)
        {
            const float t = (float) n / (float) N;
            const float sine  = std::sin (twoPi * t);
            const float tri   = 4.0f * std::abs (t - 0.5f) - 1.0f;
            const float saw   = 2.0f * t - 1.0f;
            const float sq    = (t < 0.5f ? 1.0f : -1.0f);
            // Crossfade across the four shapes
            float v;
            if (pos < 0.333f)      { float x = pos / 0.333f;             v = sine * (1 - x) + tri * x; }
            else if (pos < 0.666f) { float x = (pos - 0.333f) / 0.333f;  v = tri  * (1 - x) + saw * x; }
            else                   { float x = (pos - 0.666f) / 0.334f;  v = saw  * (1 - x) + sq  * x; }
            b[(size_t) n] = v;
        }
    });

    // Saws: super-saw cluster widens with position.
    tables[Saws].build ([&](int f, Buf& b)
    {
        const float spread = (float) f / (float) (Wavetable::kNumFrames - 1) * 0.04f;
        for (int n = 0; n < N; ++n)
        {
            float v = 0.0f;
            for (int k = -3; k <= 3; ++k)
            {
                const float det = 1.0f + spread * (float) k;
                float t = std::fmod ((float) n / (float) N * det, 1.0f);
                v += 2.0f * t - 1.0f;
            }
            b[(size_t) n] = v / 7.0f;
        }
    });

    // Squares: square -> PWM toward narrow pulse.
    tables[Squares].build ([&](int f, Buf& b)
    {
        const float duty = 0.5f - 0.4f * ((float) f / (float) (Wavetable::kNumFrames - 1));
        for (int n = 0; n < N; ++n)
        {
            const float t = (float) n / (float) N;
            b[(size_t) n] = (t < duty) ? 1.0f : -1.0f;
        }
    });

    // Vocal-ish: formant-shifted sums.
    tables[Vocal].build ([&](int f, Buf& b)
    {
        const float shift = 1.0f + (float) f * 0.25f;
        const float formants[3] = { 1.0f * shift, 2.6f * shift, 4.1f * shift };
        const float amps[3]     = { 1.0f, 0.6f, 0.35f };
        for (int n = 0; n < N; ++n)
        {
            const float t = twoPi * (float) n / (float) N;
            float v = 0.0f;
            for (int k = 1; k < 32; ++k)
            {
                float a = 1.0f / (float) k;
                float resp = 0.0f;
                for (int q = 0; q < 3; ++q)
                {
                    float d = (float) k - formants[q];
                    resp += amps[q] / (1.0f + d * d * 0.25f);
                }
                v += a * resp * std::sin (k * t);
            }
            b[(size_t) n] = v;
        }
    });

    // Bell: inharmonic partials, decaying with frame.
    tables[Bell].build ([&](int f, Buf& b)
    {
        const float decay = 1.0f - 0.6f * ((float) f / (float) (Wavetable::kNumFrames - 1));
        const float ratios[6] = { 1.0f, 2.76f, 5.4f, 8.93f, 13.34f, 18.64f };
        const float amps[6]   = { 1.0f, 0.7f, 0.55f, 0.4f, 0.3f, 0.2f };
        for (int n = 0; n < N; ++n)
        {
            const float t = twoPi * (float) n / (float) N;
            float v = 0.0f;
            for (int k = 0; k < 6; ++k)
                v += amps[k] * std::pow (decay, (float) k) * std::sin (ratios[k] * t);
            b[(size_t) n] = v;
        }
    });

    // Digital: FM-ish.
    tables[Digital].build ([&](int f, Buf& b)
    {
        const float idx = 0.5f + 3.0f * ((float) f / (float) (Wavetable::kNumFrames - 1));
        const float ratio = 2.0f;
        for (int n = 0; n < N; ++n)
        {
            const float t = twoPi * (float) n / (float) N;
            b[(size_t) n] = std::sin (t + idx * std::sin (ratio * t));
        }
    });

    // Harmonic: build by tilting the harmonic series amplitudes.
    tables[Harmonic].build ([&](int f, Buf& b)
    {
        const float tilt = -1.0f + 2.0f * ((float) f / (float) (Wavetable::kNumFrames - 1)); // -1 dark, +1 bright
        for (int n = 0; n < N; ++n)
        {
            const float t = twoPi * (float) n / (float) N;
            float v = 0.0f;
            for (int k = 1; k < 64; ++k)
                v += std::pow ((float) k, tilt) / (float) k * std::sin (k * t);
            b[(size_t) n] = v;
        }
    });

    // Glass: clustered partials.
    tables[Glass].build ([&](int f, Buf& b)
    {
        const float spread = 1.0f + (float) f * 0.18f;
        const float partials[6] = { 1.0f, 1.5f * spread, 2.0f * spread, 3.5f, 5.5f, 8.0f };
        const float amps[6]     = { 1.0f, 0.5f, 0.7f, 0.3f, 0.25f, 0.15f };
        for (int n = 0; n < N; ++n)
        {
            const float t = twoPi * (float) n / (float) N;
            float v = 0.0f;
            for (int k = 0; k < 6; ++k)
                v += amps[k] * std::sin (partials[k] * t);
            b[(size_t) n] = v;
        }
    });

    // Reso: emphasized formant near varying frequency.
    tables[Reso].build ([&](int f, Buf& b)
    {
        const float center = 3.0f + 12.0f * ((float) f / (float) (Wavetable::kNumFrames - 1));
        const float q = 1.5f;
        for (int n = 0; n < N; ++n)
        {
            const float t = twoPi * (float) n / (float) N;
            float v = 0.0f;
            for (int k = 1; k < 64; ++k)
            {
                const float dx = ((float) k - center) / q;
                const float resp = 1.0f / (1.0f + dx * dx);
                v += (resp * 1.5f / (float) k) * std::sin (k * t);
            }
            b[(size_t) n] = v;
        }
    });
}
