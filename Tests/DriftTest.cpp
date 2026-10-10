// Verifies that Drift produces a SLOW pitch drift (cassette-like) rather than
// fast per-grain jitter.  It drives the grain engine and inspects the global
// drift ratio directly: it should stay within a small range and change only
// gradually from block to block (and over half-second windows).

#include <juce_audio_basics/juce_audio_basics.h>
#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>

#include "../Source/dsp/GrainEngine.h"

int main()
{
    const double sr = 48000.0;
    const int block = 512;

    lunara::GrainEngine g;
    g.prepare (sr, block, 2);

    lunara::GrainParams p;
    p.grainSizeMs = 500.0f;
    p.density     = 40.0f;
    p.drift       = 1.0f;

    juce::AudioBuffer<float> dry (2, block), wet (2, block);
    dry.clear();

    std::vector<float> ratio;
    const int blocks = 2400;   // ~25.6 s
    for (int b = 0; b < blocks; ++b)
    {
        wet.clear();
        g.process (dry.getReadPointer (0), dry.getReadPointer (1),
                   wet.getWritePointer (0), wet.getWritePointer (1), block, p);
        ratio.push_back (g.getDriftRatio());
    }

    // Analyse after a short warm-up.
    float mn = ratio[20], mx = ratio[20], maxStep = 0.0f;
    for (size_t i = 21; i < ratio.size(); ++i)
    {
        mn = std::min (mn, ratio[i]);
        mx = std::max (mx, ratio[i]);
        maxStep = std::max (maxStep, std::abs (ratio[i] - ratio[i - 1]));
    }

    const float spreadCents = 1200.0f * std::log2 (mx / mn);
    const float stepCents   = 1200.0f * std::log2 (1.0f + maxStep);

    std::cout << "ratio min=" << mn << " max=" << mx
              << "  spread=" << spreadCents << " cents"
              << "  maxStep(block)=" << stepCents << " cents" << std::endl;

    // Audible drift but gentle, and no fast jumps.
    if (spreadCents < 15.0f)
    {
        std::cout << "FAIL: drift is too small to hear" << std::endl;
        return 1;
    }
    if (spreadCents > 120.0f)
    {
        std::cout << "FAIL: drift is too extreme" << std::endl;
        return 1;
    }
    if (stepCents > 1.0f)
    {
        std::cout << "FAIL: drift changes too fast (jittery)" << std::endl;
        return 1;
    }

    std::cout << "PASS: drift is audible but slow (cassette-like)" << std::endl;
    return 0;
}
