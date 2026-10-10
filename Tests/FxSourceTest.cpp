// Offline test for the MOOD MK2-style FX SOURCE routing switch.
//
// The switch selects what the DELAY + SPACE effects process:
//   IN   (0): the live input only
//   OUT  (1): the granular cloud only (default / legacy)
//   BOTH (2): the live input + the cloud
//
// To make the routing observable, the granular cloud is made very sparse (1
// grain per second, 20 ms long) while a continuous sine is fed in. In OUT mode
// the effects therefore only see the sparse cloud (low output level), whereas in
// IN / BOTH they also process the continuous live input (clearly louder).

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <cmath>

#include "../Source/PluginProcessor.h"

namespace
{
    void setParam (LunaraAudioProcessor& proc, const char* id, float real)
    {
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (real));
    }

    struct Result { double rms; double maxAbs; };

    Result measure (LunaraAudioProcessor& proc, juce::AudioBuffer<float>& buf,
                    int blocks, int blockSize)
    {
        double sumSq = 0.0, maxAbs = 0.0;
        long count = 0;
        juce::MidiBuffer midi;

        const double w = 2.0 * juce::MathConstants<double>::pi * 440.0
                         / proc.getSampleRate();
        long phase = 0;

        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                auto* p = buf.getWritePointer (ch);
                for (int n = 0; n < blockSize; ++n)
                    p[n] = (float) (0.5 * std::sin (w * (double) (phase + n)));
            }
            phase += blockSize;

            proc.processBlock (buf, midi);

            // Only measure after the chain has settled.
            if (b > blocks * 3 / 4)
                for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                {
                    const float* p = buf.getReadPointer (ch);
                    for (int n = 0; n < blockSize; ++n)
                    {
                        const double v = (double) p[n];
                        sumSq += v * v;
                        ++count;
                        maxAbs = juce::jmax (maxAbs, std::abs (v));
                    }
                }
        }

        return { count > 0 ? std::sqrt (sumSq / (double) count) : 0.0, maxAbs };
    }
}

int main()
{
    const double sr = 48000.0;
    const int blockSize = 512;

    LunaraAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, blockSize);
    proc.prepareToPlay (sr, blockSize);

    // --- parameter sanity: exists, defaults to OUT -------------------------
    auto* p = proc.apvts.getParameter ("fxSource");
    const bool exists = p != nullptr;
    const bool defaultOut = exists
                            && proc.apvts.getRawParameterValue ("fxSource")->load() == 1.0f;
    std::cout << "param exists=" << (exists ? "yes" : "no")
              << " default=" << (exists ? p->getCurrentValueAsText() : juce::String ("?"))
              << std::endl;

    // --- a setting that isolates the routing --------------------------------
    setParam (proc, "autoGain", 0.0f);
    setParam (proc, "mix", 1.0f);
    setParam (proc, "reverb", 1.0f);
    setParam (proc, "decay", 0.9f);
    setParam (proc, "cloud", 0.3f);
    setParam (proc, "feedback", 0.4f);
    setParam (proc, "pingpong", 0.0f);
    setParam (proc, "tone", 18000.0f);
    setParam (proc, "sync", 0.0f);
    setParam (proc, "delay", 200.0f);
    setParam (proc, "density", 1.0f);      // sparse cloud
    setParam (proc, "size", 20.0f);
    setParam (proc, "spray", 0.0f);
    setParam (proc, "chaos", 0.0f);
    setParam (proc, "output", 0.0f);
    setParam (proc, "halftime", 0.0f);

    juce::AudioBuffer<float> buf (2, blockSize);

    auto run = [&] (int mode)
    {
        setParam (proc, "fxSource", (float) mode);
        proc.prepareToPlay (sr, blockSize);   // reset the DSP state between runs
        return measure (proc, buf, 500, blockSize);
    };

    const auto in   = run (0);
    const auto out  = run (1);
    const auto both = run (2);

    std::cout << "IN   rms=" << in.rms   << " max=" << in.maxAbs   << "\n"
              << "OUT  rms=" << out.rms  << " max=" << out.maxAbs  << "\n"
              << "BOTH rms=" << both.rms << " max=" << both.maxAbs << std::endl;

    const bool bounded    = in.maxAbs <= 1.05 && out.maxAbs <= 1.05 && both.maxAbs <= 1.05;
    // IN processes the continuous input, so it must be clearly louder than OUT
    // (which only processes the sparse grain cloud).
    const bool inLouder   = in.rms   > out.rms * 2.0;
    const bool bothLouder = both.rms > out.rms * 1.5;

    if (! exists || ! defaultOut || ! bounded || ! inLouder || ! bothLouder)
    {
        std::cout << "FAIL: FX SOURCE routing" << std::endl;
        return 1;
    }

    std::cout << "PASS: FX SOURCE routes input/cloud/both and stays bounded" << std::endl;
    return 0;
}
