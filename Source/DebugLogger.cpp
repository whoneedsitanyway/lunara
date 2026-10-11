#include "DebugLogger.h"
#include "PresetManager.h"

namespace lunara
{

//==============================================================================
DebugLogger::DebugLogger()
    : juce::Thread ("Lunara debug logger")
{
}

DebugLogger::~DebugLogger()
{
    stop();
}

void DebugLogger::start()
{
    if (! isThreadRunning())
    {
        // Each fresh session starts with a clean log, so it only ever holds the
        // current run instead of appending to everything from previous launches.
        if (! sessionStarted)
        {
            clearLog();
            sessionStarted = true;
        }

        startThread (juce::Thread::Priority::low);
    }
}

void DebugLogger::stop()
{
    if (isThreadRunning())
    {
        signalThreadShouldExit();
        notify();
        stopThread (1000);
    }
}

//==============================================================================
void DebugLogger::push (const BlockSnapshot& s) noexcept
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 > 0)
    {
        fifoData[(size_t) start1] = s;
        fifo.finishedWrite (1);
    }
    // If the FIFO is full we simply drop the snapshot: logging must never block
    // or back-pressure the audio thread.
}

//==============================================================================
juce::File DebugLogger::getLogFile() const
{
   #ifdef LUNARA_TEST_BUILD
    // Test builds write to a scratch file in the temp folder, so a test run
    // never touches (or clears) the real per-user log.
    return juce::File::getSpecialLocation (juce::File::tempDirectory)
               .getChildFile ("lunara_debug.log");
   #else
    // Same per-user folder as presets / theme, so everything lives together.
    return PresetManager::getAppDir().getChildFile ("lunara_debug.log");
   #endif
}

void DebugLogger::clearLog()
{
    auto f = getLogFile();
    if (f.existsAsFile())
        f.deleteFile();
    fileStarted = false;
    lastLoggedSec = -1.0e9;
    lastLoggedFlags = 0;
    suppressedSinceLastLog = 0;
}

//==============================================================================
void DebugLogger::run()
{
    while (! threadShouldExit())
    {
        drain();
        wait (50);
    }
    drain();
}

void DebugLogger::drain()
{
    // Pop everything currently queued (bounded so we never hog the thread).
    for (int guard = 0; guard < kFifoSize; ++guard)
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead (1, start1, size1, start2, size2);

        if (size1 <= 0)
            break;

        const BlockSnapshot s = fifoData[(size_t) start1];
        fifo.finishedRead (1);

        // Keep the rolling history.
        history[(size_t) historyWrite] = s;
        historyWrite = (historyWrite + 1) % kHistory;
        if (historyCount < kHistory)
            ++historyCount;

        if (s.flags != 0)
        {
            lastFlags.store (s.flags, std::memory_order_relaxed);
            lastAnomalySec.store (s.timeSec, std::memory_order_relaxed);
            anomalyCount.fetch_add (1, std::memory_order_relaxed);

            // Coalesce a storm of identical flags: log the first one, then only
            // re-log once the flags change or the cooldown has elapsed. This keeps
            // a benign, repeating flag (e.g. AUTOGAIN_MAX on a silent input) from
            // dumping the entire history on every block and flooding the file.
            const bool flagsChanged    = (s.flags != lastLoggedFlags);
            const bool cooldownElapsed = (s.timeSec - lastLoggedSec >= kAnomalyCooldownSec);
            const bool firstEver       = (lastLoggedSec < -1.0e8);

            if (firstEver || flagsChanged || cooldownElapsed)
            {
                juce::String batch;
                batch << "\n"
                      << "============================================================\n"
                      << "*** ANOMALY @" << timestamp() << "  flags: "
                      << describeFlags (s.flags) << "\n"
                      << "============================================================\n";

                if (suppressedSinceLastLog > 0)
                    batch << "  (+" << suppressedSinceLastLog
                          << " similar block(s) suppressed since the last entry)\n";

                // Dump the recent history (oldest first) so the lead-up is visible.
                const int n = historyCount;
                for (int i = 0; i < n; ++i)
                {
                    const int idx = (historyWrite - n + i + kHistory * 2) % kHistory;
                    batch << formatLine (history[(size_t) idx], true) << "\n";
                }
                batch << "---- trigger block ----\n";
                batch << formatLine (s, true) << "\n";
                appendLine (batch);

                lastLoggedFlags = s.flags;
                lastLoggedSec = s.timeSec;
                suppressedSinceLastLog = 0;
            }
            else
            {
                ++suppressedSinceLastLog;
            }
        }
        else if (s.timeSec - lastHeartbeatSec >= 2.0)
        {
            lastHeartbeatSec = s.timeSec;
            appendLine (formatLine (s, false) + "\n");
        }
    }
}

