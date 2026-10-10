#include "WaveDisplay.h"

namespace lunara
{

WaveDisplay::WaveDisplay (LunaraAudioProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (true, true);
    startTimeMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (90);   // vsync caps it (60 on standard displays, up to 90 on ProMotion)

    // The zoom buttons sit on the always-dark oscilloscope screen, so they use
    // an explicit dark background + light text (independent of the UI theme).
    auto styleSmall = [] (juce::TextButton& b, const juce::String& tip)
    {
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1d1a17));
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::orange);
        b.setTooltip (tip);
        b.setConnectedEdges (0);
    };

    styleSmall (zoomOutButton, "Zoom out (show more time)");
    styleSmall (zoomInButton,  "Zoom in (show less time)");

    zoomOutButton.onClick = [this] { zoomOut(); };
    zoomInButton.onClick  = [this] { zoomIn(); };

    addAndMakeVisible (zoomOutButton);
    addAndMakeVisible (zoomInButton);
}

WaveDisplay::~WaveDisplay() { stopTimer(); }

//==============================================================================
void WaveDisplay::visibilityChanged()
{
    // Skip all repaint work entirely while hidden (e.g. compact mode).
    if (isVisible())
        startTimerHz (90);
    else
        stopTimer();
}

//==============================================================================
void WaveDisplay::zoomIn()  { zoomLevel = juce::jlimit (0, kNumLevels - 1, zoomLevel - 1); repaint(); }
void WaveDisplay::zoomOut() { zoomLevel = juce::jlimit (0, kNumLevels - 1, zoomLevel + 1); repaint(); }

void WaveDisplay::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (w.deltaY > 0.0f)      zoomIn();
    else if (w.deltaY < 0.0f) zoomOut();
}

void WaveDisplay::ensureBuffers (int buckets)
{
    if (buckets <= allocatedBuckets)
        return;
    allocatedBuckets = buckets;
    minIn.assign ((size_t) buckets, 0.0f);
    maxIn.assign ((size_t) buckets, 0.0f);
    minOut.assign ((size_t) buckets, 0.0f);
    maxOut.assign ((size_t) buckets, 0.0f);
}

void WaveDisplay::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto bottom = r.removeFromBottom (20);

    zoomOutButton.setBounds (bottom.removeFromLeft (26).reduced (0, 1));
    bottom.removeFromLeft (3);
    zoomInButton.setBounds (bottom.removeFromLeft (26).reduced (0, 1));
}

void WaveDisplay::timerCallback() { repaint(); }

