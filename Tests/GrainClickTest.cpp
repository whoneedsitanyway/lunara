// Regression test: a grain must never read across the write head.
//
// The circular buffer holds the newest sample right at `writePos` and the oldest
// one just before it, so a read head that runs across that point splices the
// newest sample onto ~2.5 s old material.  That is an unwindowed step, heard as
// a click ("vinyl crackle") - and at MIX = 100% there is no dry signal left to
// mask it.  A grain runs into the write head when its playback rate is off unity
// (a pitched grain travels faster/slower than the head) or when a reverse grain
// starts ahead of it.
//
// The test drives the engine with a steady 97 Hz sine: its period does not
// divide the 2.5 s buffer, so a splice would be a step of up to the full
// amplitude, while a clean read is as smooth as the input itself.

#include <juce_audio_basics/juce_audio_basics.h>
#include <iostream>
#include <cmath>

#include "../Source/dsp/GrainEngine.h"

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlock      = 512;
    constexpr double kInputHz    = 97.0;
    constexpr double kInputAmp   = 0.5;

    // Runs the engine over 4 s of a steady sine and returns the largest
    // sample-to-sample step of the settled tail plus that tail's RMS.
    void measure (float pitchSemis, bool reverse, double& maxStep, double& tailRms)
    {
        lunara::GrainEngine grain;
        grain.prepare (kSampleRate, kBlock, 2);

        lunara::GrainParams p;
        p.grainSizeMs   = 200.0f;
        p.density       = 40.0f;
        p.spray         = 1.0f;      // widest spread: grains start all over the buffer
        p.pitchSemis    = pitchSemis;
        p.pitchQuantize = true;
        p.reverseProb   = reverse ? 1.0f : 0.0f;
        p.stereoSpread  = 0.65f;
        p.jitter        = 0.35f;
        p.chaos         = 0.0f;
        p.drift         = 0.0f;

        juce::AudioBuffer<float> in (2, kBlock), out (2, kBlock);

        const int total = (int) (4.0 * kSampleRate);
        const int from  = total - (int) (1.5 * kSampleRate);   // settled tail

        maxStep = 0.0;
        double energy = 0.0;
        int    count  = 0;
        float  prev   = 0.0f;

        for (int base = 0; base < total; base += kBlock)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* q = in.getWritePointer (ch);
                for (int n = 0; n < kBlock; ++n)
                {
                    const int i = base + n;
                    q[n] = (float) (kInputAmp * std::sin (juce::MathConstants<double>::twoPi
                                                          * kInputHz * (double) i / kSampleRate));
                }
            }

            grain.process (in.getReadPointer (0), in.getReadPointer (1),
                           out.getWritePointer (0), out.getWritePointer (1), kBlock, p);

            const float* y = out.getReadPointer (0);
            for (int n = 0; n < kBlock; ++n)
            {
                if (base + n >= from)
                {
                    maxStep = juce::jmax (maxStep, (double) std::abs (y[n] - prev));
                    energy += (double) y[n] * y[n];
                    ++count;
                }
                prev = y[n];
            }
        }

        tailRms = count > 0 ? std::sqrt (energy / (double) count) : 0.0;
    }
}

int main()
{
    // Slope of the input itself; a grain reading at +12 st doubles it.
    const double inputSlope = kInputAmp * juce::MathConstants<double>::twoPi * kInputHz / kSampleRate;

    // A spliced buffer boundary steps by up to the full amplitude (0.5), i.e.
    // ~70x the input slope.  A clean cloud stays within a few slopes, so the
    // limit leaves room for the (legitimate) summation of overlapping grains.
    const double limit = 0.06;

    struct Case { const char* name; float pitch; bool reverse; };
    const Case cases[] =
    {
        { "pitch   0 st, forward",   0.0f, false },
        { "pitch   0 st, reverse",   0.0f, true  },
        { "pitch  +7 st, forward",   7.0f, false },
        { "pitch  +7 st, reverse",   7.0f, true  },
        { "pitch +12 st, forward",  12.0f, false },
        { "pitch +12 st, reverse",  12.0f, true  },
        { "pitch -12 st, forward", -12.0f, false },
        { "pitch -12 st, reverse", -12.0f, true  },
    };

    int failures = 0;

    std::cout << "input slope per sample=" << inputSlope
              << "  click limit=" << limit << std::endl;

    for (const auto& c : cases)
    {
        double maxStep = 0.0, rms = 0.0;
        measure (c.pitch, c.reverse, maxStep, rms);

        std::cout << c.name << ": max step=" << maxStep
                  << " (" << (maxStep / inputSlope) << "x input slope)"
                  << "  tail rms=" << rms << std::endl;

        if (maxStep > limit)
        {
            ++failures;
            std::cout << "FAIL: a grain read across the write head (click)" << std::endl;
        }

        if (rms < 0.05)
        {
            ++failures;
            std::cout << "FAIL: the cloud is silent - the check would be meaningless" << std::endl;
        }
    }

    if (failures > 0)
    {
        std::cout << "FAIL: grains splice the ring buffer boundary" << std::endl;
        return 1;
    }

    std::cout << "PASS: no grain reads across the write head (no clicks)" << std::endl;
    return 0;
}
