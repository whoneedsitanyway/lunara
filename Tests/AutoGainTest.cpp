// Offline test for the output Auto Gain: with it on, the processed output RMS
// should match the input RMS (so the effect does not "drop" in level); with it
// off, the output should be quieter than the input.

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <cmath>
#include <random>

#include "../Source/PluginProcessor.h"

namespace
{
    double measureRms (LunaraAudioProcessor& proc, juce::AudioBuffer<float>& buf,
                       int blocks, int blockSize, double inputRmsTarget)
    {
        std::mt19937 rng (999);
        std::normal_distribution<float> nd (0.0f, 1.0f);

        double sumSq = 0.0; long count = 0;
        juce::MidiBuffer midi;

        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                auto* p = buf.getWritePointer (ch);
                for (int n = 0; n < blockSize; ++n)
                    p[n] = nd (rng) * (float) inputRmsTarget;
            }

            proc.processBlock (buf, midi);

            // Only measure after the chain has settled.
            if (b > blocks * 3 / 4)
                for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                {
                    const float* p = buf.getReadPointer (ch);
                    for (int n = 0; n < blockSize; ++n)
                    { sumSq += (double) p[n] * p[n]; ++count; }
                }
        }

        return count > 0 ? std::sqrt (sumSq / (double) count) : 0.0;
    }

    void setParam (LunaraAudioProcessor& proc, const char* id, float real)
    {
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (real));
    }
}

int main()
{
    const double sr = 48000.0;
    const int blockSize = 512;
    const double inputRms = 0.20;

    LunaraAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, blockSize);
    proc.prepareToPlay (sr, blockSize);

    // A setting that clearly loses level: heavy wet, reverb, long grains.
    setParam (proc, "mix", 0.85f);
    setParam (proc, "reverb", 0.8f);
    setParam (proc, "decay", 0.7f);
    setParam (proc, "cloud", 0.7f);
    setParam (proc, "size", 300.0f);
    setParam (proc, "density", 40.0f);
    setParam (proc, "output", 0.0f);

    juce::AudioBuffer<float> buf (2, blockSize);

    // Auto gain ON.
    setParam (proc, "autoGain", 1.0f);
    proc.prepareToPlay (sr, blockSize);
    const double rmsOn = measureRms (proc, buf, 600, blockSize, inputRms);

    // Auto gain OFF.
    setParam (proc, "autoGain", 0.0f);
    proc.prepareToPlay (sr, blockSize);
    const double rmsOff = measureRms (proc, buf, 600, blockSize, inputRms);

    // With auto gain ON, the OUTPUT knob must be ignored (they cannot fight).
    setParam (proc, "autoGain", 1.0f);
    setParam (proc, "output", 12.0f);
    proc.prepareToPlay (sr, blockSize);
    const double rmsOnOutputUp = measureRms (proc, buf, 600, blockSize, inputRms);

    setParam (proc, "output", -24.0f);
    proc.prepareToPlay (sr, blockSize);
    const double rmsOnOutputDown = measureRms (proc, buf, 600, blockSize, inputRms);

    std::cout << "inputRms=" << inputRms << "  rmsOn=" << rmsOn
              << "  rmsOff=" << rmsOff
              << "  rmsOn+12dB=" << rmsOnOutputUp
              << "  rmsOn-24dB=" << rmsOnOutputDown << std::endl;

    // With auto gain the output should be close to the input (within ~3 dB).
    const bool onOk  = std::abs (rmsOn - inputRms) < inputRms * 0.45;
    // Without it the effect should be noticeably quieter.
    const bool offOk = rmsOff < rmsOn * 0.9;
    // While on, the OUTPUT knob must have (almost) no effect.
    const bool outputIgnored = std::abs (rmsOnOutputUp - rmsOnOutputDown) < inputRms * 0.15;

    if (! onOk || ! offOk || ! outputIgnored)
    {
        std::cout << "FAIL: auto gain did not match the level / OUTPUT not ignored" << std::endl;
        return 1;
    }

    std::cout << "PASS: auto gain matches the input level and ignores OUTPUT" << std::endl;
    return 0;
}
