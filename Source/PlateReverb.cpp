#include "PlateReverb.h"

namespace
{
    // Canonical Dattorro delay lengths in samples at 29761 Hz. Scaled to the
    // host sample rate at prepare() time.
    constexpr double kProtoSR = 29761.0;

    // Input diffusion all-passes (Dattorro: g1=0.75 for the first pair,
    // g2=0.625 for the second).
    constexpr int kInDiff1Len = 142;
    constexpr int kInDiff2Len = 107;
    constexpr int kInDiff3Len = 379;
    constexpr int kInDiff4Len = 277;

    // Tank, left half: modulated AP -> long delay -> damping LP -> fixed AP
    // -> long delay -> cross-couple to right.
    constexpr int kTankAP1LLen = 672;     // modulated AP base length
    constexpr int kTankD1LLen  = 4453;
    constexpr int kTankAP2LLen = 1800;
    constexpr int kTankD2LLen  = 3720;

    // Tank, right half (asymmetric to break stereo correlation).
    constexpr int kTankAP1RLen = 908;
    constexpr int kTankD1RLen  = 4217;
    constexpr int kTankAP2RLen = 2656;
    constexpr int kTankD2RLen  = 3163;

    // Stereo output taps (positions inside the tank delay lines, in samples
    // at 29761 Hz). Reading/summing from these points produces the classic
    // Dattorro stereo image.
    //
    // Left output:
    //   + d1R[266] + d1R[2974] - ap2R[1913] + d2R[1996]
    //   - d1L[1990] - ap2L[187] - d2L[1066]
    constexpr int kTapL_d1R_a = 266;
    constexpr int kTapL_d1R_b = 2974;
    constexpr int kTapL_ap2R  = 1913;
    constexpr int kTapL_d2R   = 1996;
    constexpr int kTapL_d1L   = 1990;
    constexpr int kTapL_ap2L  = 187;
    constexpr int kTapL_d2L   = 1066;

    // Right output:
    //   + d1L[353] + d1L[3627] - ap2L[1228] + d2L[2673]
    //   - d1R[2111] - ap2R[335] - d2R[121]
    constexpr int kTapR_d1L_a = 353;
    constexpr int kTapR_d1L_b = 3627;
    constexpr int kTapR_ap2L  = 1228;
    constexpr int kTapR_d2L   = 2673;
    constexpr int kTapR_d1R   = 2111;
    constexpr int kTapR_ap2R  = 335;
    constexpr int kTapR_d2R   = 121;

    inline int scale (int protoSamples, double sampleRate) noexcept
    {
        return juce::jmax (1, (int) std::round ((double) protoSamples * sampleRate / kProtoSR));
    }
}

