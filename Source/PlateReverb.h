#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

// Dattorro 1997 plate reverb. Reference:
//   Jon Dattorro, "Effect Design Part 1: Reverberator and Other Filters",
//   Journal of the Audio Engineering Society, Vol. 45, No. 9, Sept 1997.
//
// Topology: pre-delay -> input bandwidth filter -> 4 cascaded all-pass
// diffusers -> figure-8 tank with two halves cross-coupled. Each tank half
// runs (modulated all-pass -> long delay -> damping low-pass -> decay scale
// -> fixed all-pass -> long delay) and feeds the opposite half's input on
// the next sample. Stereo output taps from fixed positions inside the
// tank's delay lines, summing/subtracting to produce the characteristic
// Dattorro stereo image without any explicit width control.
//
// All canonical delay lengths are quoted at 29761 Hz (Dattorro's prototype
// rate); we scale them to the host sample rate at prepare() time.
class PlateReverb
{
public:
    void prepare (double sampleRate);
    void reset();

    // size01: 0 = small / short tail, 1 = large / long tail. Internally maps
    //   to the Dattorro tank decay coefficient.
    // damp01: 0 = bright (no high-frequency damping), 1 = dark.
    // bandwidth01: input low-pass before the diffusers (0 = full bandwidth,
    //   1 = aggressive low-pass). Defaults to a sensible value if unset.
    void setSize (float size01) noexcept;
    void setDamping (float damp01) noexcept;
    void setBandwidth (float bandwidth01) noexcept;

    // Process one stereo sample. Input is summed to mono internally (plates
    // are inherently mono in / stereo out); output is stereo from tank taps.
    void process (float inL, float inR, float& outL, float& outR) noexcept;

private:
    // Power-of-two sized circular buffer with read-by-delay-samples helpers.
    struct Delay
    {
        std::vector<float> buffer;
        int                mask = 0;
        int                writeIdx = 0;

        void resize (int maxLength)
        {
            int sz = 1;
            while (sz < maxLength + 4) sz <<= 1;
            buffer.assign ((size_t) sz, 0.0f);
            mask = sz - 1;
            writeIdx = 0;
        }
        void clear() noexcept
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            writeIdx = 0;
        }
        void write (float x) noexcept
        {
            buffer[(size_t) writeIdx] = x;
            writeIdx = (writeIdx + 1) & mask;
        }
        float read (int delaySamples) const noexcept
        {
            const int idx = (writeIdx - delaySamples) & mask;
            return buffer[(size_t) idx];
        }
        // Linearly-interpolated read for modulated all-passes.
        float readLerp (float delaySamples) const noexcept
        {
            const int   d0 = (int) delaySamples;
            const float fr = delaySamples - (float) d0;
            const int   i0 = (writeIdx - d0)     & mask;
            const int   i1 = (writeIdx - d0 - 1) & mask;
            return buffer[(size_t) i0] + fr * (buffer[(size_t) i1] - buffer[(size_t) i0]);
        }
    };

    // Schroeder all-pass in direct form II:
    //   xh = x - g * z^-N(xh)
    //   y  = z^-N(xh) + g * xh
    struct Allpass
    {
        Delay delay;
        int   length = 0;
        float g = 0.0f;

        void resize (int len, float gain)
        {
            delay.resize (len + 4);
            length = len;
            g = gain;
        }
        void clear() noexcept { delay.clear(); }
        float process (float x) noexcept
        {
            const float dl = delay.read (length);
            const float xh = x - g * dl;
            delay.write (xh);
            return dl + g * xh;
        }
    };

    // Modulated all-pass: read position oscillates ±modDepth around baseLen.
    // The modulation breaks up flutter echoes inside the tank.
    struct ModAllpass
    {
        Delay delay;
        float baseLen  = 0.0f;
        float modDepth = 0.0f;
        float g        = 0.0f;
        float phase    = 0.0f;
        float inc      = 0.0f;

        void prepare (int maxLen, float baseLen_, float modDepth_, float gain,
                      double sampleRate, double rateHz)
        {
            delay.resize (maxLen + 8);
            baseLen  = baseLen_;
            modDepth = modDepth_;
            g        = gain;
            inc      = (float) (rateHz / sampleRate);
            phase    = 0.0f;
        }
        void clear() noexcept { delay.clear(); phase = 0.0f; }
        float process (float x) noexcept
        {
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
            const float lfo = std::sin (phase * juce::MathConstants<float>::twoPi);
            const float len = baseLen + modDepth * lfo;
            const float dl  = delay.readLerp (len);
            const float xh  = x - g * dl;
            delay.write (xh);
            return dl + g * xh;
        }
    };

    double sr = 44100.0;

    // Pre-delay (configurable; we keep it small - tail is in the tank).
    Delay preDelay;
    int   preDelayLen = 0;

    // Input bandwidth one-pole low-pass:
    //   y = (1 - bw) * x + bw * y_prev
    // bw in [0, 1); higher = tighter low-pass.
    float bandwidth = 0.0005f;
    float bwState   = 0.0f;

    // Input diffusion (Dattorro's 4-stage cascade).
    Allpass inDiff1, inDiff2, inDiff3, inDiff4;

    // Tank, two halves. Each contains a modulated all-pass, a long delay,
    // a damping low-pass, a fixed all-pass, and a second long delay.
    ModAllpass tankAP1L;
    Delay      tankD1L;
    int        tankD1LLen = 0;
    Allpass    tankAP2L;
    Delay      tankD2L;
    int        tankD2LLen = 0;
    float      dampStateL = 0.0f;

    ModAllpass tankAP1R;
    Delay      tankD1R;
    int        tankD1RLen = 0;
    Allpass    tankAP2R;
    Delay      tankD2R;
    int        tankD2RLen = 0;
    float      dampStateR = 0.0f;

    // Cross-coupled feedback: end-of-half exit value of the OPPOSITE half on
    // the previous sample.
    float feedbackL = 0.0f;
    float feedbackR = 0.0f;

    // Tank parameters
    float decay   = 0.5f;  // controls tail length
    float damping = 0.0f;  // damping LP coefficient (0 = bright)

    // Pre-computed taps (sample positions inside delay lines, scaled to sr).
    int tap_d1R_a = 0, tap_d1R_b = 0;
    int tap_ap2R  = 0;
    int tap_d2R_a = 0;
    int tap_d1L_a = 0;
    int tap_ap2L  = 0;
    int tap_d2L_a = 0;

    int tap_d1L_b = 0, tap_d1L_c = 0;
    int tap_ap2L_b = 0;
    int tap_d2L_b = 0;
    int tap_d1R_c = 0;
    int tap_ap2R_b = 0;
    int tap_d2R_b = 0;
};
