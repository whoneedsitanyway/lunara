// Temporary diagnostic: hunt for a CloudReverb instability with sustained
// periodic input at extreme settings (freeze feeds a held tone into it).

#include <juce_audio_basics/juce_audio_basics.h>
#include <iostream>
#include <cmath>
#include <vector>
#include <random>

#include "../Source/dsp/CloudReverb.h"

int main()
{
    const double sr = 48000.0;
    const int blk = 512;

    // Excite with noise for a few seconds, then feed SILENCE and watch the tail.
    // This mirrors the black-box log: in=0, grain=0, dly=0, yet rev explodes.
    lunara::CloudReverb r; r.prepare (sr);
    juce::AudioBuffer<float> b (2, blk);

    std::mt19937 rng (99);
    std::uniform_real_distribution<float> dist (-0.3f, 0.3f);

    const int exciteBlocks = (int) (5.0 * sr / blk);
    const int silenceBlocks = (int) (120.0 * sr / blk);

    double peak = 0.0;
    for (int k = 0; k < exciteBlocks + silenceBlocks; ++k)
    {
        const bool silent = k >= exciteBlocks;
        for (int c = 0; c < 2; ++c)
        {
            auto* p = b.getWritePointer (c);
            for (int i = 0; i < blk; ++i)
                p[i] = silent ? 0.0f : dist (rng);
        }
        r.process (b.getWritePointer (0), b.getWritePointer (1), blk,
                   0.6f, 1.0f, 0.5f, 0.4f, 0.53f, 0.85f);

        for (int c = 0; c < 2; ++c)
        {
            const float* p = b.getReadPointer (c);
            for (int i = 0; i < blk; ++i)
                peak = std::max (peak, (double) std::abs (p[i]));
        }

        if (k % (int) (5.0 * sr / blk) == 0)
            std::cout << (silent ? "silence " : "excite  ")
                      << "t=" << (k * blk / sr) << "s peak=" << peak << std::endl;
    }

    std::cout << "FINAL peak=" << peak << std::endl;
    return peak > 10.0 ? 1 : 0;
}
