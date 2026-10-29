#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
    using APVTS = juce::AudioProcessorValueTreeState;

    // Default low-cut (HPF) cutoff: keeps the low end tight out of the box.
    constexpr float kHpfDefaultHz = 100.0f;

    // Per-mode "character" applied on top of the user's knob values.  Gives the
    // Microcosm-style personalities without hiding the controls.
    struct ModeChar
    {
        float densityMul;
        float sizeMul;
        float sprayAdd;
        float reverseAdd;
        float reverbMul;
        float cloudAdd;
        float drift;
    };

    //                     dens   size  spray  rev    revM   cloud  drift
    const ModeChar kModes[6] =
    {
        /* Mosaic */ { 1.7f, 0.45f, 0.08f, 0.00f, 0.85f, 0.10f, 0.00f },
        /* Glide  */ { 0.8f, 2.00f, 0.18f, 0.00f, 1.20f, 0.40f, 0.00f },
        /* Haze   */ { 0.6f, 2.40f, 0.40f, 0.10f, 1.60f, 0.80f, 0.12f },
        /* Bloom  */ { 1.0f, 1.50f, 0.15f, 0.50f, 1.30f, 0.50f, 0.00f },
        /* Nebula */ { 1.2f, 2.80f, 0.50f, 0.20f, 2.00f, 1.00f, 0.00f },
        /* Drift  */ { 1.3f, 1.00f, 0.30f, 0.20f, 1.10f, 0.30f, 1.00f }
    };

    // Peak magnitude across the first two channels (cheap, used for the logger).
    float peakMagnitude (const juce::AudioBuffer<float>& b, int numSamples) noexcept
    {
        float p = 0.0f;
        const int chans = juce::jmin (2, b.getNumChannels());
        for (int ch = 0; ch < chans; ++ch)
        {
            const float* d = b.getReadPointer (ch);
            for (int n = 0; n < numSamples; ++n)
                p = juce::jmax (p, std::abs (d[n]));
        }
        return p;
    }
}

//==============================================================================
LunaraAudioProcessor::LunaraAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    bindRawParams();
}

LunaraAudioProcessor::~LunaraAudioProcessor() = default;

//==============================================================================
void LunaraAudioProcessor::bindRawParams()
{
    using namespace lunara;
    auto bind = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    rawParams.mix       = bind (ids::mix);
    rawParams.mode      = bind (ids::mode);
    rawParams.halftime  = bind (ids::halftime);
    rawParams.sync      = bind (ids::sync);
    rawParams.division  = bind (ids::division);
    rawParams.delay     = bind (ids::delay);
    rawParams.feedback  = bind (ids::feedback);
    rawParams.pingpong  = bind (ids::pingpong);
    rawParams.tone      = bind (ids::tone);
    rawParams.reverb    = bind (ids::reverb);
    rawParams.decay     = bind (ids::decay);
    rawParams.cloud     = bind (ids::cloud);
    rawParams.size      = bind (ids::size);
    rawParams.density   = bind (ids::density);
    rawParams.spray     = bind (ids::spray);
    rawParams.pitch     = bind (ids::pitch);
    rawParams.pitchMode = bind (ids::pitchMode);
    rawParams.chaos     = bind (ids::chaos);
    rawParams.freeze    = bind (ids::freeze);
    rawParams.limiter   = bind (ids::limiter);
    rawParams.hpf       = bind (ids::hpf);
    rawParams.output    = bind (ids::output);
    rawParams.autoGain  = bind (ids::autoGain);
    rawParams.fxSource  = bind (ids::fxSource);
    rawParams.reverse   = bind (ids::reverse);
    rawParams.extreme   = bind (ids::extreme);
}

