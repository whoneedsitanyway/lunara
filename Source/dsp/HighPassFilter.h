#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace lunara
{

//==============================================================================
//  Stereo 2-pole (12 dB/oct) high-pass filter, implemented as a TPT
//  state-variable filter (Zavalishin).  Used as a low-cut on the output so big
//  reverb / delay tails never turn the low end into mud.
//
//  Butterworth Q (no resonant boost), so the passband stays flat and the filter
//  can never exceed the input level.  The cutoff is glided between blocks, so
//  sweeping the knob produces a smooth filter sweep instead of clicks.
//==============================================================================
class HighPassFilter
{
public:
    static constexpr float kMinCutoffHz = 20.0f;
    static constexpr float kMaxCutoffHz = 500.0f;

    void prepare (double sampleRate, float cutoffHz = 100.0f)
    {
        sr = sampleRate;
        currentCutoff = targetCutoff = juce::jlimit (kMinCutoffHz, kMaxCutoffHz, cutoffHz);
        update();
        reset();
    }

    void reset()
    {
        for (auto& s : st)
            s = {};
    }

    // Sets the target cutoff; process() glides the coefficients towards it.
    void setCutoff (float hz) noexcept
    {
        targetCutoff = juce::jlimit (kMinCutoffHz, kMaxCutoffHz, hz);
    }

    void process (float* L, float* R, int numSamples) noexcept
    {
        // One-pole glide (~40 ms) so a knob move does not jump the coefficients.
        if (std::abs (currentCutoff - targetCutoff) > 0.01f)
        {
            const float coeff = 1.0f - std::exp (-(float) numSamples / (0.04f * (float) sr));
            currentCutoff += (targetCutoff - currentCutoff) * coeff;

            if (std::abs (currentCutoff - targetCutoff) < 0.05f)
                currentCutoff = targetCutoff;

            update();
        }

        for (int n = 0; n < numSamples; ++n)
        {
            L[n] = processSample (st[0], L[n]);
            R[n] = processSample (st[1], R[n]);
        }
    }

private:
    struct State { float ic1eq = 0.0f, ic2eq = 0.0f; };

    float processSample (State& s, float v0) const noexcept
    {
        const float v3 = v0 - s.ic2eq;
        const float v1 = a1 * s.ic1eq + a2 * v3;
        const float v2 = s.ic2eq + a2 * s.ic1eq + a3 * v3;
        s.ic1eq = 2.0f * v1 - s.ic1eq;
        s.ic2eq = 2.0f * v2 - s.ic2eq;
        return v0 - k * v1 - v2;   // high-pass output
    }

    void update() noexcept
    {
        g  = std::tan (juce::MathConstants<float>::pi * currentCutoff / (float) sr);
        k  = 1.0f / 0.7071f;                    // Butterworth (Q = 1/sqrt(2))
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    double sr = 48000.0;
    float  currentCutoff = 100.0f;
    float  targetCutoff  = 100.0f;
    float  g = 0.0f, k = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    State  st[2];
};

} // namespace lunara