//==============================================================================
void DebugLogger::appendLine (const juce::String& line)
{
    auto f = getLogFile();
    f.getParentDirectory().createDirectory();

    // Simple size cap so the log can't grow without bound: rotate to ".prev".
    if (f.existsAsFile() && f.getSize() > kMaxLogBytes)
    {
        auto prev = f.getSiblingFile ("lunara_debug.prev.log");
        prev.deleteFile();
        f.moveFileTo (prev);
        fileStarted = false;
    }

    if (! fileStarted)
    {
        f.appendText (juce::String ("# Lunara debug log - started ")
                          + timestamp() + "\n");
        fileStarted = true;
    }

    f.appendText (line);
}

//==============================================================================
juce::String DebugLogger::timestamp()
{
    const auto now = juce::Time::getCurrentTime();
    return now.toString (true, true, true, true) + "."
             + juce::String (now.getMilliseconds()).paddedLeft ('0', 3);
}

juce::String DebugLogger::describeFlags (unsigned flags)
{
    if (flags == 0)
        return "none";

    juce::StringArray parts;
    if (flags & kFlagNonFinite)   parts.add ("NONFINITE");
    if (flags & kFlagInputHot)    parts.add ("INPUT_HOT");
    if (flags & kFlagMixHot)      parts.add ("MIX_HOT");
    if (flags & kFlagOutHot)      parts.add ("OUT_HOT");
    if (flags & kFlagAutoGainMax) parts.add ("AUTOGAIN_MAX");
    if (flags & kFlagHeavyGR)     parts.add ("HEAVY_GR");
    return parts.joinIntoString ("|");
}

juce::String DebugLogger::formatLine (const BlockSnapshot& s, bool anomaly)
{
    juce::String line;
    line << (anomaly ? "  " : "")
         << "t=" << juce::String (s.timeSec, 2) << "s"
         << "  in=" << juce::String (s.inPeak, 4)
         << " grain=" << juce::String (s.grainPeak, 4)
         << " dly=" << juce::String (s.delayPeak, 4)
         << " rev=" << juce::String (s.reverbPeak, 4)
         << " mix=" << juce::String (s.mixPeak, 4)
         << " preLim=" << juce::String (s.preLimPeak, 4)
         << " out=" << juce::String (s.outPeak, 4)
         << "  AG=" << juce::String (s.autoGain, 3) << "x"
         << " tgt=" << juce::String (s.autoTarget, 3)
         << " GR=" << juce::String (s.grDb, 1) << "dB"
         << "  fb=" << juce::String (s.feedback, 2)
         << " tone=" << juce::String (s.tone, 0)
         << " decay=" << juce::String (s.decay, 2)
         << " send=" << juce::String (s.send, 2)
         << " cloud=" << juce::String (s.cloud, 2)
         << " hpf=" << juce::String (s.hpf, 0)
         << " out=" << juce::String (s.output, 1)
         << " pitch=" << juce::String (s.pitch, 1)
         << " size=" << juce::String (s.size, 0)
         << " dens=" << juce::String (s.density, 0)
         << " spray=" << juce::String (s.spray, 2)
         << " chaos=" << juce::String (s.chaos, 2)
         << " mode=" << s.mode
         << " HT=" << (s.halfTime ? 1 : 0)
         << " AGen=" << (s.autoGainOn ? 1 : 0)
         << " FZ=" << (s.frozen ? 1 : 0)
         << " RV=" << (s.reverseOn ? 1 : 0)
         << " EX=" << juce::String (s.extremeAmt, 2);
    return line;
}

} // namespace lunara
