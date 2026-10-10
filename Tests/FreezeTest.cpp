// Offline test for the FREEZE control of the granular engine.
//
// Freeze holds the circular buffer: while frozen the engine keeps reading the
// captured moment, so the cloud sustains even when the input goes silent. With
// freeze off the same silent input is recorded, so the cloud decays away.
//
// The test fills the buffer with a steady tone, then feeds silence - first with
// freeze on (the tail must sustain) and then with freeze off (the tail must
// decay).

#include <juce_audio_basics/juce_audio_basics.h>
#include <iostream>
#include <cmath>

#include "../Source/dsp/GrainEngine.h"

namespace
{
    float blockRms (const juce::AudioBuffer<float>& b, int numSamples)
    {
        double sum = 0.0; long count = 0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            const float* p = b.getReadPointer (ch);
            for (int n = 0; n < numSamples; ++n)
            { sum += (double) p[n] * p[n]; ++count; }
        }
        return count > 0 ? (float) std::sqrt (sum / (double) count) : 0.0f;
    }

    // Fills the buffer with a tone, then feeds `silenceSeconds` of silence with
    // the given freeze state and returns the RMS of the tail (last quarter).
    float runTail (bool freeze, double sampleRate, int block)
    {
        lunara::GrainEngine grain;
        grain.prepare (sampleRate, block, 2);

        lunara::GrainParams p;
        p.grainSizeMs   = 200.0f;
        p.density       = 30.0f;
        p.spray         = 0.25f;
        p.jitter        = 0.0f;
        p.reverseProb   = 0.0f;
        p.stereoSpread  = 0.4f;
        p.pitchSemis    = 0.0f;
        p.chaos         = 0.0f;
        p.drift         = 0.0f;

        juce::AudioBuffer<float> in (2, block), out (2, block);

        // 1) Fill the whole buffer (2.5 s) with a steady tone.
        p.frozen = false;
        const double w = 2.0 * juce::MathConstants<double>::pi * 440.0 / sampleRate;
        long phase = 0;
        const int fillBlocks = (int) (3.0 * sampleRate / block);
        for (int b = 0; b < fillBlocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* q = in.getWritePointer (ch);
                for (int n = 0; n < block; ++n)
                    q[n] = (float) (0.3 * std::sin (w * (double) (phase + n)));
            }
            phase += block;
            grain.process (in.getReadPointer (0), in.getReadPointer (1),
                           out.getWritePointer (0), out.getWritePointer (1), block, p);
        }

        // 2) Feed silence long enough to fully overwrite the buffer (2.5 s) and
        //    measure the tail.
        p.frozen = freeze;
        float tailRms = 0.0f;
        const int silenceBlocks = (int) (4.0 * sampleRate / block);
        for (int b = 0; b < silenceBlocks; ++b)
        {
            in.clear();
            grain.process (in.getReadPointer (0), in.getReadPointer (1),
                           out.getWritePointer (0), out.getWritePointer (1), block, p);
            if (b > silenceBlocks * 3 / 4)   // last second: the buffer is all silence
                tailRms = juce::jmax (tailRms, blockRms (out, block));
        }
        return tailRms;
    }
}

int main()
{
    const double sr = 48000.0;
    const int block = 512;

    const float frozenTail = runTail (true,  sr, block);
    const float openTail   = runTail (false, sr, block);

    std::cout << "frozen tail RMS=" << frozenTail
              << "  unfrozen tail RMS=" << openTail << std::endl;

    // Frozen: the cloud must keep sounding on silence.
    const bool freezeHolds = frozenTail > 0.01f;

    // Not frozen: the same silence must let the cloud decay away.
    const bool openDecays  = openTail < frozenTail * 0.25f;

    if (! freezeHolds || ! openDecays)
    {
        std::cout << "FAIL: freeze did not hold the cloud (or it did not decay when open)"
                  << std::endl;
        return 1;
    }

    std::cout << "PASS: FREEZE holds the granular cloud on silence; without it the cloud decays"
              << std::endl;
    return 0;
}
