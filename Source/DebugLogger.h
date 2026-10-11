#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>

namespace lunara
{

//==============================================================================
//  Lightweight "black box" recorder for hunting rare, hard-to-reproduce audio
//  bugs (e.g. a sudden runaway / squeal).  The audio thread only pushes small
//  POD snapshots into a lock-free FIFO; a background thread drains it, keeps a
//  rolling history and appends to a log file.  When an anomaly flag is set the
//  whole recent history is dumped, so we can see exactly what led up to it.
//
//  The audio-thread side never allocates, never blocks and never touches disk.
//==============================================================================
struct BlockSnapshot
{
    double timeSec = 0.0;

    // Per-stage peak magnitudes (post each processing stage).
    float inPeak     = 0.0f;
    float grainPeak  = 0.0f;
    float delayPeak  = 0.0f;
    float reverbPeak = 0.0f;
    float mixPeak    = 0.0f;   // dry/wet blend, into auto-gain + limiter
    float preLimPeak = 0.0f;   // after auto-gain / output, before the limiter
    float outPeak    = 0.0f;   // final output

    float autoGain   = 1.0f;   // current auto-gain multiplier
    float autoTarget = 1.0f;   // requested multiplier (before smoothing/clamp)
    float grDb       = 0.0f;   // limiter gain reduction

    // Key parameters in effect for this block.
    float mix = 0.0f, feedback = 0.0f, tone = 0.0f, decay = 0.0f, send = 0.0f,
          cloud = 0.0f, hpf = 0.0f, output = 0.0f, pitch = 0.0f, size = 0.0f,
          density = 0.0f, spray = 0.0f, chaos = 0.0f;

    int    mode     = 0;
    bool   halfTime = false;
    bool   autoGainOn = false;
    bool   frozen   = false;
    bool   reverseOn = false;   // REVERSE switch engaged
    bool   extreme   = false;   // STRETCH switch engaged (any non-zero amount)
    float  extremeAmt = 0.0f;   // smoothed STRETCH amount (0..1)

    unsigned flags  = 0;
};

enum SnapshotFlags : unsigned
{
    kFlagNonFinite   = 1u << 0,   // NaN / Inf anywhere in the chain
    kFlagInputHot    = 1u << 1,   // input peak > 2.0
    kFlagMixHot      = 1u << 2,   // pre-limiter peak > 4.0 (runaway)
    kFlagOutHot      = 1u << 3,   // final output peak > 1.5 (protection failed)
    kFlagAutoGainMax = 1u << 4,   // auto-gain clamped (asked for > +18 dB)
    kFlagHeavyGR     = 1u << 5    // limiter reducing more than 24 dB
};

class DebugLogger : private juce::Thread
{
public:
    DebugLogger();
    ~DebugLogger() override;

    void start();
    void stop();

    // ---- audio thread -------------------------------------------------------
    // Records one block. Lock-free and allocation-free; safe on the RT thread.
    void push (const BlockSnapshot& s) noexcept;

    // ---- message thread -----------------------------------------------------
    juce::File getLogFile() const;

    bool     hasAnomaly()        const noexcept { return anomalyCount.load (std::memory_order_relaxed) > 0; }

    void clearAnomaly() noexcept { anomalyCount.store (0, std::memory_order_relaxed);
                                   lastFlags.store (0, std::memory_order_relaxed); }
    void clearLog();

    static juce::String describeFlags (unsigned flags);

private:
    void run() override;
    void drain();
    void appendLine (const juce::String& line);
    static juce::String formatLine (const BlockSnapshot& s, bool anomaly);
    static juce::String timestamp();

    static constexpr int kFifoSize = 8192;
    static constexpr int kHistory  = 256;
    static constexpr int kMaxLogBytes = 4 * 1024 * 1024;

    // When the same set of flags keeps firing block after block (e.g. a benign
    // AUTOGAIN_MAX while the input is silent) we must not dump the whole history
    // every block: that floods the log. Repeats are coalesced into a counter and
    // only re-logged when the flags change or this cooldown has elapsed.
    static constexpr double kAnomalyCooldownSec = 5.0;

    juce::AbstractFifo fifo { kFifoSize };
    std::array<BlockSnapshot, kFifoSize> fifoData;

    // Writer-thread only:
    std::array<BlockSnapshot, kHistory> history {};
    int    historyCount = 0;
    int    historyWrite = 0;
    double lastHeartbeatSec = -1000.0;
    bool   fileStarted = false;

    // Set once the first session has started, so the log is truncated on launch
    // but not again on every start()/stop() cycle within the same session.
    bool   sessionStarted = false;

    // Writer-thread only: anomaly coalescing state.
    double   lastLoggedSec = -1.0e9;
    unsigned lastLoggedFlags = 0;
    int      suppressedSinceLastLog = 0;

    std::atomic<int>      anomalyCount   { 0 };
    std::atomic<unsigned> lastFlags      { 0 };
    std::atomic<double>   lastAnomalySec { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DebugLogger)
};

} // namespace lunara