//==============================================================================
void WaveDisplay::paint (juce::Graphics& g)
{
    const auto accent    = findColour (LunaraLookAndFeel::accentColourId);
    const auto accentDim = findColour (LunaraLookAndFeel::accentDimColourId);
    const auto cool      = findColour (LunaraLookAndFeel::coolColourId);
    const auto textDim   = findColour (LunaraLookAndFeel::textDimColourId);

    // FPS measurement.
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (lastPaintMs > 0.0)
    {
        const double dt = now - lastPaintMs;
        if (dt > 0.0)
            fpsSmooth = fpsSmooth * 0.9f + (float) (1000.0 / dt) * 0.1f;
    }
    lastPaintMs = now;

    auto bounds = getLocalBounds().toFloat();
    const auto panelRect = bounds.reduced (0.5f);

    // Recessed screen (kept dark in both themes - like a real display).
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (panelRect, 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.72f));
    g.fillRoundedRectangle (panelRect.reduced (3.0f), 6.0f);
    g.setColour (cool.withAlpha (0.18f));
    g.drawRoundedRectangle (panelRect, 8.0f, 1.0f);

    auto inner = panelRect.reduced (8.0f, 6.0f);
    if (inner.getWidth() < 40.0f || inner.getHeight() < 24.0f)
        return;

    // Reserve the right edge for the gain-reduction meter.
    auto meterArea = inner.removeFromRight (14.0f);
    inner.removeFromRight (6.0f);

    const float centreY = inner.getCentreY();
    const float halfH   = inner.getHeight() * 0.5f - 1.0f;

    // Faint grid.
    g.setColour (accentDim.withAlpha (0.18f));
    g.drawHorizontalLine ((int) centreY, inner.getX(), inner.getRight());
    for (int i = 1; i < 4; ++i)
    {
        const float x = inner.getX() + inner.getWidth() * (float) i / 4.0f;
        g.setColour (accentDim.withAlpha (0.07f));
        g.drawVerticalLine ((int) x, inner.getY(), inner.getBottom());
    }

    const double elapsed = now - startTimeMs;
    const bool warming = elapsed < kWarmUpMs;
    const int numBuckets = proc.getScopeNumBuckets();
    const int w = (int) inner.getWidth();

    if (! warming && numBuckets > 0)
    {
        const float bpm = juce::jmax (20.0f, proc.getDisplayBpm());
        const double barSeconds = 4.0 * 60.0 / (double) bpm;
        const double windowSeconds = (double) kBarLevels[zoomLevel] * barSeconds;
        const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;

        const double bucketSeconds = (double) LunaraAudioProcessor::kScopeDecimate / sr;
        const int bucketsToShow = juce::jlimit (1, numBuckets,
                                                (int) (windowSeconds / bucketSeconds));

        ensureBuffers (bucketsToShow);
        proc.copyScopeBuckets (minIn.data(), maxIn.data(), minOut.data(), maxOut.data(),
                               bucketsToShow);

        const float dispGain = 1.8f;

        auto drawTrace = [&] (juce::Path& path, const std::vector<float>& mn,
                              const std::vector<float>& mx,
                              juce::Colour colour, float thickness)
        {
            path.clear();
            for (int x = 0; x < w; ++x)
            {
                const int b0 = (int) ((int64_t) x * bucketsToShow / w);
                const int b1 = juce::jmax (b0 + 1, (int) ((int64_t) (x + 1) * bucketsToShow / w));

                float lo = 1.0e9f, hi = -1.0e9f;
                for (int b = b0; b < b1 && b < bucketsToShow; ++b)
                {
                    lo = juce::jmin (lo, mn[(size_t) b]);
                    hi = juce::jmax (hi, mx[(size_t) b]);
                }

                lo = juce::jlimit (-1.0f, 1.0f, lo * dispGain);
                hi = juce::jlimit (-1.0f, 1.0f, hi * dispGain);

                const float px = inner.getX() + (float) x;
                path.startNewSubPath (px, centreY - hi * halfH);
                path.lineTo (px, centreY - lo * halfH);
            }

            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::butt));
        };

        drawTrace (tracePathIn,  minIn,  maxIn,  accent, 1.2f);
        drawTrace (tracePathOut, minOut, maxOut, juce::Colours::white.withAlpha (0.30f), 1.6f);
    }
    else if (warming)
    {
        g.setColour (textDim.withAlpha (0.7f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("warming up...", inner, juce::Justification::centred, false);
    }

    // ---- gain-reduction / limiting meter -----------------------------------
    {
        const float grDb = proc.getDisplayGrDb();
        const bool  limiting = proc.getDisplayLimiting();
        meterGrDb = juce::jmax (grDb, meterGrDb - 1.2f);

        auto mr = meterArea.reduced (0.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (mr, 3.0f);
        g.setColour (accentDim.withAlpha (0.5f));
        g.drawRoundedRectangle (mr, 3.0f, 1.0f);

        const float norm = juce::jlimit (0.0f, 1.0f, meterGrDb / 24.0f);
        auto fill = mr.reduced (2.0f);
        const float fh = fill.getHeight() * norm;
        auto fillBar = fill.removeFromBottom (fh);

        g.setColour (limiting ? juce::Colours::red.withAlpha (0.9f) : accent.withAlpha (0.85f));
        g.fillRoundedRectangle (fillBar, 2.0f);

        auto led = meterArea.withSizeKeepingCentre (7.0f, 7.0f).withY (meterArea.getY() + 1.0f);
        g.setColour (limiting ? juce::Colours::red : juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (led);
    }

    // ---- readouts ----------------------------------------------------------
    const bool synced = proc.apvts.getRawParameterValue (ids::sync) != nullptr
                        && proc.apvts.getRawParameterValue (ids::sync)->load() > 0.5f;
    juce::String div = "FREE";
    if (synced)
        if (auto* p = proc.apvts.getParameter (ids::division))
            div = p->getCurrentValueAsText();

    const float ms = proc.getDisplayDelayMs();
    const juce::String timing =
        juce::String (proc.getDisplayBpm(), 1) + " BPM   |   " + div + "   |   "
        + (ms >= 1000.0f ? juce::String (ms / 1000.0f, 2) + " s"
                         : juce::String (ms, 1) + " ms");

    auto textRow = inner.removeFromTop (14.0f);
    g.setColour (cool.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    g.drawText (timing, textRow.withTrimmedRight (150.0f), juce::Justification::centredLeft, false);

    // FPS + zoom level, right aligned.
    const float bars = kBarLevels[zoomLevel];
    const juce::String barsText = (bars < 1.0f) ? juce::String (bars, 2) + " bar"
                                                : juce::String ((int) bars) + " bar";
    g.setColour (accentDim);
    g.drawText (barsText, textRow.withTrimmedRight (62.0f), juce::Justification::centredRight, false);

    g.setColour (textDim.withAlpha (0.75f));
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.drawText (juce::String ((int) std::round (fpsSmooth)) + " fps",
                textRow, juce::Justification::centredRight, false);

    g.setColour (accent.withAlpha (0.9f));
    g.setFont (juce::Font (juce::FontOptions (9.0f).withStyle ("Bold")));
    g.drawText ("IN", inner.withTrimmedTop (1.0f).removeFromTop (12.0f),
                juce::Justification::centredLeft, false);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawText ("OUT", inner.withTrimmedTop (1.0f).removeFromTop (12.0f),
                juce::Justification::centredRight, false);
}

} // namespace lunara