void PlateReverb::prepare (double sampleRate)
{
    sr = sampleRate;

    // Pre-delay: small, just enough to delay the first reflections.
    preDelayLen = scale (480, sampleRate); // ~16 ms at 29761 Hz proto
    preDelay.resize (preDelayLen + 8);

    // Input diffusion stages.
    inDiff1.resize (scale (kInDiff1Len, sampleRate), 0.75f);
    inDiff2.resize (scale (kInDiff2Len, sampleRate), 0.75f);
    inDiff3.resize (scale (kInDiff3Len, sampleRate), 0.625f);
    inDiff4.resize (scale (kInDiff4Len, sampleRate), 0.625f);

    // Tank, left half.
    {
        const int baseLen  = scale (kTankAP1LLen, sampleRate);
        const int modDepth = juce::jmax (1, scale (8, sampleRate));
        // Mod rate at ~1 Hz - slow, breaks flutter echoes without causing
        // audible warble.
        tankAP1L.prepare (baseLen + modDepth + 4,
                          (float) baseLen, (float) modDepth,
                          -0.7f, sampleRate, 0.95);
    }
    tankD1LLen = scale (kTankD1LLen, sampleRate);
    tankD1L.resize (tankD1LLen + 8);

    tankAP2L.resize (scale (kTankAP2LLen, sampleRate), 0.5f);

    tankD2LLen = scale (kTankD2LLen, sampleRate);
    tankD2L.resize (tankD2LLen + 8);

    // Tank, right half.
    {
        const int baseLen  = scale (kTankAP1RLen, sampleRate);
        const int modDepth = juce::jmax (1, scale (8, sampleRate));
        tankAP1R.prepare (baseLen + modDepth + 4,
                          (float) baseLen, (float) modDepth,
                          -0.7f, sampleRate, 1.05);
    }
    tankD1RLen = scale (kTankD1RLen, sampleRate);
    tankD1R.resize (tankD1RLen + 8);

    tankAP2R.resize (scale (kTankAP2RLen, sampleRate), 0.5f);

    tankD2RLen = scale (kTankD2RLen, sampleRate);
    tankD2R.resize (tankD2RLen + 8);

    // Cache scaled tap offsets.
    tap_d1R_a  = juce::jmin (tankD1RLen,  scale (kTapL_d1R_a, sampleRate));
    tap_d1R_b  = juce::jmin (tankD1RLen,  scale (kTapL_d1R_b, sampleRate));
    tap_ap2R   = juce::jmin (tankAP2R.length, scale (kTapL_ap2R,  sampleRate));
    tap_d2R_a  = juce::jmin (tankD2RLen,  scale (kTapL_d2R, sampleRate));
    tap_d1L_a  = juce::jmin (tankD1LLen,  scale (kTapL_d1L, sampleRate));
    tap_ap2L   = juce::jmin (tankAP2L.length, scale (kTapL_ap2L,  sampleRate));
    tap_d2L_a  = juce::jmin (tankD2LLen,  scale (kTapL_d2L, sampleRate));

    tap_d1L_b  = juce::jmin (tankD1LLen,  scale (kTapR_d1L_a, sampleRate));
    tap_d1L_c  = juce::jmin (tankD1LLen,  scale (kTapR_d1L_b, sampleRate));
    tap_ap2L_b = juce::jmin (tankAP2L.length, scale (kTapR_ap2L, sampleRate));
    tap_d2L_b  = juce::jmin (tankD2LLen,  scale (kTapR_d2L, sampleRate));
    tap_d1R_c  = juce::jmin (tankD1RLen,  scale (kTapR_d1R, sampleRate));
    tap_ap2R_b = juce::jmin (tankAP2R.length, scale (kTapR_ap2R, sampleRate));
    tap_d2R_b  = juce::jmin (tankD2RLen,  scale (kTapR_d2R, sampleRate));

    reset();
}

void PlateReverb::reset()
{
    preDelay.clear();
    bwState = 0.0f;

    inDiff1.clear(); inDiff2.clear(); inDiff3.clear(); inDiff4.clear();

    tankAP1L.clear(); tankD1L.clear(); tankAP2L.clear(); tankD2L.clear();
    tankAP1R.clear(); tankD1R.clear(); tankAP2R.clear(); tankD2R.clear();

    dampStateL = 0.0f;
    dampStateR = 0.0f;
    feedbackL  = 0.0f;
    feedbackR  = 0.0f;
}

void PlateReverb::setSize (float size01) noexcept
{
    // Map 0..1 to a tank decay coefficient. The lower bound stays well above
    // 0 so even "small" still produces a tail; the upper bound stays below 1
    // so the tank can't self-oscillate.
    decay = juce::jlimit (0.4f, 0.86f,
                          0.4f + juce::jlimit (0.0f, 1.0f, size01) * 0.46f);
}

void PlateReverb::setDamping (float damp01) noexcept
{
    // Map 0..1 to a one-pole LP coefficient. Capped below 1 so the tank
    // doesn't freeze (output = state forever).
    damping = juce::jlimit (0.0f, 1.0f, damp01) * 0.92f;
}

