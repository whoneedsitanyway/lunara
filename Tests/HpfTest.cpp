// Offline test for the HPF (low-cut) knob.
//
// The cutoff defaults to 100 Hz and is continuously adjustable (20..500 Hz).
// With MIX fully dry the output equals the input (through the HPF), so feeding
// a low sine makes the effect easy to observe: at 100 Hz a 30 Hz tone is
// strongly attenuated, at the minimum (20 Hz) it passes almost untouched, and a
// 4 kHz tone passes in both cases.

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

    double measureRms (LunaraAudioProcessor& proc, juce::AudioBuffer<float>& buf,
                       double freq, int blocks, int blockSize)
    {
        double sumSq = 0.0; long count = 0;
        juce::MidiBuffer midi;
        const double w = 2.0 * juce::MathConstants<double>::pi * freq
                         / proc.getSampleRate();
        long phase = 0;

        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                auto* p = buf.getWritePointer (ch);
                for (int n = 0; n < blockSize; ++n)
                    p[n] = (float) (0.3 * std::sin (w * (double) (phase + n)));
            }
            phase += blockSize;

            proc.processBlock (buf, midi);

            if (b > blocks / 2)
                for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                {
                    const float* p = buf.getReadPointer (ch);
                    for (int n = 0; n < blockSize; ++n)
                    { sumSq += (double) p[n] * p[n]; ++count; }
                }
        }
        return count > 0 ? std::sqrt (sumSq / (double) count) : 0.0;
    }
}

int main()
{
    const double sr = 48000.0;
    const int blockSize = 512;

    LunaraAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, blockSize);

    // --- the knob defaults to 100 Hz ----------------------------------------
    const float defHz = proc.apvts.getRawParameterValue ("hpf")->load();
    std::cout << "default HPF = " << defHz << " Hz" << std::endl;
    const bool defaultOk = std::abs (defHz - 100.0f) < 0.5f;

    // Listen fully dry: the output tracks the input through the HPF, so its
    // effect is unambiguous.
    setParam (proc, "mix", 0.0f);
    setParam (proc, "autoGain", 0.0f);
    setParam (proc, "output", 0.0f);
    setParam (proc, "limiter", 0.0f);

    juce::AudioBuffer<float> buf (2, blockSize);

    auto run = [&] (float hpfHz, double freq)
    {
        setParam (proc, "hpf", hpfHz);
        proc.prepareToPlay (sr, blockSize);   // snap the filter state
        return measureRms (proc, buf, freq, 200, blockSize);
    };

    const double lowOpen  = run (20.0f,  30.0);    // knob at minimum: nearly open
    const double lowCut   = run (100.0f, 30.0);    // default: strong low cut
    const double highOpen = run (20.0f,  4000.0);
    const double highCut  = run (100.0f, 4000.0);

    std::cout << "30Hz:  cut@20=" << lowOpen  << " cut@100=" << lowCut  << "\n"
              << "4kHz:  cut@20=" << highOpen << " cut@100=" << highCut << std::endl;

    const bool lowCutWorks = lowCut < lowOpen * 0.35;            // clearly cut
    const bool highKept    = std::abs (highCut - highOpen) < highOpen * 0.15;

    if (! defaultOk || ! lowCutWorks || ! highKept)
    {
        std::cout << "FAIL: HPF knob did not cut lows / keep highs / default 100 Hz"
                  << std::endl;
        return 1;
    }

    std::cout << "PASS: HPF knob defaults to 100 Hz, cuts the low end and keeps the highs"
              << std::endl;
    return 0;
}
