// Regression tests for the wet path at MIX = 1.
//
//   (1) With FX SOURCE = OUT the output must be the granular cloud only - the
//       dry signal must not leak through at lag 0.
//   (2) The cloud inherently lags the input: it reads grains from the past
//       (writePos - grainLen - spray - jitter), so the wet signal is delayed
//       relative to the host timeline.
//   (3) With FX SOURCE = BOTH (the default preset) the dry is also routed
//       through the delay/reverb, so the wet output does contain a delayed copy
//       of the dry. That is by design - switching FX SOURCE to OUT removes it.
//
// The first block can only read a cleared (empty) buffer, so a truly wet-only
// output is silent there even with a loud input.

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <cmath>
#include <vector>

#include "../Source/PluginProcessor.h"

namespace
{
    void setParam (LunaraAudioProcessor& proc, const char* id, float real)
    {
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (real));
    }

    // A wet-only granular path with the delay/reverb effectively bypassed.
    void configureWetOnly (LunaraAudioProcessor& proc, int modeIdx, float sizeMs,
                           int fxSource = 1)
    {
        setParam (proc, "mix", 1.0f);        // wet only
        setParam (proc, "fxSource", (float) fxSource);
        setParam (proc, "reverb", 0.0f);
        setParam (proc, "feedback", 0.0f);
        setParam (proc, "delay", 10.0f);     // minimal: delays the source, not the dry
        setParam (proc, "sync", 0.0f);
        setParam (proc, "density", 100.0f);
        setParam (proc, "spray", 0.0f);
        setParam (proc, "size", sizeMs);
        setParam (proc, "pitch", 0.0f);
        setParam (proc, "chaos", 0.0f);
        setParam (proc, "mode", (float) modeIdx);
        setParam (proc, "autoGain", 0.0f);
        setParam (proc, "limiter", 0.0f);
    }

    // RMS of the first block for a steady 220 Hz input (the buffer is still
    // empty then, so anything non-zero comes from the dry path).
    double firstBlockRms (int fxSource)
    {
        const double sr = 48000.0;
        const int blockSize = 512;

        LunaraAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sr, blockSize);
        configureWetOnly (proc, 0, 200.0f, fxSource);
        proc.prepareToPlay (sr, blockSize);   // params set first -> no ramps

        juce::AudioBuffer<float> buf (2, blockSize);
        for (int n = 0; n < blockSize; ++n)
        {
            const float s = (float) std::sin (juce::MathConstants<double>::twoPi
                                              * 220.0 * n / sr);
            buf.setSample (0, n, s);
            buf.setSample (1, n, s);
        }
        juce::MidiBuffer midi;
        proc.processBlock (buf, midi);

        double e = 0.0;
        for (int n = 0; n < blockSize; ++n)
            e += (double) buf.getSample (0, n) * buf.getSample (0, n);
        return std::sqrt (e / blockSize);
    }

    // Energy centroid (ms) of the output relative to the input burst: a positive
    // value means the cloud lags the input.
    double measureCentroidMs (int modeIdx, float sizeMs)
    {
        const double sr = 48000.0;
        const int blockSize = 512;

        LunaraAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sr, blockSize);
        configureWetOnly (proc, modeIdx, sizeMs);
        proc.prepareToPlay (sr, blockSize);

        const int totalSamples = (int) (3.0 * sr);
        const int burstStart   = (int) (0.5 * sr);
        const int burstLen     = (int) (0.1 * sr);

        std::vector<float> out;
        out.reserve ((size_t) totalSamples);

        double phase = 0.0;
        for (int base = 0; base < totalSamples; base += blockSize)
        {
            juce::AudioBuffer<float> buf (2, blockSize);
            for (int n = 0; n < blockSize; ++n)
            {
                const int gi = base + n;
                float s = 0.0f;
                if (gi >= burstStart && gi < burstStart + burstLen)
                {
                    s = (float) std::sin (phase);
                    phase += juce::MathConstants<double>::twoPi * 1000.0 / sr;
                }
                buf.setSample (0, n, s);
                buf.setSample (1, n, s);
            }
            juce::MidiBuffer midi;
            proc.processBlock (buf, midi);
            for (int n = 0; n < blockSize; ++n)
                out.push_back (buf.getSample (0, n));
        }

        double sumE = 0.0, sumWE = 0.0;
        for (size_t i = 0; i < out.size(); ++i)
        {
            const double e = (double) out[i] * out[i];
            sumE  += e;
            sumWE += e * (double) i;
        }
        return sumE > 0.0
                   ? ((sumWE / sumE) - (double) burstStart) * 1000.0 / sr
                   : 0.0;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    int failures = 0;

    // --- (1) MIX = 1, FX SOURCE = OUT: wet-only, no dry leak at lag 0 --------
    {
        const double rms = firstBlockRms (/*fxSource*/ 1);
        std::cout << "MIX=1, FX SOURCE=OUT first-block rms=" << rms
                  << " (wet-only => silent)" << std::endl;
        if (rms > 1.0e-6)
        {
            ++failures;
            std::cout << "FAIL: dry signal leaks into the MIX=1 output" << std::endl;
        }
    }

    // --- (2) The cloud must lag the input (granular look-back) ---------------
    {
        const double centroidMs = measureCentroidMs (0, 200.0f);
        std::cout << "cloud energy centroid=" << centroidMs
                  << " ms after the input" << std::endl;
        if (centroidMs <= 1.0)
        {
            ++failures;
            std::cout << "FAIL: wet path is not delayed as expected" << std::endl;
        }
    }

    // --- (3) FX SOURCE = BOTH routes the dry through the effects -------------
    {
        const double rms = firstBlockRms (/*fxSource*/ 2);
        std::cout << "MIX=1, FX SOURCE=BOTH first-block rms=" << rms
                  << " (dry routed through the effects)" << std::endl;
        if (rms <= 1.0e-6)
        {
            ++failures;
            std::cout << "FAIL: BOTH should feed the dry through the effects"
                      << std::endl;
        }
    }

    if (failures > 0)
    {
        std::cout << "FAIL: MIX=1 / wet-path invariants broken" << std::endl;
        return 1;
    }

    std::cout << "PASS: MIX=1 wet-path invariants hold" << std::endl;
    return 0;
}