void PlateReverb::setBandwidth (float bandwidth01) noexcept
{
    // Higher bandwidth01 = tighter low-pass at the input. Default ~0.0005
    // (essentially full bandwidth) keeps things bright unless asked.
    bandwidth = juce::jlimit (0.0f, 0.95f, bandwidth01);
}

void PlateReverb::process (float inL, float inR, float& outL, float& outR) noexcept
{
    // Sum to mono - plates are inherently mono-input.
    const float monoIn = 0.5f * (inL + inR);

    // Pre-delay then input bandwidth low-pass.
    preDelay.write (monoIn);
    const float pre = preDelay.read (preDelayLen);

    bwState = (1.0f - bandwidth) * pre + bandwidth * bwState;
    const float bw = bwState;

    // Input diffusion (4 stages).
    float diffused = inDiff1.process (bw);
    diffused = inDiff2.process (diffused);
    diffused = inDiff3.process (diffused);
    diffused = inDiff4.process (diffused);

    // ==== Tank, left half ====
    // Cross-coupled: take the OPPOSITE half's stored exit value.
    float xL = diffused + feedbackR;
    float v1L = tankAP1L.process (xL);
    tankD1L.write (v1L);
    float d1L = tankD1L.read (tankD1LLen);

    // Damping LP (one-pole)
    dampStateL = (1.0f - damping) * d1L + damping * dampStateL;
    const float dampedL = dampStateL;

    // Decay scale, fixed all-pass, second long delay.
    const float decayedL = decay * dampedL;
    const float v2L = tankAP2L.process (decayedL);
    tankD2L.write (v2L);
    const float d2L = tankD2L.read (tankD2LLen);

    // ==== Tank, right half ====
    float xR = diffused + feedbackL;
    float v1R = tankAP1R.process (xR);
    tankD1R.write (v1R);
    float d1R = tankD1R.read (tankD1RLen);

    dampStateR = (1.0f - damping) * d1R + damping * dampStateR;
    const float dampedR = dampStateR;

    const float decayedR = decay * dampedR;
    const float v2R = tankAP2R.process (decayedR);
    tankD2R.write (v2R);
    const float d2R = tankD2R.read (tankD2RLen);

    // Update cross-couple feedback for next sample. Using the second-delay
    // output (the "exit" of each half) creates the classic Dattorro
    // figure-eight loop. Decay is applied at the cross-feedback (rather
    // than inside each half) - this keeps the input diffusers contributing
    // at unity into the loop's first iteration.
    feedbackL = decay * d2L;
    feedbackR = decay * d2R;

    // ==== Output taps ====
    // Read fixed positions inside both halves' delay lines and the second
    // (fixed) all-passes. The plus/minus pattern is from Dattorro's paper -
    // it produces a wide stereo image without any explicit width control.
    const float yL =  tankD1R.read  (tap_d1R_a)
                    + tankD1R.read  (tap_d1R_b)
                    - tankAP2R.delay.read (tap_ap2R)
                    + tankD2R.read  (tap_d2R_a)
                    - tankD1L.read  (tap_d1L_a)
                    - tankAP2L.delay.read (tap_ap2L)
                    - tankD2L.read  (tap_d2L_a);

    const float yR =  tankD1L.read  (tap_d1L_b)
                    + tankD1L.read  (tap_d1L_c)
                    - tankAP2L.delay.read (tap_ap2L_b)
                    + tankD2L.read  (tap_d2L_b)
                    - tankD1R.read  (tap_d1R_c)
                    - tankAP2R.delay.read (tap_ap2R_b)
                    - tankD2R.read  (tap_d2R_b);

    // The 7-tap sum has roughly unity peak per tap at the equivalent sample
    // rate; scale to keep the wet signal in a sensible range.
    constexpr float kTapGain = 0.6f;
    outL = yL * kTapGain;
    outR = yR * kTapGain;
}