//==============================================================================
APVTS::ParameterLayout LunaraAudioProcessor::createParameterLayout()
{
    using namespace lunara;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto addFloat = [&params] (const char* id, const juce::String& name,
                               float min, float max, float def,
                               const juce::String& suffix = {})
    {
        // Readouts: at most 4 decimals, trailing zeros trimmed, so the text
        // boxes stay clean (e.g. "0.35", "150", "-12" instead of "0.3500000").
        auto attrs = juce::AudioParameterFloatAttributes()
                         .withLabel (suffix)
                         .withStringFromValueFunction ([] (float v, int)
                         {
                             auto s = juce::String (v, 4);
                             if (s.containsChar ('.'))
                             {
                                 s = s.trimCharactersAtEnd ("0");
                                 s = s.trimCharactersAtEnd (".");
                             }
                             return s;
                         });

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            id, name, juce::NormalisableRange<float> (min, max), def, attrs));
    };

    auto addChoice = [&params] (const char* id, const juce::String& name,
                                const juce::StringArray& choices, int def)
    {
        params.push_back (std::make_unique<juce::AudioParameterChoice> (id, name, choices, def));
    };

    addFloat (ids::mix, "Mix", 0.0f, 1.0f, 0.3f);
    addChoice (ids::mode, "Mode",
               { "Mosaic", "Glide", "Haze", "Bloom", "Nebula", "Drift" }, 3);

    addFloat (ids::size, "Size", 20.0f, 500.0f, 150.0f, "ms");
    addFloat (ids::density, "Density", 1.0f, 100.0f, 22.0f);
    addFloat (ids::spray, "Spray", 0.0f, 1.0f, 0.35f);
    addFloat (ids::pitch, "Pitch", -12.0f, 12.0f, 0.0f, "st");
    addChoice (ids::pitchMode, "Pitch Mode", { "Quantize", "Smooth" }, 0);

    addFloat (ids::delay, "Delay", 10.0f, 2000.0f, 375.0f, "ms");
    params.push_back (std::make_unique<juce::AudioParameterBool> (ids::sync, "Sync", true));
    addChoice (ids::division, "Division",
               { "1/32", "1/16T", "1/16", "1/8T", "1/8D", "1/8",
                 "1/4T", "1/4D", "1/4", "1/2", "1/1" }, 8);
    addFloat (ids::feedback, "Feedback", 0.0f, 0.95f, 0.35f);
    addFloat (ids::pingpong, "Ping-Pong", 0.0f, 1.0f, 0.3f);
    addFloat (ids::tone, "Tone", 60.0f, 18000.0f, 18000.0f, "Hz");

    addFloat (ids::reverb, "Reverb", 0.0f, 1.0f, 0.5f);
    addFloat (ids::decay, "Decay", 0.0f, 1.0f, 0.6f);
    addFloat (ids::cloud, "Cloud", 0.0f, 1.0f, 0.5f);

    // The output stage is a simple on/off peak limiter; the low-cut filter is a
    // continuous HPF knob (20..500 Hz, 100 Hz by default).
    params.push_back (std::make_unique<juce::AudioParameterBool> (ids::limiter, "Limiter", true));
    addFloat (ids::hpf, "HPF", lunara::HighPassFilter::kMinCutoffHz,
              lunara::HighPassFilter::kMaxCutoffHz, kHpfDefaultHz, "Hz");
    addFloat (ids::output, "Output", -24.0f, 12.0f, 0.0f, "dB");
    params.push_back (std::make_unique<juce::AudioParameterBool> (ids::autoGain, "Auto Gain", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (ids::freeze, "Freeze", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (ids::halftime, "Half Time", false));
    addFloat (ids::chaos, "Chaos", 0.0f, 1.0f, 0.0f);

    // MOOD MK2-style routing: what the DELAY + SPACE effects process. The
    // granular cloud always stays audible in parallel (see processBlock).
    addChoice (ids::fxSource, "FX Source", { "IN", "OUT", "BOTH" }, 1);

    // REVERSE: play every grain backwards (a full 100% reverse probability).
    params.push_back (std::make_unique<juce::AudioParameterBool> (ids::reverse, "Reverse", false));

    // STRETCH ("Extreme"): fade the whole chain into a long, PaulStretch-like
    // texture (long grains, dense cloud, big reverb/delay tails).
    params.push_back (std::make_unique<juce::AudioParameterBool> (ids::extreme, "Stretch", false));

    return { params.begin(), params.end() };
}

//==============================================================================
void LunaraAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // The chain has no look-ahead, so the plugin introduces no host latency.
    setLatencySamples (0);

    grainEngine.prepare (sampleRate, samplesPerBlock, juce::jmax (2, getTotalNumOutputChannels()));
    syncDelay.prepare (sampleRate);
    cloudReverb.prepare (sampleRate);
    outputLimiter.prepare (sampleRate);
    outputHpf.prepare (sampleRate, rawParams.hpf->load());
    dryBuffer.setSize (2, samplesPerBlock, false, false, true);
    wetBuffer.setSize (2, samplesPerBlock, false, false, true);
    fxBuffer.setSize (2, samplesPerBlock, false, false, true);
    outBuffer.setSize (2, samplesPerBlock, false, false, true);

    scopeNumBuckets = juce::jlimit (64, 1 << 20,
                                    (int) (sampleRate * kScopeSeconds / kScopeDecimate));
    scopeMinIn.assign ((size_t) scopeNumBuckets, 0.0f);
    scopeMaxIn.assign ((size_t) scopeNumBuckets, 0.0f);
    scopeMinOut.assign ((size_t) scopeNumBuckets, 0.0f);
    scopeMaxOut.assign ((size_t) scopeNumBuckets, 0.0f);
    bucketWrite.store (0, std::memory_order_relaxed);
    scopeFilledBuckets.store (0, std::memory_order_relaxed);
    accMinIn = accMinOut = 1.0e9f;
    accMaxIn = accMaxOut = -1.0e9f;
    accCount = 0;

    mixSmoothed.reset (sampleRate, 0.02);
    outputSmoothed.reset (sampleRate, 0.02);
    autoGainSmoothed.reset (sampleRate, 0.12);
    // ~0.6 s fade in/out for the STRETCH mode.
    extremeSmoothed.reset (sampleRate, 0.6);

    // ~150 ms RMS followers for the auto-gain level match.
    autoGainCoeff = std::exp (-1.0f / (0.15f * (float) sampleRate));
    dryRms = outRms = 0.0f;
    autoGainSmoothed.setCurrentAndTargetValue (1.0f);

    mixSmoothed.setCurrentAndTargetValue (rawParams.mix->load());
    outputSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (rawParams.output->load()));
    extremeSmoothed.setCurrentAndTargetValue (
        rawParams.extreme->load() > 0.5f ? 1.0f : 0.0f);

    // Start the "black box" recorder (idempotent).
    logTimeSec = 0.0;
    debugLogger.start();
}

