#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

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
    double getTailLengthSeconds() const override { return 8.0; }

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

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

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
    float accMinIn = 1.0e9f, accMaxIn = -1.0e9f;
    float accMinOut = 1.0e9f, accMaxOut = -1.0e9f;
    int   accCount = 0;

    std::atomic<float> displayBpm     { 120.0f };
    std::atomic<float> displayDelayMs { 0.0f };
    std::atomic<float> displayGrDb    { 0.0f };
    std::atomic<bool>  displayLimiting { false };

    std::atomic<bool>  stateRestored  { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LunaraAudioProcessor)
};
