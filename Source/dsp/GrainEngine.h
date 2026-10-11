#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>
#include <random>
#include <cmath>
#include <algorithm>

namespace lunara
{

//==============================================================================
//  Granular cloud engine.
//
//  Records the incoming signal into a circular buffer and continuously sprays
//  short, windowed grains that are read back at a (optionally pitch-shifted)
//  rate.  This is the "texture" core of the plugin - the part that turns any
//  input into drifting ambient clouds.
//==============================================================================
struct GrainParams
{
    float grainSizeMs    = 150.0f;   // 20 .. 500 ms
    float density        = 20.0f;    // grains per second
    float spray          = 0.35f;    // 0..1 random read-offset spread
    float pitchSemis     = 0.0f;     // -12 .. +12
    bool  pitchQuantize  = true;     // snap pitch to whole semitones
    float reverseProb    = 0.15f;    // 0..1 chance a grain plays backwards
    float stereoSpread   = 0.5f;     // 0..1 pan spread between grains
    float jitter         = 0.25f;    // 0..1 timing jitter of grain onset
    float chaos          = 0.0f;     // 0..1 detune + unpredictable artefacts
    float drift          = 0.0f;     // 0..1 slow tape-like pitch drift (wow/flutter)
    bool  frozen         = false;    // freeze: hold the buffer, ignore new input
};

class GrainEngine
{
public:
    GrainEngine() = default;

    static constexpr double kBufferSeconds = 2.5;

    // Current slow-drift pitch ratio (for tests / metering).
    float getDriftRatio() const noexcept { return lastDriftRatio; }

    void prepare (double sampleRate, int /*maxBlock*/, int numChannels)
    {
        sr = sampleRate;
        const int len = (int) (sampleRate * kBufferSeconds) + 4;
        buffer.setSize (juce::jmax (1, numChannels), len, false, true, false);
        buffer.clear();
        writePos = 0;
        for (auto& g : grains)
            g = {};
        activeGrains = 0;
        spawnAccumulator = 0.0f;
        rng.seed (0x5eed);
        driftPhase = flutterPhase = 0.0;
        driftRandom = 0.0f;
        lastDriftRatio = 1.0f;

        // Pre-compute the Hann window into a small table (linear interpolation
        // between points), so the hot loop does not call cos() per grain.
        for (int i = 0; i <= kWindowLutSize; ++i)
            windowLut[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                           * (float) i / (float) kWindowLutSize);
    }

    void reset()
    {
        buffer.clear();
        writePos = 0;
        for (auto& g : grains)
            g = {};
        activeGrains = 0;
        spawnAccumulator = 0.0f;
        driftPhase = flutterPhase = 0.0;
        driftRandom = 0.0f;
        lastDriftRatio = 1.0f;
    }