void LunaraAudioProcessor::releaseResources()
{
    debugLogger.stop();
}

bool LunaraAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void LunaraAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    if (numSamples == 0)
        return;

    // --- tempo ---------------------------------------------------------------
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 1.0)
                    currentBpm = *bpm;

    using namespace lunara;
    auto& rp = rawParams;   // cached pointers: no string lookups on the audio thread

    const float mix     = rp.mix->load();
    const int   modeIdx = juce::jlimit (0, 5, (int) rp.mode->load());
    const ModeChar mc   = kModes[modeIdx];

    // HALF TIME: everything runs at half speed - grains an octave down, delay
    // times doubled and a longer reverb tail - like the classic "half time" FX.
    const bool halfTime = rp.halftime->load() > 0.5f;

    const bool  syncOn  = rp.sync->load() > 0.5f;
    const int   divIdx  = juce::jlimit (0, 10, (int) rp.division->load());
    const float delayMsRaw = syncOn
        ? (float) (60000.0 / currentBpm * kDivisions[divIdx])
        : rp.delay->load();
    const float delayMs = juce::jlimit (1.0f, 3900.0f, delayMsRaw * (halfTime ? 2.0f : 1.0f));

    // STRETCH ("Extreme"): a smooth 0..1 amount that fades the whole chain
    // towards its most diffuse settings - long grains, a dense cloud, a big
    // reverb/delay tail and a slow drift. PaulStretch-style: any input melts
    // into a long, evolving texture. Bounded so the safety stage stays in
    // charge (feedback stays below the tested 0.95 ceiling).
    extremeSmoothed.setTargetValue (rp.extreme->load() > 0.5f ? 1.0f : 0.0f);
    const float ex = extremeSmoothed.getNextValue();
    auto melt = [] (float base, float extreme, float t) { return base + (extreme - base) * t; };

    const float feedback = melt (rp.feedback->load(), 0.93f, ex);
    const float pingpong = rp.pingpong->load();
    const float tone     = rp.tone->load();

    const float reverbSend = melt (juce::jlimit (0.0f, 1.2f, rp.reverb->load() * mc.reverbMul), 1.2f, ex);
    const float decay      = melt (juce::jlimit (0.0f, 1.0f, rp.decay->load() + (halfTime ? 0.12f : 0.0f)), 1.0f, ex);
    const float cloud      = melt (juce::jlimit (0.0f, 1.0f, rp.cloud->load() + mc.cloudAdd), 1.0f, ex);
    const float revSize    = halfTime ? 0.78f : 0.6f;

    // REVERSE: force every grain to play backwards.
    const bool reverseOn = rp.reverse->load() > 0.5f;

    GrainParams gp;
    gp.grainSizeMs   = juce::jlimit (20.0f, 500.0f,
                                     melt (rp.size->load() * mc.sizeMul * (halfTime ? 1.4f : 1.0f), 500.0f, ex));
    gp.density       = juce::jlimit (1.0f, 120.0f, melt (rp.density->load() * mc.densityMul, 120.0f, ex));
    gp.spray         = juce::jlimit (0.0f, 1.0f, melt (rp.spray->load() + mc.sprayAdd, 1.0f, ex));
    gp.pitchSemis    = rp.pitch->load() + (halfTime ? -12.0f : 0.0f);
    gp.pitchQuantize = rp.pitchMode->load() < 0.5f;
    gp.reverseProb   = reverseOn ? 1.0f : juce::jlimit (0.0f, 1.0f, 0.05f + mc.reverseAdd);
    gp.stereoSpread  = 0.65f;
    gp.jitter        = 0.35f;
    gp.chaos         = rp.chaos->load();
    gp.drift         = juce::jlimit (0.0f, 1.0f, melt (mc.drift + rp.chaos->load() * 0.3f, 0.6f, ex));
    gp.frozen        = rp.freeze->load() > 0.5f;

    const bool  limiterOn = rp.limiter->load() > 0.5f;

    // --- copy dry ------------------------------------------------------------
    dryBuffer.setSize (2, numSamples, false, false, true);
    wetBuffer.setSize (2, numSamples, false, false, true);
    fxBuffer.setSize (2, numSamples, false, false, true);
    outBuffer.setSize (2, numSamples, false, false, true);

    dryBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
    dryBuffer.copyFrom (1, 0, buffer, numCh > 1 ? 1 : 0, 0, numSamples);

    // --- granular cloud (always computed: it is the plugin's core texture) ---
    grainEngine.process (dryBuffer.getReadPointer (0), dryBuffer.getReadPointer (1),
                         wetBuffer.getWritePointer (0), wetBuffer.getWritePointer (1),
                         numSamples, gp);

    const float grainPeak = peakMagnitude (wetBuffer, numSamples);

    // --- effects source switch (MOOD MK2-style routing) ----------------------
    // The DELAY + SPACE effects process a selectable source, while the granular
    // cloud always stays audible in the wet path (in parallel):
    //   IN   (0): effects process the live input only
    //   OUT  (1): effects process the granular cloud only   (default / legacy)
    //   BOTH (2): effects process the live input + the cloud
    const int fxSrc = juce::jlimit (0, 2, (int) rp.fxSource->load());
    {
        const float* dL = dryBuffer.getReadPointer (0);
        const float* dR = dryBuffer.getReadPointer (1);
        const float* gL = wetBuffer.getReadPointer (0);
        const float* gR = wetBuffer.getReadPointer (1);
        float* sL = fxBuffer.getWritePointer (0);
        float* sR = fxBuffer.getWritePointer (1);

        for (int n = 0; n < numSamples; ++n)
        {
            if (fxSrc == 0)      { sL[n] = dL[n];       sR[n] = dR[n]; }        // IN
            else if (fxSrc == 2) { sL[n] = dL[n] + gL[n]; sR[n] = dR[n] + gR[n]; } // BOTH
            else                 { sL[n] = gL[n];       sR[n] = gR[n]; }        // OUT
        }
    }

    syncDelay.process (fxBuffer.getWritePointer (0), fxBuffer.getWritePointer (1),
                       numSamples, delayMs, feedback, tone, pingpong);

    const float delayPeak = peakMagnitude (fxBuffer, numSamples);

    cloudReverb.process (fxBuffer.getWritePointer (0), fxBuffer.getWritePointer (1),
                         numSamples, revSize, decay, 0.5f, 0.4f, cloud, reverbSend);

    const float reverbPeak = peakMagnitude (fxBuffer, numSamples);

    // Recombine. Both the delay and the reverb are *additive* sends (they add
    // their wet output on top of whatever enters them), so subtracting the
    // selected source leaves only the effects' contribution. The granular cloud
    // is always mixed back in, so it never disappears when IN or BOTH is chosen.
    {
        const float* dL = dryBuffer.getReadPointer (0);
        const float* dR = dryBuffer.getReadPointer (1);
        const float* fL = fxBuffer.getReadPointer (0);
        const float* fR = fxBuffer.getReadPointer (1);
        float* wL = wetBuffer.getWritePointer (0);
        float* wR = wetBuffer.getWritePointer (1);

        for (int n = 0; n < numSamples; ++n)
        {
            const float grL = wL[n];
            const float grR = wR[n];
            float srcL, srcR;
            if (fxSrc == 0)      { srcL = dL[n];          srcR = dR[n]; }
            else if (fxSrc == 2) { srcL = dL[n] + grL;    srcR = dR[n] + grR; }
            else                 { srcL = grL;            srcR = grR; }

            wL[n] = grL + (fL[n] - srcL);
            wR[n] = grR + (fR[n] - srcR);
        }
    }

    // --- mix dry/wet into a stereo scratch buffer ----------------------------
    mixSmoothed.setTargetValue (mix);
    {
        const float* dL = dryBuffer.getReadPointer (0);
        const float* dR = dryBuffer.getReadPointer (1);
        const float* wL = wetBuffer.getReadPointer (0);
        const float* wR = wetBuffer.getReadPointer (1);
        float* oL = outBuffer.getWritePointer (0);
        float* oR = outBuffer.getWritePointer (1);

        for (int n = 0; n < numSamples; ++n)
        {
            const float m = mixSmoothed.getNextValue();
            oL[n] = dL[n] * (1.0f - m) + wL[n] * m;
            oR[n] = dR[n] * (1.0f - m) + wR[n] * m;
        }
    }

    const float mixPeak = peakMagnitude (outBuffer, numSamples);

    // --- low-cut (HPF) on the output -----------------------------------------
    // A continuous 20..500 Hz low-cut (100 Hz by default) so big reverb/delay
    // tails never turn the low end into mud. The cutoff glides internally, so
    // sweeping the knob produces a smooth filter sweep.
    outputHpf.setCutoff (rp.hpf->load());
    outputHpf.process (outBuffer.getWritePointer (0), outBuffer.getWritePointer (1), numSamples);

    // --- auto gain + output, applied BEFORE the limiter ----------------------
    // Auto Gain matches the loudness of the processed signal to the input. It is
    // deliberately placed *before* the limiter so the limiter remains the final
    // brick-wall: previously it sat after the limiter and could multiply an
    // already-limited signal by up to +18 dB, blowing past full scale (the
    // "volume to the heavens" bug). With Auto Gain on the OUTPUT knob is
    // bypassed (and disabled in the UI) so the two cannot fight each other.
    const bool autoGainOn = rp.autoGain->load() > 0.5f;
    float autoTarget = 1.0f;
    bool  autoGainClamped = false;

    // Input (dry) loudness - the level we want to match.
    {
        const float* dL = dryBuffer.getReadPointer (0);
        const float* dR = dryBuffer.getReadPointer (1);
        float dryMS = 0.0f;
        for (int n = 0; n < numSamples; ++n)
            dryMS += dL[n] * dL[n] + dR[n] * dR[n];
        dryMS *= 1.0f / (float) juce::jmax (1, numSamples * 2);
        dryRms = std::sqrt (autoGainCoeff * dryRms * dryRms + (1.0f - autoGainCoeff) * dryMS);
    }

    // Auto Gain is a feedback controller: outRms holds the *final* (post-limiter)
    // loudness measured on the previous block, so the perceived output level is
    // still matched while the limiter remains the last stage (and thus a true
    // brick-wall).
    if (autoGainOn)
    {
        if (outRms > 1.0e-4f && dryRms > 1.0e-5f)
        {
            const float ratio = dryRms / outRms;
            autoTarget = juce::jlimit (0.125f, 8.0f, ratio);
            autoGainClamped = (ratio > 8.0f || ratio < 0.125f);
        }

        autoGainSmoothed.setTargetValue (autoTarget);
    }
    else
    {
        autoGainSmoothed.setTargetValue (1.0f);
    }

    outputSmoothed.setTargetValue (autoGainOn
                                       ? 1.0f
                                       : juce::Decibels::decibelsToGain (rp.output->load()));
    {
        float* oL = outBuffer.getWritePointer (0);
        float* oR = outBuffer.getWritePointer (1);
        for (int n = 0; n < numSamples; ++n)
        {
            const float g = autoGainSmoothed.getNextValue() * outputSmoothed.getNextValue();
            oL[n] *= g;
            oR[n] *= g;
        }
    }

    const float preLimPeak = peakMagnitude (outBuffer, numSamples);

    // --- output stage: an optional final peak limiter ------------------------
    // When the Limiter checkbox is off the output stage is bypassed; the final
    // safety clamp below still guarantees a bounded output. When on it is the
    // last musical stage and keeps the output at the ceiling (a true
    // brick-wall, with a soft ceiling for the attack overshoot).
    float grDb = 0.0f;
    if (limiterOn)
    {
        outputLimiter.process (outBuffer.getWritePointer (0), outBuffer.getWritePointer (1),
                               numSamples, 0.98f);
        grDb = outputLimiter.getGainReductionDb();
        displayGrDb.store (grDb, std::memory_order_relaxed);
        displayLimiting.store (outputLimiter.isLimiting(), std::memory_order_relaxed);
    }
    else
    {
        displayGrDb.store (0.0f, std::memory_order_relaxed);
        displayLimiting.store (false, std::memory_order_relaxed);
    }

    // --- final safety: finite AND bounded ------------------------------------
    // The limiter (when enabled) already keeps the output within +/-1.0. This
    // last stage catches anything else: non-finite values are zeroed, while a
    // finite over-range sample (e.g. with the limiter bypassed) is clamped to
    // +/-1.0 rather than zeroed, so it degrades gracefully instead of gating.
    // The finite-check, the clamp and both peak measurements are fused into a
    // single pass over the output.
    const float kSafetyCeil = 1.0f;
    float rawOutPeak = 0.0f;
    float outPeak    = 0.0f;
    bool  bad        = false;
    bool  outClamped = false;
    {
        float* oL = outBuffer.getWritePointer (0);
        float* oR = outBuffer.getWritePointer (1);
        auto sanitise = [&] (float& s)
        {
            if (! std::isfinite (s))
            {
                bad = outClamped = true;
                s = 0.0f;
            }
            else
            {
                const float a = std::abs (s);
                rawOutPeak = juce::jmax (rawOutPeak, a);
                if (a > kSafetyCeil)
                {
                    outClamped = true;
                    s = juce::jlimit (-kSafetyCeil, kSafetyCeil, s);
                }
            }
            outPeak = juce::jmax (outPeak, std::abs (s));
        };
        for (int n = 0; n < numSamples; ++n)
        {
            sanitise (oL[n]);
            sanitise (oR[n]);
        }
    }

    // Update the output loudness follower from the final (post-limiter) signal,
    // ready for the Auto Gain controller on the next block.
    {
        const float* oL = outBuffer.getReadPointer (0);
        const float* oR = outBuffer.getReadPointer (1);
        float outMS = 0.0f;
        for (int n = 0; n < numSamples; ++n)
            outMS += oL[n] * oL[n] + oR[n] * oR[n];
        outMS *= 1.0f / (float) juce::jmax (1, numSamples * 2);
        outRms = std::sqrt (autoGainCoeff * outRms * outRms + (1.0f - autoGainCoeff) * outMS);
    }

    // A true runaway: non-finite values, or the pre-limiter signal exploding far
    // beyond anything musical. Only then do we hard-reset the DSP state.
    const bool runaway = bad || mixPeak > 64.0f || preLimPeak > 64.0f;

    // --- black box: record this block (per-stage peaks + flags) --------------
    {
        lunara::BlockSnapshot snap;
        snap.timeSec     = logTimeSec;
        snap.inPeak      = peakMagnitude (dryBuffer, numSamples);
        snap.grainPeak   = grainPeak;
        snap.delayPeak   = delayPeak;
        snap.reverbPeak  = reverbPeak;
        snap.mixPeak     = mixPeak;
        snap.preLimPeak  = preLimPeak;
        snap.outPeak     = outPeak;
        snap.autoGain    = autoGainSmoothed.getCurrentValue();
        snap.autoTarget  = autoTarget;
        snap.grDb        = grDb;
        snap.mix         = mix;
        snap.feedback    = feedback;
        snap.tone        = tone;
        snap.decay       = decay;
        snap.send        = reverbSend;
        snap.cloud       = cloud;
        snap.hpf         = rp.hpf->load();
        snap.output      = rp.output->load();
        snap.pitch       = gp.pitchSemis;
        snap.size        = gp.grainSizeMs;
        snap.density     = gp.density;
        snap.spray       = gp.spray;
        snap.chaos       = gp.chaos;
        snap.mode        = modeIdx;
        snap.halfTime    = halfTime;
        snap.autoGainOn  = autoGainOn;
        snap.frozen      = gp.frozen;
        snap.reverseOn   = reverseOn;
        snap.extreme     = ex > 0.01f;
        snap.extremeAmt  = ex;

        if (bad)                    snap.flags |= lunara::kFlagNonFinite;
        if (snap.inPeak > 2.0f)     snap.flags |= lunara::kFlagInputHot;
        if (mixPeak > 4.0f)         snap.flags |= lunara::kFlagMixHot;
        if (outClamped || rawOutPeak > 1.5f) snap.flags |= lunara::kFlagOutHot;
        if (autoGainClamped)        snap.flags |= lunara::kFlagAutoGainMax;
        if (grDb > 24.0f)           snap.flags |= lunara::kFlagHeavyGR;

        debugLogger.push (snap);
    }

    logTimeSec += (double) numSamples / currentSampleRate;

    if (runaway)
    {
        outBuffer.clear();
        grainEngine.reset();
        syncDelay.reset();
        cloudReverb.reset();
        outputLimiter.reset();
        outputHpf.reset();
        dryRms = outRms = 0.0f;
    }

    // --- fold back into the host buffer (handles mono hosts safely) ----------
    if (numCh >= 2)
    {
        buffer.copyFrom (0, 0, outBuffer, 0, 0, numSamples);
        buffer.copyFrom (1, 0, outBuffer, 1, 0, numSamples);
    }
    else
    {
        // Mono host: fold L+R down to a single channel.
        for (int n = 0; n < numSamples; ++n)
            buffer.setSample (0, n, (outBuffer.getSample (0, n) + outBuffer.getSample (1, n)) * 0.5f);
    }

    // --- feed the oscilloscope (decimated min/max envelope) ------------------
    displayBpm.store ((float) currentBpm, std::memory_order_relaxed);
    displayDelayMs.store (delayMs, std::memory_order_relaxed);

    if (scopeNumBuckets > 0)
    {
        const float* d0 = dryBuffer.getReadPointer (0);
        const float* d1 = dryBuffer.getReadPointer (1);
        const float* o0 = outBuffer.getReadPointer (0);
        const float* o1 = outBuffer.getReadPointer (1);

        int bw = bucketWrite.load (std::memory_order_relaxed);
        int filled = scopeFilledBuckets.load (std::memory_order_relaxed);
        for (int n = 0; n < numSamples; ++n)
        {
            const float i = 0.5f * (d0[n] + d1[n]);
            const float o = 0.5f * (o0[n] + o1[n]);

            accMinIn  = juce::jmin (accMinIn,  i);
            accMaxIn  = juce::jmax (accMaxIn,  i);
            accMinOut = juce::jmin (accMinOut, o);
            accMaxOut = juce::jmax (accMaxOut, o);

            if (++accCount >= kScopeDecimate)
            {
                scopeMinIn[(size_t) bw]  = accMinIn;
                scopeMaxIn[(size_t) bw]  = accMaxIn;
                scopeMinOut[(size_t) bw] = accMinOut;
                scopeMaxOut[(size_t) bw] = accMaxOut;

                accMinIn = accMinOut = 1.0e9f;
                accMaxIn = accMaxOut = -1.0e9f;
                accCount = 0;

                if (++bw >= scopeNumBuckets)
                    bw = 0;

                if (filled < scopeNumBuckets)
                    ++filled;
            }
        }
        bucketWrite.store (bw, std::memory_order_release);
        scopeFilledBuckets.store (filled, std::memory_order_release);
    }
}

