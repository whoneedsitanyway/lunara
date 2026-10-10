// Offline round-trip test for the PresetManager: save the current parameter
// state, change it, load it back and verify the values are restored.

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <cmath>

#include "../Source/PresetManager.h"

namespace
{
    struct DummyProcessor : juce::AudioProcessor
    {
        DummyProcessor() : AudioProcessor (BusesProperties()) {}
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        const juce::String getName() const override { return "Dummy"; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };
}

int main()
{
    DummyProcessor proc;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> ("size", "Size", 20.0f, 500.0f, 150.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("feedback", "Feedback", 0.0f, 1.0f, 0.35f));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("mode", "Mode",
                 juce::StringArray { "A", "B", "C" }, 0));

    juce::AudioProcessorValueTreeState apvts (proc, nullptr, "PARAMETERS", std::move (layout));

    auto tmpDir = juce::File::getCurrentWorkingDirectory().getChildFile ("preset_test_tmp");
    tmpDir.deleteRecursively();
    tmpDir.createDirectory();

    PresetManager pm (apvts, tmpDir);

    auto setVal = [&] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    auto getVal = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    setVal ("size", 321.0f);
    setVal ("feedback", 0.77f);
    setVal ("mode", 2.0f);

    const bool saved = pm.savePreset ("Unit Test");

    // Names listing should now contain our preset.
    const bool listed = pm.getPresetNames().contains ("Unit Test");

    setVal ("size", 100.0f);
    setVal ("feedback", 0.10f);
    setVal ("mode", 0.0f);

    const bool loaded = pm.loadPreset ("Unit Test");

    const float s = getVal ("size"), f = getVal ("feedback"), m = getVal ("mode");
    const bool valuesOk = std::abs (s - 321.0f) < 0.5f
                       && std::abs (f - 0.77f) < 0.001f
                       && std::abs (m - 2.0f)  < 0.01f;

    tmpDir.deleteRecursively();

    std::cout << "saved=" << saved << " listed=" << listed << " loaded=" << loaded
              << " size=" << s << " fb=" << f << " mode=" << m << std::endl;

    if (! (saved && listed && loaded && valuesOk))
    {
        std::cout << "FAIL: preset round-trip" << std::endl;
        return 1;
    }

    std::cout << "PASS: preset round-trip" << std::endl;
    return 0;
}
