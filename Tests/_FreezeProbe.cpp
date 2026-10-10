// Temporary diagnostic: run the real processor with FREEZE and watch the
// per-stage levels for a runaway (mirrors the black-box log).

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <cmath>
#include <random>

#include "../Source/PluginProcessor.h"

static void setParam (LunaraAudioProcessor& proc, const char* id, float real)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (real));
}

int main()
{
    const double sr = 48000.0;
    const int blockSize = 512;

    LunaraAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, blockSize);
    proc.prepareToPlay (sr, blockSize);

    juce::AudioBuffer<float> buf (2, blockSize);
    juce::MidiBuffer midi;

    std::mt19937 rng (2026);
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);

    // Long simulated session: evolving input (bursts + silence + transients)
    // and random parameter changes, watching for the CloudReverb blowup.
    const int totalBlocks = 5000000;   // ~15 hours at 48k/512
    float globalPeak = 0.0f;
    int bursts = 0;

    for (int b = 0; b < totalBlocks; ++b)
    {
        // Rapid random parameter changes (like RANDOM / user tweaks).
        if (b % 20 == 0)
        {
            auto rnd = [&] (const char* id, float lo, float hi)
            { setParam (proc, id, lo + uni (rng) * (hi - lo)); };
            rnd ("feedback", 0.3f, 0.95f);
            rnd ("reverb",   0.0f, 1.0f);
            rnd ("decay",    0.0f, 1.0f);
            rnd ("cloud",    0.0f, 1.0f);
            rnd ("size",     20.0f, 500.0f);
            rnd ("density",  1.0f, 120.0f);
            rnd ("spray",    0.0f, 1.0f);
            rnd ("pitch",   -12.0f, 12.0f);
            rnd ("mix",      0.0f, 1.0f);
            rnd ("hpf",      20.0f, 500.0f);
            rnd ("tone",     200.0f, 18000.0f);
            rnd ("delay",    20.0f, 3900.0f);
            rnd ("pingpong", 0.0f, 1.0f);
            setParam (proc, "mode", (float) (int) (uni (rng) * 6.0f));
            setParam (proc, "halfTime", uni (rng) < 0.5f ? 1.0f : 0.0f);
            setParam (proc, "autoGain", uni (rng) < 0.5f ? 1.0f : 0.0f);
            setParam (proc, "limiter",  uni (rng) < 0.8f ? 1.0f : 0.0f);
            setParam (proc, "freeze",   uni (rng) < 0.4f ? 1.0f : 0.0f);
            setParam (proc, "fxSource", (float) (int) (uni (rng) * 3.0f));
        }

        // Input: bursts of noise separated by silence, with loud transients.
        const bool burst = ((b / 40) % 3) != 0;   // ~2/3 of the time
        if (burst) ++bursts;
        for (int c = 0; c < 2; ++c)
        {
            auto* p = buf.getWritePointer (c);
            for (int n = 0; n < blockSize; ++n)
                p[n] = burst ? (uni (rng) * 2.0f - 1.0f) * 0.3f : 0.0f;
            if (b % 500 == 0) p[0] = 1.0f;   // transient
        }

        proc.processBlock (buf, midi);

        for (int c = 0; c < 2; ++c)
        {
            const float* p = buf.getReadPointer (c);
            for (int n = 0; n < blockSize; ++n)
                globalPeak = std::max (globalPeak, std::abs (p[n]));
        }
    }

    std::cout << "DONE blocks=" << totalBlocks << " globalPeak=" << globalPeak << std::endl;
    return 0;
}
