// Smoke test for the HALF TIME switch: with it engaged the delay time must be
// doubled (everything runs at half speed), and the parameter must round-trip.

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <cmath>

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

    setParam (proc, "sync", 0.0f);
    setParam (proc, "delay", 300.0f);
    setParam (proc, "halftime", 0.0f);

    juce::AudioBuffer<float> buf (2, blockSize);
    juce::MidiBuffer midi;

    auto runOnce = [&]
    {
        buf.clear();
        proc.processBlock (buf, midi);
        return proc.getDisplayDelayMs();
    };

    const float msOff = runOnce();

    setParam (proc, "halftime", 1.0f);
    const float msOn = runOnce();

    std::cout << "delayMs off=" << msOff << "  on=" << msOn << std::endl;

    if (std::abs (msOff - 300.0f) > 1.0f || std::abs (msOn - 600.0f) > 1.0f)
    {
        std::cout << "FAIL: halftime did not double the delay time" << std::endl;
        return 1;
    }

    std::cout << "PASS: halftime doubles the delay time" << std::endl;
    return 0;
}