    void process (const float* inL, const float* inR,
                  float* outL, float* outR,
                  int numSamples, const GrainParams& p)
    {
        const int numCh = buffer.getNumChannels();
        const int bufLen = buffer.getNumSamples();

        const double maxGrain = juce::jmin ((double) p.grainSizeMs * 0.001 * sr,
                                            kBufferSeconds * sr * 0.8);
        const double grainLenSamples = juce::jlimit (16.0, maxGrain,
                                                     (double) p.grainSizeMs * 0.001 * sr);

        const int baseOffset = juce::jlimit (0, bufLen - 4, (int) grainLenSamples);

        const float density = juce::jlimit (0.5f, 120.0f, p.density);
        const float spray   = juce::jlimit (0.0f, 1.0f, p.spray);
        const float jitter  = juce::jlimit (0.0f, 1.0f, p.jitter);
        const float spread  = juce::jlimit (0.0f, 1.0f, p.stereoSpread);
        const float chaos   = juce::jlimit (0.0f, 1.0f, p.chaos);

        // Gain compensation: at high density/long grains dozens of Hann-windowed
        // grains overlap, so the raw sum would be many times unity and would slam
        // the downstream delay/reverb into saturation (audible as "noise").
        // Normalise by the expected overlap (RMS-style) so the cloud stays at a
        // roughly constant, musical level.
        const float overlap = density * (float) (grainLenSamples / sr);
        const float grainGain = 1.0f / std::sqrt (juce::jmax (1.0f, overlap * 0.62f));

        // ---- slow tape-like pitch drift (wow + flutter) --------------------
        // Instead of randomising each grain's pitch (which sounds jittery), the
        // whole cloud is gently detuned by a very slow oscillator, so the pitch
        // drifts over seconds like a worn cassette. Computed once per block and
        // linearly interpolated across it for smoothness.
        const float driftAmt = juce::jlimit (0.0f, 1.0f, p.drift);
        float driftStart = lastDriftRatio, driftEnd = lastDriftRatio;
        if (driftAmt > 0.001f)
        {
            const double dt = (double) numSamples / sr;
            driftPhase   += 0.08 * dt;              // wow     ~0.08 Hz (very slow)
            flutterPhase += 0.23 * dt;              // flutter ~0.23 Hz
            if (driftPhase   >= 1.0) driftPhase   -= std::floor (driftPhase);
            if (flutterPhase >= 1.0) flutterPhase -= std::floor (flutterPhase);

            // A heavily smoothed random walk adds a wandering, non-periodic
            // character without any fast jumps (the previous per-grain random
            // detune was what sounded harsh).
            driftRandom += (uniform (-1.0f, 1.0f) - driftRandom) * 0.02f;

            const float mod = 0.60f * std::sin (juce::MathConstants<float>::twoPi * (float) driftPhase)
                            + 0.20f * std::sin (juce::MathConstants<float>::twoPi * (float) flutterPhase)
                            + 0.30f * driftRandom;

            driftStart = driftEnd;
            driftEnd = std::exp2 (driftAmt * 0.32f * mod / 12.0f);   // up to ~±0.32 st
            lastDriftRatio = driftEnd;
        }
        else
        {
            lastDriftRatio = 1.0f;
            driftStart = driftEnd = 1.0f;
        }

        // Raw channel pointers avoid the bounds-checked getSample/setSample in
        // the hot loop.
        float* const bufL = buffer.getWritePointer (0);
        float* const bufR = (numCh > 1) ? buffer.getWritePointer (1) : nullptr;
        const float bufLenF = (float) bufLen;

        const float driftStep = (driftEnd - driftStart) / (float) juce::jmax (1, numSamples);
        float driftRatio = driftStart;

        for (int n = 0; n < numSamples; ++n)
        {
            // FREEZE: while frozen the circular buffer is not updated, so the
            // cloud keeps reading the held moment even when the input goes
            // silent - the granular texture sustains forever. The write pointer
            // still advances, so grains slowly scan the whole frozen buffer.
            if (! p.frozen)
            {
                bufL[writePos] = std::isfinite (inL[n]) ? inL[n] : 0.0f;
                if (bufR != nullptr)
                    bufR[writePos] = std::isfinite (inR[n]) ? inR[n] : 0.0f;
            }

            float wetL = 0.0f, wetR = 0.0f;

            // Skipped entirely while the cloud is silent (no active grains), so
            // an idle engine costs almost nothing per sample.
            if (activeGrains > 0)
            {
                for (auto& g : grains)
                {
                    if (! g.active)
                        continue;

                    const float w  = window (g.phase);
                    const float s  = readInterpolated (bufL, g.pos, bufLen);
                    const float sR = (bufR != nullptr) ? readInterpolated (bufR, g.pos, bufLen) : s;

                    wetL += s  * w * g.panL;
                    wetR += sR * w * g.panR;

                    g.pos += g.baseInc * driftRatio;
                    while (g.pos >= bufLenF) g.pos -= bufLenF;
                    while (g.pos < 0.0f)     g.pos += bufLenF;

                    g.phase += g.phaseInc;
                    if (g.phase >= 1.0f || ! std::isfinite (g.pos))
                    {
                        g.active = false;
                        --activeGrains;
                    }
                }
            }

            outL[n] = softClip (wetL * grainGain);
            outR[n] = softClip (wetR * grainGain);

            spawnAccumulator += density / (float) sr;
            while (spawnAccumulator >= 1.0f)
            {
                spawnAccumulator -= 1.0f;
                spawnGrain (p, grainLenSamples, baseOffset, spray, jitter, spread, chaos);
            }

            driftRatio += driftStep;

            if (++writePos >= bufLen)
                writePos = 0;
        }
    }

private:
    struct Grain
    {
        bool  active  = false;
        float pos     = 0.0f;
        float baseInc = 1.0f;    // pitch ratio before the global drift is applied
        float phase   = 0.0f;
        float phaseInc = 0.01f;
        float panL    = 0.7f;
        float panR    = 0.7f;
    };

    static constexpr int kMaxGrains = 96;
    static constexpr int kWindowLutSize = 1024;

    // Hann window via the pre-computed table (linear interpolation).
    float window (float phase) const noexcept
    {
        if (! std::isfinite (phase))
            return 0.0f;

        const float f = juce::jlimit (0.0f, (float) kWindowLutSize,
                                      phase * (float) kWindowLutSize);
        const int   i = juce::jlimit (0, kWindowLutSize - 1, (int) f);
        const float frac = f - (float) i;
        const float a = windowLut[(size_t) i];
        const float b = windowLut[(size_t) (i + 1)];
        return a + (b - a) * frac;
    }

    // Gentle peak tamer: transparent below 0.7, smoothly saturating above. Keeps
    // the cloud's RMS level but stops rare coherent grain peaks from slamming the
    // downstream feedback loops (which would ring for seconds as "noise").
    static float softClip (float x) noexcept
    {
        if (! std::isfinite (x)) return 0.0f;
        if (x >  0.7f) return  0.7f + 0.3f * std::tanh ((x - 0.7f) / 0.3f);
        if (x < -0.7f) return -0.7f + 0.3f * std::tanh ((x + 0.7f) / 0.3f);
        return x;
    }

