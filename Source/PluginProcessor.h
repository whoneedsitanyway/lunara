#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>

#include "dsp/GrainEngine.h"
#include "dsp/SyncDelay.h"
#include "dsp/CloudReverb.h"
#include "dsp/Limiter.h"
#include "dsp/HighPassFilter.h"
#include "DebugLogger.h"

//==============================================================================
namespace lunara
{
    namespace ids
    {
        static constexpr const char* mix        = "mix";
        static constexpr const char* mode       = "mode";
        static constexpr const char* size       = "size";
        static constexpr const char* density    = "density";
        static constexpr const char* spray      = "spray";
        static constexpr const char* pitch      = "pitch";
        static constexpr const char* pitchMode  = "pitchMode";
        static constexpr const char* delay      = "delay";
        static constexpr const char* sync       = "sync";
        static constexpr const char* division   = "division";
        static constexpr const char* feedback   = "feedback";
        static constexpr const char* pingpong   = "pingpong";
        static constexpr const char* tone       = "tone";
        static constexpr const char* reverb     = "reverb";
        static constexpr const char* decay      = "decay";
        static constexpr const char* cloud      = "cloud";
        static constexpr const char* limiter    = "limiter";
        static constexpr const char* output     = "output";
        static constexpr const char* autoGain   = "autoGain";
        static constexpr const char* hpf        = "hpf";
        static constexpr const char* freeze     = "freeze";
        static constexpr const char* halftime   = "halftime";
        static constexpr const char* chaos      = "chaos";
        static constexpr const char* fxSource   = "fxSource";
        static constexpr const char* reverse    = "reverse";
        static constexpr const char* extreme    = "extreme";
    }

    // Sync division values expressed as a fraction of a whole note.
    static constexpr double kDivisions[] =
    {
        1.0 / 8.0,   // 1/32
        1.0 / 6.0,   // 1/16T
        1.0 / 4.0,   // 1/16
        1.0 / 3.0,   // 1/8T
        3.0 / 4.0,   // 1/8D
        1.0 / 2.0,   // 1/8
        2.0 / 3.0,   // 1/4T
        3.0 / 2.0,   // 1/4D
        1.0,         // 1/4
        2.0,         // 1/2
        4.0          // 1/1
    };
}

//==============================================================================
class LunaraAudioProcessor : public juce::AudioProcessor
{
public:
    LunaraAudioProcessor();
    ~LunaraAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // 12 s covers the longest musical tails (STRETCH mode / max reverb decay)
    // for hosts that render the tail after the input stops.
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Randomises the musical parameters to a sensible range. Safe to call from
    // the message thread (used by the RANDOM button in the editor).
    void randomizeParameters();

    // --- Scope / visualisation (audio thread -> message thread) --------------
    // The audio thread keeps a decimated min/max envelope of the input and output
    // (one bucket per kScopeDecimate samples). The editor reads the most recent
    // buckets to draw the oscilloscope, so the per-frame cost is tiny regardless
    // of the visible time span. Lock-free via an atomic bucket write index.
    static constexpr double kScopeSeconds  = 10.0;
    static constexpr int    kScopeDecimate = 32;

    int  getScopeNumBuckets() const noexcept { return scopeNumBuckets; }

    // How many of the most recent buckets already hold audio. The ring buffer is
    // zero-filled after prepareToPlay, so this grows 0 -> scopeNumBuckets; the
    // editor uses it to draw only the recorded part of the history.
    int  getScopeFilledBuckets() const noexcept
    {
        return scopeFilledBuckets.load (std::memory_order_relaxed);
    }

    // Copies the most recent numBuckets buckets (oldest first) into the arrays.
    void copyScopeBuckets (float* minIn, float* maxIn,
                           float* minOut, float* maxOut, int numBuckets) const;

    float getDisplayBpm() const noexcept     { return displayBpm.load (std::memory_order_relaxed); }
    float getDisplayDelayMs() const noexcept { return displayDelayMs.load (std::memory_order_relaxed); }
    float getDisplayGrDb() const noexcept    { return displayGrDb.load (std::memory_order_relaxed); }
    bool  getDisplayLimiting() const noexcept { return displayLimiting.load (std::memory_order_relaxed) > 0.5f; }

    // "Black box" recorder: per-stage level snapshots + anomaly dumps.
    lunara::DebugLogger& getDebugLogger() noexcept { return debugLogger; }

    // True once the host has restored a saved state (project reload). The editor
    // uses this to decide whether to apply the default factory preset: it only
    // does so for a fresh instance, never overriding a restored project.
    bool wasStateRestored() const noexcept { return stateRestored.load (std::memory_order_relaxed); }

