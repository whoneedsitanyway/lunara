#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace lunara
{

//==============================================================================
//  Cloud reverb.
//
//  An 8-line feedback delay network (FDN) fed through a bank of diffusion
//  all-passes, with per-line damping, slow modulation and a "cloud" smear
//  stage.  Tuned for long, drifting ambient tails rather than realistic rooms.
//  Adds its wet output on top of the incoming (already granular/delayed) signal.
//==============================================================================
class CloudReverb
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;

        static constexpr double baseMs[8] =
            { 71.3, 89.7, 107.1, 131.9, 149.3, 173.7, 197.1, 223.9 };

        for (int i = 0; i < kLines; ++i)
        {
            const int maxLen = (int) (sampleRate * (baseMs[i] * 0.001 * kMaxSizeScale)) + 64;
            lines[i].assign ((size_t) maxLen, 0.0f);
            writeIdx[i] = 0;
            lpState[i] = 0.0f;
            modPhase[i] = (float) i / (float) kLines;
            modInc[i] = (0.05f + 0.03f * (float) i) / (float) sampleRate;
            baseLen[i] = (float) (baseMs[i] * 0.001 * sampleRate);
        }

        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kDiff; ++i)
            {
                diff[c][i].assign ((size_t) (sampleRate * 0.05) + 4, 0.0f);
                diffIdx[c][i] = 0;
            }

        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kSmear; ++i)
            {
                smear[c][i].assign ((size_t) (sampleRate * kSmearMs[c][i] * 0.001) + 4, 0.0f);
                smearIdx[c][i] = 0;
            }
        injLp = 0.0f;
        smoothSizeScale = -1.0f;   // snap to the target on the first block
    }

    void reset()
    {
        for (int i = 0; i < kLines; ++i)
        {
            std::fill (lines[i].begin(), lines[i].end(), 0.0f);
            writeIdx[i] = 0;
            lpState[i] = 0.0f;
        }
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kDiff; ++i)
                std::fill (diff[c][i].begin(), diff[c][i].end(), 0.0f);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kSmear; ++i)
                std::fill (smear[c][i].begin(), smear[c][i].end(), 0.0f);
        injLp = 0.0f;
        smoothSizeScale = -1.0f;
    }

    void process (float* L, float* R, int numSamples,
                  float size, float decay, float damp, float modDepth, float cloud,
                  float send)
    {
        size  = juce::jlimit (0.0f, 1.0f, size);
        decay = juce::jlimit (0.0f, 1.0f, decay);
        damp  = juce::jlimit (0.0f, 1.0f, damp);
        cloud = juce::jlimit (0.0f, 1.0f, cloud);
        send  = juce::jlimit (0.0f, 1.5f, send);

        const float targetSizeScale = juce::jmap (size, 0.35f, (float) kMaxSizeScale);

        // Glide the size so turning the knob (or loading a preset) does not jump
        // the read taps and click; on the first block it snaps to the target.
        if (smoothSizeScale < 0.0f)
            smoothSizeScale = targetSizeScale;
        else
        {
            const float sizeCoeff = 1.0f - std::exp (-(float) numSamples / (0.05f * (float) sr));
            smoothSizeScale += (targetSizeScale - smoothSizeScale) * sizeCoeff;
        }

        // The glide must never carry a non-finite value into the read taps.
        if (! std::isfinite (smoothSizeScale))
            smoothSizeScale = targetSizeScale;

        const float fb        = 0.50f + decay * 0.36f;
        const float lpCoeff   = std::exp (-juce::MathConstants<float>::twoPi
                                          * juce::jmap (damp, 12000.0f, 1500.0f)
                                          / (float) sr);
        const float modAmt    = modDepth * 12.0f;
        const float diffG     = 0.6f;

        // One-pole high-pass (~40 Hz) on the input so the network cannot build
        // up low-frequency rumble, plus injection normalisation by sqrt(N) so
        // feeding 8 lines does not multiply the input energy.
        const float hpCoeff  = std::exp (-juce::MathConstants<float>::twoPi * 40.0f / (float) sr);
        const float hpAlpha  = 1.0f - hpCoeff;   // low-pass smoothing coefficient
        const float injNorm  = 2.2f * send / std::sqrt ((float) kLines);
        const float outNorm  = 0.6f;

        // The modulation is very slow, so compute each line's length once per
        // block instead of calling sin() every sample (big CPU saving).
        float lineLen[kLines];
        int   lineSize[kLines];
        for (int i = 0; i < kLines; ++i)
        {
            lineLen[i] = baseLen[i] * smoothSizeScale
                       + modAmt * std::sin (juce::MathConstants<float>::twoPi * modPhase[i]);
            // A non-finite length would defeat jlimit (NaN fails every comparison)
            // and then make the read index garbage; snap it to a safe value.
            if (! std::isfinite (lineLen[i]))
                lineLen[i] = 8.0f;
            // Never let the read tap run past the allocated line.
            lineLen[i] = juce::jlimit (8.0f, (float) lines[i].size() - 4.0f, lineLen[i]);
            lineSize[i] = (int) lines[i].size();
            modPhase[i] += modInc[i] * (float) numSamples;
            if (modPhase[i] >= 1.0f) modPhase[i] -= std::floor (modPhase[i]);
        }

        for (int n = 0; n < numSamples; ++n)
        {
            const float dryL = std::isfinite (L[n]) ? L[n] : 0.0f;
            const float dryR = std::isfinite (R[n]) ? R[n] : 0.0f;

            float dL = dryL, dR = dryR;
            for (int i = 0; i < kDiff; ++i)
            {
                dL = allpass (diff[0][i], diffIdx[0][i], dL, diffG);
                dR = allpass (diff[1][i], diffIdx[1][i], dR, diffG);
            }

            const float mono = (dL + dR) * 0.5f;
            injLp += hpAlpha * (mono - injLp);
            const float inj = (mono - injLp) * injNorm;

            float out[kLines];
            float sum = 0.0f;
            for (int i = 0; i < kLines; ++i)
            {
                float rp = (float) writeIdx[i] - lineLen[i];
                if (rp < 0.0f) rp += (float) lineSize[i];

                const float v = readInterp (lines[i], rp, lineSize[i]);
                out[i] = v;
                sum += v;
            }

            const float norm = sum * (2.0f / (float) kLines);

            for (int i = 0; i < kLines; ++i)
            {
                float x = out[i] - norm;
                x += inj * ((i % 2 == 0) ? 1.0f : -1.0f);

                lpState[i] = x + lpCoeff * (lpState[i] - x);

                // Soft saturation bounds the FDN state so the network can never
                // run away into noise, even at maximum decay/send.
                lines[i][(size_t) writeIdx[i]] = softSat (lpState[i] * fb);
                if (++writeIdx[i] >= (int) lines[i].size())
                    writeIdx[i] = 0;
            }

            float wetL = 0.0f, wetR = 0.0f;
            for (int i = 0; i < kLines; ++i)
            {
                const float sign = (i % 2 == 0) ? 1.0f : -1.0f;
                wetL += out[i] * sign;
                wetR += out[(i + 3) % kLines] * -sign;
            }
            wetL *= outNorm;
            wetR *= outNorm;

            // Cloud smear: a cascade of all-pass diffusers (more stages and more
            // feedback as CLOUD rises), which blurs the tail into a thick cloud.
            if (cloud > 0.0f)
            {
                const int stages = 1 + (int) (cloud * (float) (kSmear - 1));
                const float g = 0.55f + cloud * 0.35f;
                for (int i = 0; i < stages; ++i)
                {
                    wetL = allpass (smear[0][i], smearIdx[0][i], wetL, g);
                    wetR = allpass (smear[1][i], smearIdx[1][i], wetR, g);
                }
            }

            L[n] = dryL + wetL;
            R[n] = dryR + wetR;
        }
    }