//==============================================================================
void LunaraAudioProcessor::copyScopeBuckets (float* minIn, float* maxIn,
                                             float* minOut, float* maxOut,
                                             int numBuckets) const
{
    if (scopeNumBuckets <= 0 || numBuckets <= 0)
        return;

    numBuckets = juce::jlimit (1, scopeNumBuckets, numBuckets);

    const int bw = bucketWrite.load (std::memory_order_acquire);
    int idx = bw - numBuckets;
    while (idx < 0)
        idx += scopeNumBuckets;

    for (int i = 0; i < numBuckets; ++i)
    {
        const int j = (idx + i) % scopeNumBuckets;
        minIn[i]  = scopeMinIn[(size_t) j];
        maxIn[i]  = scopeMaxIn[(size_t) j];
        minOut[i] = scopeMinOut[(size_t) j];
        maxOut[i] = scopeMaxOut[(size_t) j];
    }
}

//==============================================================================
juce::AudioProcessorEditor* LunaraAudioProcessor::createEditor()
{
    return new LunaraAudioProcessorEditor (*this);
}

//==============================================================================
void LunaraAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LunaraAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            stateRestored.store (true, std::memory_order_relaxed);
        }
}

//==============================================================================
void LunaraAudioProcessor::randomizeParameters()
{
    using namespace lunara;
    auto& rng = juce::Random::getSystemRandom();

    auto setReal = [this] (const char* id, float realValue)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (realValue));
    };

    auto pick = [&rng] (std::initializer_list<float> options)
    {
        const int n = (int) options.size();
        const int idx = rng.nextInt (juce::jmax (1, n));
        return *(options.begin() + juce::jlimit (0, n - 1, idx));
    };

    // Character.
    setReal (ids::mode, (float) rng.nextInt (6));

    // Grains.
    setReal (ids::size,    60.0f + rng.nextFloat() * 290.0f);
    setReal (ids::density, 8.0f  + rng.nextFloat() * 52.0f);
    setReal (ids::spray,   rng.nextFloat() * 0.7f);
    setReal (ids::pitch,   pick ({ -12.0f, -7.0f, -5.0f, 0.0f, 0.0f, 0.0f, 3.0f, 5.0f, 7.0f, 12.0f }));
    setReal (ids::pitchMode, rng.nextFloat() < 0.7f ? 0.0f : 1.0f);

    // Delay (keep sync on; randomise the division and feel).
    setReal (ids::sync, 1.0f);
    setReal (ids::division, (float) rng.nextInt (11));
    setReal (ids::delay, 120.0f + rng.nextFloat() * 680.0f);
    setReal (ids::feedback, 0.15f + rng.nextFloat() * 0.55f);
    setReal (ids::pingpong, rng.nextFloat());
    setReal (ids::tone, 2000.0f + rng.nextFloat() * 7000.0f);

    // Space.
    setReal (ids::reverb, 0.30f + rng.nextFloat() * 0.55f);
    setReal (ids::decay,  0.40f + rng.nextFloat() * 0.55f);
    setReal (ids::cloud,  0.20f + rng.nextFloat() * 0.70f);

    // Output: gentle, never brutal.
    setReal (ids::hpf, 60.0f + rng.nextFloat() * 120.0f);   // 60..180 Hz
    setReal (ids::output, 0.0f);

    // Note: MIX and CHAOS are intentionally left untouched, so Random keeps the
    // user's balance between dry/wet and their chosen amount of chaos.
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LunaraAudioProcessor();
}