    // True once the default factory preset has been applied for this instance.
    // The default is applied only on the *first* editor open of a fresh instance:
    // hosts routinely destroy and recreate the editor (e.g. when switching
    // tracks), and without this guard every re-open re-applied the default and
    // wiped the user's edits. Marking it also keeps a restored project safe.
    bool wasDefaultPresetApplied() const noexcept { return defaultPresetApplied.load (std::memory_order_relaxed); }
    void markDefaultPresetApplied() noexcept { defaultPresetApplied.store (true, std::memory_order_relaxed); }

    // Last preset selected in the editor. Kept on the processor (not the editor)
    // so it survives the host destroying and recreating the editor, which would
    // otherwise make the preset combo fall back to the default name on re-open.
    const juce::String& getLastPresetName() const noexcept { return lastPresetName; }
    void setLastPresetName (const juce::String& n) { lastPresetName = n; }

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Cached raw parameter pointers, filled once in the constructor. The audio
    // thread reads these instead of doing a string lookup per block
    // (apvts.getRawParameterValue is a search by parameter id).
    struct RawParams
    {
        std::atomic<float>* mix       = nullptr;
        std::atomic<float>* mode      = nullptr;
        std::atomic<float>* halftime  = nullptr;
        std::atomic<float>* sync      = nullptr;
        std::atomic<float>* division  = nullptr;
        std::atomic<float>* delay     = nullptr;
        std::atomic<float>* feedback  = nullptr;
        std::atomic<float>* pingpong  = nullptr;
        std::atomic<float>* tone      = nullptr;
        std::atomic<float>* reverb    = nullptr;
        std::atomic<float>* decay     = nullptr;
        std::atomic<float>* cloud     = nullptr;
        std::atomic<float>* size      = nullptr;
        std::atomic<float>* density   = nullptr;
        std::atomic<float>* spray     = nullptr;
        std::atomic<float>* pitch     = nullptr;
        std::atomic<float>* pitchMode = nullptr;
        std::atomic<float>* chaos     = nullptr;
        std::atomic<float>* freeze    = nullptr;
        std::atomic<float>* limiter   = nullptr;
        std::atomic<float>* hpf       = nullptr;
        std::atomic<float>* output    = nullptr;
        std::atomic<float>* autoGain  = nullptr;
        std::atomic<float>* fxSource  = nullptr;
        std::atomic<float>* reverse   = nullptr;
        std::atomic<float>* extreme   = nullptr;
    };

    void bindRawParams();
    RawParams rawParams;

    double currentBpm = 120.0;
    double currentSampleRate = 48000.0;

    lunara::GrainEngine   grainEngine;
    lunara::SyncDelay     syncDelay;
    lunara::CloudReverb   cloudReverb;
    lunara::Limiter       outputLimiter;
    lunara::HighPassFilter outputHpf;

    lunara::DebugLogger   debugLogger;
    double logTimeSec = 0.0;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> fxBuffer;
    juce::AudioBuffer<float> outBuffer;

    juce::SmoothedValue<float> mixSmoothed, outputSmoothed;
    juce::SmoothedValue<float> autoGainSmoothed;

    // Smooth 0..1 amount for the STRETCH ("Extreme") mode: it fades the whole
    // chain towards its most diffuse settings, so the switch never clicks.
    juce::SmoothedValue<float> extremeSmoothed;

    // Auto-gain (level matching) envelope followers.
    float dryRms = 0.0f;
    float outRms = 0.0f;
    float autoGainCoeff = 0.0f;

    std::vector<float> scopeMinIn, scopeMaxIn, scopeMinOut, scopeMaxOut;
    int scopeNumBuckets = 0;
    std::atomic<int> bucketWrite { 0 };
    std::atomic<int> scopeFilledBuckets { 0 };
    float accMinIn = 1.0e9f, accMaxIn = -1.0e9f;
    float accMinOut = 1.0e9f, accMaxOut = -1.0e9f;
    int   accCount = 0;

    std::atomic<float> displayBpm     { 120.0f };
    std::atomic<float> displayDelayMs { 0.0f };
    std::atomic<float> displayGrDb    { 0.0f };
    std::atomic<bool>  displayLimiting { false };

    std::atomic<bool>  stateRestored  { false };

    // Set the first time the default preset is applied, so re-opening the editor
    // never re-applies it (see wasDefaultPresetApplied()).
    std::atomic<bool>  defaultPresetApplied { false };

    // Message-thread only: last preset selected in the editor (survives editor
    // recreation, see getLastPresetName()).
    juce::String lastPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LunaraAudioProcessor)
};