    // Cubic (Catmull-Rom) interpolation: much cleaner than linear when grains
    // are pitch-shifted, at a small extra CPU cost.
    static float readInterpolated (const float* data, float pos, int bufLen) noexcept
    {
        if (bufLen < 4 || ! std::isfinite (pos))
            return 0.0f;

        const int i1 = (int) pos;
        const float frac = pos - (float) i1;
        const int i0 = (i1 > 0) ? i1 - 1 : bufLen - 1;
        const int i2 = (i1 + 1 < bufLen) ? i1 + 1 : 0;
        const int i3 = (i2 + 1 < bufLen) ? i2 + 1 : 0;

        const float a = data[i0];
        const float b = data[i1];
        const float c = data[i2];
        const float d = data[i3];

        const float c1 = 0.5f * (c - a);
        const float c2 = a - 2.5f * b + 2.0f * c - 0.5f * d;
        const float c3 = 0.5f * (d - a) + 1.5f * (b - c);
        return ((c3 * frac + c2) * frac + c1) * frac + b;
    }

    void spawnGrain (const GrainParams& p,
                     double grainLenSamples,
                     int baseOffset,
                     float spray, float jitter, float spread, float chaos)
    {
        Grain* slot = nullptr;
        for (auto& g : grains)
            if (! g.active) { slot = &g; break; }

        if (slot == nullptr)
            return;

        const int bufLen = buffer.getNumSamples();

        // Chaos gently widens spray/jitter (kept musical, not brutal).
        float effectiveSpray  = juce::jlimit (0.0f, 1.0f, spray  + chaos * 0.18f);
        float effectiveJitter = juce::jlimit (0.0f, 1.0f, jitter + chaos * 0.30f);

        const float maxSpray = effectiveSpray * (float) grainLenSamples * 5.0f;
        const float sprayOffset = uniform (-maxSpray, maxSpray);
        const float jitterSamples = effectiveJitter * (float) grainLenSamples * uniform (0.0f, 1.0f);

        // Occasional (rare) wild jump: read from a random spot in the buffer.
        const float wildJump = (chaos > 0.0f && uniform (0.0f, 1.0f) < chaos * 0.06f)
                               ? uniform (0.0f, (float) bufLen) : 0.0f;

        float start = (float) writePos - (float) baseOffset - sprayOffset - jitterSamples - wildJump;
        while (start < 0.0f)            start += (float) bufLen;
        while (start >= (float) bufLen) start -= (float) bufLen;

        // Chaos slightly raises the chance of reverse playback.
        const float revProb = juce::jlimit (0.0f, 1.0f, p.reverseProb + chaos * 0.20f);
        const bool reverse = uniform (0.0f, 1.0f) < revProb;

        // Pitch for this grain. Chaos picks a consonant interval (musical) when
        // pitch quantisation is on, or a small continuous detune when it is not.
        // (Per-grain random detune was removed in favour of the global drift.)
        float semis = p.pitchSemis;

        if (chaos > 0.0f && uniform (0.0f, 1.0f) < chaos * 0.8f)
        {
            if (p.pitchQuantize)
            {
                // Consonant offsets (octaves, fifths, fourths, thirds, sixth).
                static constexpr int intervals[] = { -12, -7, -5, -3, 3, 5, 7, 12 };
                semis += (float) intervals[(int) (uniform (0.0f, 8.0f))];
            }
            else
            {
                semis += uniform (-chaos * 2.0f, chaos * 2.0f);
            }
        }

        if (p.pitchQuantize)
            semis = std::round (semis);
        const float ratio = std::pow (2.0f, semis / 12.0f);

        // Chaos mildly varies the grain length (window).
        const double lenScale = juce::jlimit (0.5, 2.0,
                                              1.0 + uniform (-chaos * 0.30f, chaos * 0.30f));
        const double grainLen = juce::jlimit (16.0, (double) (bufLen - 4), grainLenSamples * lenScale);

        const float pan = uniform (-1.0f, 1.0f) * spread;
        const float panL = std::cos ((pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi);
        const float panR = std::sin ((pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi);

        slot->active   = true;
        slot->pos      = start;
        slot->baseInc  = reverse ? -ratio : ratio;
        slot->phase    = 0.0f;
        slot->phaseInc = (float) (1.0 / grainLen);
        slot->panL     = panL;
        slot->panR     = panR;
        ++activeGrains;
    }

    float uniform (float lo, float hi)
    {
        return std::uniform_real_distribution<float> (lo, hi) (rng);
    }

    juce::AudioBuffer<float> buffer;
    std::array<Grain, kMaxGrains> grains {};
    std::array<float, kWindowLutSize + 1> windowLut {};

    double sr = 48000.0;
    int    writePos = 0;
    int    activeGrains = 0;
    float  spawnAccumulator = 0.0f;
    std::mt19937 rng { 0x5eed };

    // Slow drift oscillators (wow / flutter / random walk).
    double driftPhase = 0.0, flutterPhase = 0.0;
    float  driftRandom = 0.0f;
    float  lastDriftRatio = 1.0f;
};

} // namespace lunara
