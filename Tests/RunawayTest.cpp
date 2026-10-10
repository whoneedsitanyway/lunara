// Verifies (a) the output can never exceed full scale even at extreme settings
// with Auto Gain + a hot output, and (b) the black-box logger captures an
// anomaly and writes it to disk.

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <cmath>
#include <random>

#include "../Source/PluginProcessor.h"
#include "../Source/DebugLogger.h"

static void setParam (LunaraAudioProcessor& proc, const char* id, float real)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (real));
}

// Runs the processor on hot noise and returns the worst |output| seen.
static double runWorst (LunaraAudioProcessor& proc, juce::AudioBuffer<float>& buf,
                        int blocks, int blockSize, double inputRms)
{
    std::mt19937 rng (4242);
    std::normal_distribution<float> nd (0.0f, 1.0f);
    juce::MidiBuffer midi;

    double worst = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            auto* p = buf.getWritePointer (ch);
            for (int n = 0; n < blockSize; ++n)
                p[n] = nd (rng) * (float) inputRms;
        }

        proc.processBlock (buf, midi);

        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* p = buf.getReadPointer (ch);
            for (int n = 0; n < blockSize; ++n)
            {
                if (! std::isfinite (p[n])) return 1.0e9;
                worst = std::max (worst, (double) std::abs (p[n]));
            }
        }
    }
    return worst;
}

int main()
{
    const double sr = 48000.0;
    const int blockSize = 512;

    LunaraAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, blockSize);
    proc.prepareToPlay (sr, blockSize);

    // Deliberately brutal settings that previously could blast the output.
    setParam (proc, "sync", 0.0f);
    setParam (proc, "delay", 30.0f);
    setParam (proc, "feedback", 0.95f);
    setParam (proc, "tone", 18000.0f);
    setParam (proc, "reverb", 1.0f);
    setParam (proc, "decay", 1.0f);
    setParam (proc, "cloud", 1.0f);
    setParam (proc, "mix", 1.0f);
    setParam (proc, "hpf", 20.0f);        // knob at minimum: keep the low end in
    setParam (proc, "size", 500.0f);
    setParam (proc, "density", 100.0f);
    setParam (proc, "spray", 1.0f);
    setParam (proc, "chaos", 1.0f);
    setParam (proc, "mode", 4.0f);        // Nebula (max reverb)

    juce::AudioBuffer<float> buf (2, blockSize);

    // Auto Gain ON, output pushed to +12 (ignored while Auto Gain is on).
    setParam (proc, "autoGain", 1.0f);
    setParam (proc, "output", 12.0f);
    proc.prepareToPlay (sr, blockSize);
    const double worstAg = runWorst (proc, buf, 400, blockSize, 0.3);

    // Auto Gain OFF, output +12 dB (limiter must still catch everything).
    setParam (proc, "autoGain", 0.0f);
    proc.prepareToPlay (sr, blockSize);
    const double worstOut = runWorst (proc, buf, 400, blockSize, 0.3);

    // Limiter OFF: the output stage is bypassed, but the final safety clamp must
    // still keep the output bounded (never above full scale).
    setParam (proc, "limiter", 0.0f);
    proc.prepareToPlay (sr, blockSize);
    const double worstNoLim = runWorst (proc, buf, 400, blockSize, 0.3);

    std::cout << "worst |out|: autoGain=" << worstAg << "  manual+12dB=" << worstOut
              << "  limiterOff=" << worstNoLim << std::endl;

    const double kCeil = 1.0001;
    if (worstAg > kCeil || worstOut > kCeil || worstNoLim > kCeil)
    {
        std::cout << "FAIL: output exceeded full scale (limiter no longer last)" << std::endl;
        return 1;
    }

    // --- black-box logger ----------------------------------------------------
    auto& logger = proc.getDebugLogger();
    logger.clearAnomaly();
    logger.clearLog();

    lunara::BlockSnapshot s;
    s.timeSec  = 12.0;
    s.mixPeak  = 9.0f;
    s.outPeak  = 0.9f;
    s.feedback = 0.95f;
    s.flags    = lunara::kFlagMixHot;
    logger.push (s);

    // Give the writer thread a moment to drain and flush.
    juce::Thread::sleep (400);

    const bool captured = logger.hasAnomaly();
    const auto logText = logger.getLogFile().existsAsFile()
                             ? logger.getLogFile().loadFileAsString()
                             : juce::String();

    std::cout << "logger: captured=" << (captured ? "yes" : "no")
              << "  fileHasAnomaly=" << (logText.contains ("ANOMALY") ? "yes" : "no")
              << "  file=" << logger.getLogFile().getFullPathName() << std::endl;

    if (! captured || ! logText.contains ("ANOMALY"))
    {
        std::cout << "FAIL: black-box logger did not record the anomaly" << std::endl;
        return 1;
    }

    std::cout << "PASS: output stays bounded and the black-box logger captures anomalies" << std::endl;
    return 0;
}
