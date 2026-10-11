// Offline stress test for the Lunara DSP chain.
//
// Runs the granular -> delay -> reverb chain with extreme settings and verifies
// that (a) the output is always finite and bounded and (b) the signal entering
// the limiter is not "hot" (which used to slam the feedback loops into
// saturation and sound like noise).  Also exercises rapid parameter changes.

#include <juce_audio_basics/juce_audio_basics.h>
#include <iostream>
#include <cmath>
#include <random>
#include <limits>

#include "../Source/dsp/GrainEngine.h"
#include "../Source/dsp/SyncDelay.h"
#include "../Source/dsp/CloudReverb.h"
#include "../Source/dsp/Limiter.h"

int main()
{
    const double sr = 48000.0;
    const int block = 512;

    lunara::GrainEngine    grain;
    lunara::SyncDelay      delay;
    lunara::CloudReverb    reverb;
    lunara::Limiter        limiter;

    grain.prepare (sr, block, 2);
    delay.prepare (sr);
    reverb.prepare (sr);
    limiter.prepare (sr);

    lunara::GrainParams gp;
    gp.grainSizeMs   = 500.0f;
    gp.density       = 120.0f;
    gp.spray         = 1.0f;
    gp.pitchSemis    = 12.0f;
    gp.pitchQuantize = true;
    gp.reverseProb   = 1.0f;
    gp.stereoSpread  = 1.0f;
    gp.jitter        = 1.0f;
    gp.chaos         = 1.0f;
    gp.drift         = 1.0f;

    juce::AudioBuffer<float> dry (2, block), wet (2, block);
    std::mt19937 rng (12345);
    std::uniform_real_distribution<float> dist (-1.0f, 1.0f);

    double outMaxAbs = 0.0, wetMaxAbs = 0.0, grainMax = 0.0, delayMax = 0.0;
    double wetRmsAcc = 0.0; long wetRmsN = 0;
    bool nonFinite = false;

    const int numBlocks = 4000;   // ~42 s at 48k
    for (int b = 0; b < numBlocks; ++b)
    {
        // Periodically shake the parameters hard, like hitting RANDOM.
        if (b % 400 == 0)
        {
            gp.density     = (b % 800 == 0) ? 120.0f : 8.0f;
            gp.grainSizeMs = (b % 1200 == 0) ? 500.0f : 60.0f;
            gp.chaos       = (b % 1600 == 0) ? 1.0f : 0.0f;
            gp.pitchSemis  = (b % 600 == 0) ? 12.0f : -12.0f;
        }

        dry.clear();
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* p = dry.getWritePointer (ch);
            for (int n = 0; n < block; ++n)
                p[n] = dist (rng) * 0.8f;
            if (b % 50 == 0)
                p[0] = (b % 100 == 0) ? 1.0f : -1.0f;
        }

        wet.clear();
        grain.process (dry.getReadPointer (0), dry.getReadPointer (1),
                       wet.getWritePointer (0), wet.getWritePointer (1), block, gp);

        for (int ch = 0; ch < 2; ++ch) { const float* q = wet.getReadPointer (ch);
            for (int n = 0; n < block; ++n) grainMax = juce::jmax (grainMax, (double) std::abs (q[n])); }

        delay.process (wet.getWritePointer (0), wet.getWritePointer (1),
                       block, 375.0f, 0.95f, 18000.0f, 1.0f);
        for (int ch = 0; ch < 2; ++ch) { const float* q = wet.getReadPointer (ch);
            for (int n = 0; n < block; ++n) delayMax = juce::jmax (delayMax, (double) std::abs (q[n])); }
        reverb.process (wet.getWritePointer (0), wet.getWritePointer (1),
                        block, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.2f);

        // Measure the signal *before* the limiter - this is what the feedback
        // loops have to cope with.
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* p = wet.getReadPointer (ch);
            for (int n = 0; n < block; ++n)
            {
                if (! std::isfinite (p[n])) { nonFinite = true; continue; }
                const double a = std::abs ((double) p[n]);
                wetMaxAbs = juce::jmax (wetMaxAbs, a);
                if (b > 100) { wetRmsAcc += (double) p[n] * p[n]; ++wetRmsN; }
            }
        }

        limiter.process (wet.getWritePointer (0), wet.getWritePointer (1),
                         block, 0.98f);

        for (int ch = 0; ch < 2; ++ch)
        {
            const float* p = wet.getReadPointer (ch);
            for (int n = 0; n < block; ++n)
                if (std::isfinite (p[n]))
                    outMaxAbs = juce::jmax (outMaxAbs, (double) std::abs (p[n]));
        }
    }

    const double wetRms = wetRmsN > 0 ? std::sqrt (wetRmsAcc / (double) wetRmsN) : 0.0;

    // --- non-finite robustness -----------------------------------------------
    // A single NaN / Inf sample (leaked from an upstream stage or a corrupted
    // state) must never propagate through the chain, turn into an out-of-bounds
    // read, or blow the feedback loops up.  Each stage must sanitise it.
    bool nfBlowup = false;
    double nfMaxAbs = 0.0;
    {
        lunara::GrainEngine grain2;
        lunara::SyncDelay   delay2;
        lunara::CloudReverb reverb2;
        lunara::Limiter     limiter2;
        grain2.prepare (sr, block, 2);
        delay2.prepare (sr);
        reverb2.prepare (sr);
        limiter2.prepare (sr);

        for (int b = 0; b < 300; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* p = dry.getWritePointer (ch);
                for (int n = 0; n < block; ++n)
                    p[n] = ((n == 0 || n == block / 2) && b % 2 == 0)
                               ? (b % 4 == 0 ? std::nanf ("") : std::numeric_limits<float>::infinity())
                               : 0.3f;
            }

            wet.clear();
            grain2.process (dry.getReadPointer (0), dry.getReadPointer (1),
                            wet.getWritePointer (0), wet.getWritePointer (1), block, gp);
            delay2.process (wet.getWritePointer (0), wet.getWritePointer (1),
                            block, 375.0f, 0.95f, 18000.0f, 1.0f);
            reverb2.process (wet.getWritePointer (0), wet.getWritePointer (1),
                             block, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.2f);
            limiter2.process (wet.getWritePointer (0), wet.getWritePointer (1), block, 0.98f);

            for (int ch = 0; ch < 2; ++ch)
            {
                const float* p = wet.getReadPointer (ch);
                for (int n = 0; n < block; ++n)
                {
                    if (! std::isfinite (p[n])) nfBlowup = true;
                    else nfMaxAbs = juce::jmax (nfMaxAbs, (double) std::abs (p[n]));
                }
            }
        }
    }

    std::cout << "blocks=" << numBlocks
              << "  outMaxAbs=" << outMaxAbs
              << "  grainMax=" << grainMax << "  delayMax=" << delayMax << "  wetMaxAbs=" << wetMaxAbs
              << "  wetRms=" << wetRms
              << "  nonFinite=" << (nonFinite ? "YES" : "no")
              << "  nfMaxAbs=" << nfMaxAbs << "  nfBlowup=" << (nfBlowup ? "YES" : "no") << std::endl;

    if (nonFinite || outMaxAbs > 1.05)
    {
        std::cout << "FAIL: DSP did not stay bounded/finite" << std::endl;
        return 1;
    }

    if (wetMaxAbs > 4.0 || wetRms > 1.0)
    {
        std::cout << "FAIL: pre-limiter signal is too hot (would saturate)" << std::endl;
        return 1;
    }

    if (nfBlowup || nfMaxAbs > 1.05)
    {
        std::cout << "FAIL: non-finite input was not sanitised (leaked or blew up)" << std::endl;
        return 1;
    }

    std::cout << "PASS: DSP stayed finite, bounded and at a musical level" << std::endl;
    return 0;
}