private:
    static constexpr int kLines = 8;
    static constexpr int kDiff  = 4;
    static constexpr int kSmear = 3;
    static constexpr double kMaxSizeScale = 4.0;

    // Smear all-pass delay lengths (ms), deliberately mutually prime-ish.
    static constexpr double kSmearMs[2][kSmear] =
    {
        {  9.1, 13.7, 11.3 },   // left
        { 12.9,  8.3, 15.1 }    // right
    };

    static float allpass (std::vector<float>& buf, int& idx, float x, float g) noexcept
    {
        const int n = (int) buf.size();
        const float delayed = buf[(size_t) idx];
        const float y = -g * x + delayed;
        buf[(size_t) idx] = x + g * y;
        if (++idx >= n) idx = 0;
        return y;
    }

    static float softSat (float x) noexcept
    {
        // NaN / Inf fail both range comparisons and would otherwise pass straight
        // through into the FDN state (and out to the output), so trap them first.
        if (! std::isfinite (x)) return 0.0f;
        if (x >  0.95f) return  0.95f + 0.05f * std::tanh ((x - 0.95f) / 0.05f);
        if (x < -0.95f) return -0.95f + 0.05f * std::tanh ((x + 0.95f) / 0.05f);
        return x;
    }

    float readInterp (const std::vector<float>& line, float pos, int sz) const noexcept
    {
        if (sz < 2)
            return 0.0f;

        // Keep the tap strictly in range: a non-finite or out-of-range position
        // must never turn into an out-of-bounds read of heap memory.
        if (! std::isfinite (pos))
            pos = 0.0f;
        pos = juce::jlimit (0.0f, (float) (sz - 1), pos);

        const int i0 = (int) pos;
        const int i1 = (i0 + 1 < sz) ? i0 + 1 : 0;
        const float frac = pos - (float) i0;
        const float a = line[(size_t) i0];
        const float b = line[(size_t) i1];
        return a + (b - a) * frac;
    }

    double sr = 48000.0;
    std::vector<float> lines[kLines];
    std::vector<float> diff[2][kDiff];
    std::vector<float> smear[2][kSmear];

    int   writeIdx[kLines] {};
    int   diffIdx[2][kDiff] {};
    int   smearIdx[2][kSmear] {};
    float lpState[kLines] {};
    float modPhase[kLines] {};
    float modInc[kLines] {};
    float baseLen[kLines] {};
    float smoothSizeScale = -1.0f;
    float injLp = 0.0f;
};

} // namespace lunara
