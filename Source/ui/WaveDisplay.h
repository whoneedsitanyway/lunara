#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "LunaraLookAndFeel.h"

namespace lunara
{

//==============================================================================
//  Oscilloscope + gain-reduction meter for the bottom of the pedal.
//
//  Original signal (before the plugin) is drawn in the accent colour; the
//  processed output is drawn as a semi-transparent trace on top.  A slim meter
//  on the right shows the limiter's gain reduction and lights up red while the
//  limiter is actively reducing gain.  Also shows the current display refresh
//  rate (FPS).
//
//  Reads a decimated min/max envelope from the processor. Buffers are
//  pre-allocated so painting never allocates (keeps the display smooth).
//==============================================================================
class WaveDisplay : public juce::Component,
                    private juce::Timer
{
public:
    explicit WaveDisplay (LunaraAudioProcessor& p);
    ~WaveDisplay() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    void zoomIn();
    void zoomOut();
    void ensureBuffers (int buckets);

    LunaraAudioProcessor& proc;

    std::vector<float> minIn, maxIn, minOut, maxOut;
    int allocatedBuckets = 0;

    // Reused across paints so the 90 Hz repaint never allocates.
    juce::Path tracePathIn, tracePathOut;

    static constexpr int kNumLevels = 4;
    static constexpr float kBarLevels[kNumLevels] = { 0.5f, 1.0f, 2.0f, 4.0f };
    int zoomLevel = 3;   // default = 4 bars

    float meterGrDb = 0.0f;

    double lastPaintMs = 0.0;
    float  fpsSmooth = 60.0f;

    juce::TextButton zoomOutButton { "-" };
    juce::TextButton zoomInButton  { "+" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveDisplay)
};

} // namespace lunara
