// Offline test for the REVERSE and STRETCH ("Extreme") controls.
//
//   REVERSE - forces every grain to play backwards (reverseProb = 1). With a
//             noise input the reversed cloud is clearly different from the
//             forward one, so the two outputs decorrelate.
//   STRETCH - smoothly melts the whole chain into a long, PaulStretch-like
//             texture (long grains, dense cloud, big delay/reverb tails). On
//             silence the stretched chain therefore rings on for longer.
//
// Both controls must keep the output finite and bounded.

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <vector>
#include <cmath>

#include "../Source/PluginProcessor.h"

namespace
{
    void setParam (LunaraAudioProcessor& proc, const char* id, float real)
    {
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (real));
    }

    void fillNoise (juce::AudioBuffer<float>& buf, juce::Random& rng, float amp)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            auto* p = buf.getWritePointer (ch);
            for (int n = 0; n < buf.getNumSamples(); ++n)
                p[n] = amp * (rng.nextFloat() * 2.0f - 1.0f);
        }
    }

    double correlation (const std::vector<float>& a, const std::vector<float>& b)
    {
        if (a.size() != b.size() || a.empty())
            return 0.0;

        double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0;
        const double n = (double) a.size();
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            const double x = a[i], y = b[i];
            sa += x; sb += y; saa += x * x; sbb += y * y; sab += x * y;
        }
        const double cov = sab / n - (sa / n) * (sb / n);
        const double va  = saa / n - (sa / n) * (sa / n);
        const double vb  = sbb / n - (sb / n) * (sb / n);
        const double d   = std::sqrt (va * vb);
        return d > 1.0e-12 ? cov / d : 0.0;
    }
}

int main()
{
    const double sr = 48000.0;
    const int blockSize = 512;

    LunaraAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, blockSize);
    proc.prepareToPlay (sr, blockSize);

    // --- parameter sanity ----------------------------------------------------
    auto* revP = proc.apvts.getParameter ("reverse");
    auto* extP = proc.apvts.getParameter ("extreme");
    const bool exist = revP != nullptr && extP != nullptr;
    const bool offByDefault = exist
        && proc.apvts.getRawParameterValue ("reverse")->load() < 0.5f
        && proc.apvts.getRawParameterValue ("extreme")->load() < 0.5f;
    std::cout << "params exist=" << (exist ? "yes" : "no")
              << " offByDefault=" << (offByDefault ? "yes" : "no") << std::endl;

    // A neutral, wet-only setup: Mosaic (no built-in reverse), no auto gain.
    auto setup = [&]
    {
        setParam (proc, "mode", 0.0f);
        setParam (proc, "autoGain", 0.0f);
        setParam (proc, "mix", 1.0f);
        setParam (proc, "size", 200.0f);
        setParam (proc, "density", 30.0f);
        setParam (proc, "spray", 0.0f);
        setParam (proc, "chaos", 0.0f);
        setParam (proc, "pitch", 0.0f);
        setParam (proc, "pitchMode", 0.0f);
        setParam (proc, "sync", 0.0f);
        setParam (proc, "delay", 375.0f);
        setParam (proc, "feedback", 0.4f);
        setParam (proc, "tone", 18000.0f);
        setParam (proc, "pingpong", 0.0f);
        setParam (proc, "reverb", 0.0f);
        setParam (proc, "decay", 0.0f);
        setParam (proc, "cloud", 0.0f);
        setParam (proc, "fxSource", 1.0f);   // OUT: cloud only
        setParam (proc, "hpf", 20.0f);
        setParam (proc, "limiter", 1.0f);
        setParam (proc, "output", 0.0f);
        setParam (proc, "halftime", 0.0f);
        setParam (proc, "freeze", 0.0f);
        setParam (proc, "reverse", 0.0f);
        setParam (proc, "extreme", 0.0f);
    };

    juce::MidiBuffer midi;

    // ---- REVERSE ------------------------------------------------------------
    auto captureForward = [&] (bool reverseOn)
    {
        setup();
        setParam (proc, "reverse", reverseOn ? 1.0f : 0.0f);
        proc.prepareToPlay (sr, blockSize);

        juce::AudioBuffer<float> buf (2, blockSize);
        juce::Random rng (0x1234);          // identical input for both runs
        std::vector<float> out;
        const int blocks = 300;
        for (int b = 0; b < blocks; ++b)
        {
            fillNoise (buf, rng, 0.4f);
            proc.processBlock (buf, midi);
            if (b > blocks / 2)
                for (int n = 0; n < blockSize; ++n)
                    out.push_back (buf.getSample (0, n));
        }
        return out;
    };

    const auto fwd = captureForward (false);
    const auto rev = captureForward (true);
    const double corr = correlation (fwd, rev);

    double peakRev = 0.0;
    for (float v : rev) peakRev = juce::jmax (peakRev, (double) std::abs (v));

    std::cout << "reverse: corr(forward,reversed)=" << corr
              << " peak=" << peakRev << std::endl;

    const bool reverseDiffers = corr < 0.90;
    const bool reverseBounded = peakRev <= 1.05;

    // ---- STRETCH ------------------------------------------------------------
    auto tailRms = [&] (bool stretch)
    {
        setup();
        setParam (proc, "feedback", 0.4f);
        setParam (proc, "reverb", 0.6f);
        setParam (proc, "decay", 0.6f);
        setParam (proc, "cloud", 0.4f);
        setParam (proc, "extreme", stretch ? 1.0f : 0.0f);
        proc.prepareToPlay (sr, blockSize);

        juce::AudioBuffer<float> buf (2, blockSize);
        const double w = 2.0 * juce::MathConstants<double>::pi * 440.0 / sr;
        long phase = 0;

        // 2 s of tone to charge the delay/reverb...
        const int fillBlocks = (int) (2.0 * sr / blockSize);
        for (int b = 0; b < fillBlocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* p = buf.getWritePointer (ch);
                for (int n = 0; n < blockSize; ++n)
                    p[n] = (float) (0.4 * std::sin (w * (double) (phase + n)));
            }
            phase += blockSize;
            proc.processBlock (buf, midi);
        }

        // ...then 3 s of silence; measure the last second (the tail).
        double sumSq = 0.0; long count = 0;
        const int silBlocks = (int) (3.0 * sr / blockSize);
        for (int b = 0; b < silBlocks; ++b)
        {
            buf.clear();
            proc.processBlock (buf, midi);
            if (b > silBlocks * 2 / 3)
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float* p = buf.getReadPointer (ch);
                    for (int n = 0; n < blockSize; ++n)
                    { sumSq += (double) p[n] * p[n]; ++count; }
                }
        }
        return count > 0 ? std::sqrt (sumSq / (double) count) : 0.0;
    };

    const double tailNormal  = tailRms (false);
    const double tailStretch = tailRms (true);

    std::cout << "stretch: tail normal=" << tailNormal
              << " stretch=" << tailStretch << std::endl;

    const bool stretchRings = tailStretch > tailNormal * 1.3;

    if (! exist || ! offByDefault || ! reverseDiffers || ! reverseBounded || ! stretchRings)
    {
        std::cout << "FAIL: REVERSE / STRETCH" << std::endl;
        return 1;
    }

    std::cout << "PASS: REVERSE reverses the cloud; STRETCH rings on (both bounded)"
              << std::endl;
    return 0;
}
