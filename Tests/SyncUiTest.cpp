// Regression test for the SYNC / TIME interaction: when SYNC is on the delay
// time is locked to the host tempo, so the TIME knob must be disabled (dimmed)
// just like OUTPUT is disabled under Auto Gain. When SYNC is off it must be
// active again.

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <functional>
#include <memory>

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

namespace
{
    // Creates an editor for the processor, marks the state as restored (so the
    // editor keeps our parameter choices instead of applying the default factory
    // preset), and returns it.
    std::unique_ptr<juce::AudioProcessorEditor> makeEditor (LunaraAudioProcessor& proc)
    {
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);

        {
            juce::MemoryBlock mb;
            proc.getStateInformation (mb);
            proc.setStateInformation (mb.getData(), (int) mb.getSize());
        }

        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (editor->getWidth(), editor->getHeight());
        return editor;
    }

    juce::Slider* findTimeSlider (juce::Component& root)
    {
        juce::Slider* found = nullptr;
        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            for (auto* child : c.getChildren())
            {
                if (auto* s = dynamic_cast<juce::Slider*> (child))
                    if (s->getTooltip().startsWith ("TIME"))
                        found = s;
                walk (*child);
            }
        };
        walk (root);
        return found;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    int failures = 0;

    // --- SYNC ON: TIME must be disabled + dimmed -----------------------------
    {
        LunaraAudioProcessor proc;
        if (auto* p = proc.apvts.getParameter ("sync"))
            p->setValueNotifyingHost (1.0f);

        auto editor = makeEditor (proc);
        auto* time = findTimeSlider (*editor);
        if (time == nullptr)
        {
            std::cout << "FAIL: TIME slider not found" << std::endl;
            return 1;
        }

        const bool disabled = ! time->isEnabled();
        const bool dimmed   = time->getAlpha() < 0.9f;
        std::cout << "SYNC on : TIME enabled=" << (time->isEnabled() ? "yes" : "no")
                  << " alpha=" << time->getAlpha() << std::endl;
        if (! disabled || ! dimmed)
            ++failures;
    }

    // --- SYNC OFF: TIME must be active again --------------------------------
    {
        LunaraAudioProcessor proc;
        if (auto* p = proc.apvts.getParameter ("sync"))
            p->setValueNotifyingHost (0.0f);

        auto editor = makeEditor (proc);
        auto* time = findTimeSlider (*editor);
        if (time == nullptr)
        {
            std::cout << "FAIL: TIME slider not found" << std::endl;
            return 1;
        }

        const bool enabled = time->isEnabled();
        const bool opaque  = time->getAlpha() > 0.9f;
        std::cout << "SYNC off: TIME enabled=" << (time->isEnabled() ? "yes" : "no")
                  << " alpha=" << time->getAlpha() << std::endl;
        if (! enabled || ! opaque)
            ++failures;
    }

    if (failures > 0)
    {
        std::cout << "FAIL: TIME knob is not locked to the SYNC state" << std::endl;
        return 1;
    }

    std::cout << "PASS: TIME knob is disabled while SYNC is on and active when off" << std::endl;
    return 0;
}
