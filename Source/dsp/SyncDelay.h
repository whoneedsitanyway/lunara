#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace lunara
{

//==============================================================================
//  Stereo feedback delay with a resonant-capable 2-pole low-pass (TPT state
//  variable filter) in the feedback path and optional ping-pong cross-feed.
//  Time is fed in milliseconds (already computed from the host BPM by the
//  processor) so the delay can lock to the musical grid.
//
//  Safety: the feedback path has a DC blocker and a soft saturator, so even at
//  maximum feedback it can never run away into noise.
//==============================================================================
class SyncDelay
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int maxSamples = (int) (sampleRate * kMaxSeconds) + 4;
        for (auto& d : lines)
            d.assign ((size_t) maxSamples, 0.0f);
        writePos = 0;
        currentDelaySamples = -1.0f;   // snap to the target on the first block
        for (auto& f : svf) f.reset();
        dcX1[0] = dcX1[1] = dcY1[0] = dcY1[1] = 0.0f;
    }

    void reset()
    {
        for (auto& d : lines)
            std::fill (d.begin(), d.end(), 0.0f);
        writePos = 0;
        currentDelaySamples = -1.0f;
        for (auto& f : svf) f.reset();
        dcX1[0] = dcX1[1] = dcY1[0] = dcY1[1] = 0.0f;
    }

    void process (float* L, float* R, int numSamples,
                  float delayMs, float feedback, float toneHz, float pingPong)
    {
        const int lineLen = (int) lines[0].size();
        const float targetDelay = juce::jlimit (1.0f, (float) (lineLen - 2),
                                                delayMs * 0.001f * (float) sr);

        // First block after prepare/reset snaps to the target so the delay never
        // sweeps up from zero. Afterwards the delay time glides (tape-style)
        // towards the target, so changing the division/time bends the pitch
        // instead of clicking.
        if (currentDelaySamples < 0.0f)
            currentDelaySamples = targetDelay;

        const float glideCoeff = 1.0f - std::exp (-1.0f / (0.06f * (float) sr));   // ~60 ms

        // 2-pole low-pass: cut-off mapped, Q fixed for a clean 12 dB/oct slope.
        const float cutoff = juce::jlimit (40.0f, (float) sr * 0.45f, toneHz);
        svf[0].setLowpass (cutoff, (float) sr, 0.85f);
        svf[1].setLowpass (cutoff, (float) sr, 0.85f);

        const float fb = juce::jlimit (0.0f, 0.95f, feedback);
        const float pp = juce::jlimit (0.0f, 1.0f, pingPong);

        for (int n = 0; n < numSamples; ++n)
        {
            currentDelaySamples += (targetDelay - currentDelaySamples) * glideCoeff;

            float readPos = (float) writePos - currentDelaySamples;
            if (readPos < 0.0f)
                readPos += (float) lineLen;

            const float dl = readInterp (lines[0], readPos, lineLen);
            const float dr = readInterp (lines[1], readPos, lineLen);

            // Ping-pong cross-feed of the recirculating signal.
            const float fl = dl * (1.0f - pp) + dr * pp;
            const float fr = dr * (1.0f - pp) + dl * pp;

            // Damping low-pass inside the feedback loop (2-pole).
            const float lpL = svf[0].processLowpass (fl);
            const float lpR = svf[1].processLowpass (fr);

            // DC blocker on the feedback (prevents low-frequency build-up).
            const float fbL = dcBlock (0, lpL);
            const float fbR = dcBlock (1, lpR);

            // Input injection. At full ping-pong the (summed) input is fed into
            // one line only, so the echoes audibly bounce left/right even for
            // mono sources (a plain cross-feed of identical channels does nothing).
            const float monoIn = (L[n] + R[n]) * 0.5f;
            const float injL = L[n] * (1.0f - pp) + monoIn * pp;
            const float injR = R[n] * (1.0f - pp);

            lines[0][(size_t) writePos] = softSat (injL + fbL * fb);
            lines[1][(size_t) writePos] = softSat (injR + fbR * fb);

            // The filtered (tone-shaped) signal is both the audible wet output
            // and the recirculating tail, so TONE is clearly audible even at low
            // feedback.
            L[n] += lpL;
            R[n] += lpR;

            if (++writePos >= lineLen) writePos = 0;
        }
    }

private:
    //--------------------------------------------------------------------------
    //  Topology-preserving-transform state-variable filter (Zavalishin).
    //--------------------------------------------------------------------------
    struct Svf
    {
        void setLowpass (float cutoff, float sampleRate, float q) noexcept
        {
            g = std::tan (juce::MathConstants<float>::pi * cutoff / sampleRate);
            k = 1.0f / juce::jmax (0.05f, q);
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void reset() noexcept { ic1eq = ic2eq = 0.0f; }

        float processLowpass (float v0) noexcept
        {
            const float v3 = v0 - ic2eq;
            const float v1 = a1 * ic1eq + a2 * v3;
            const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
            ic1eq = 2.0f * v1 - ic1eq;
            ic2eq = 2.0f * v2 - ic2eq;
            return v2;
        }

        float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        float ic1eq = 0.0f, ic2eq = 0.0f;
    };

    static constexpr double kMaxSeconds = 4.0;

    float dcBlock (int ch, float x) noexcept
    {
        const float y = x - dcX1[ch] + 0.995f * dcY1[ch];
        dcX1[ch] = x;
        dcY1[ch] = y;
        return y;
    }

    static float softSat (float x) noexcept
    {
        if (x >  0.95f) return  0.95f + 0.05f * std::tanh ((x - 0.95f) / 0.05f);
        if (x < -0.95f) return -0.95f + 0.05f * std::tanh ((x + 0.95f) / 0.05f);
        return x;
    }

    static float readInterp (const std::vector<float>& line, float pos, int lineLen) noexcept
    {
        const int i0 = (int) pos;
        const int i1 = (i0 + 1 < lineLen) ? i0 + 1 : 0;
        const float frac = pos - (float) i0;
        const float a = line[(size_t) i0];
        const float b = line[(size_t) i1];
        return a + (b - a) * frac;
    }

    double sr = 48000.0;
    std::vector<float> lines[2];
    int   writePos = 0;
    float currentDelaySamples = -1.0f;
    Svf   svf[2];
    float dcX1[2] { 0.0f, 0.0f };
    float dcY1[2] { 0.0f, 0.0f };
};

} // namespace lunara
