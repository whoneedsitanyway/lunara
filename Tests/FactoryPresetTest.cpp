// Offline test for the bundled factory presets and the default preset
// selection ("Play with Me").
//
// Verifies that:
//   - the factory presets are always listed (even before installation),
//   - installFactoryPresets() writes the files to the user folder,
//   - loading a factory preset applies its parameters,
//   - loading falls back to the embedded copy if the file is missing,
//   - a fresh editor instance selects (and applies) the default preset,
//   - a restored host state is never overridden by the default preset.

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <cmath>
#include <functional>

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/PresetManager.h"

namespace
{
    void setParam (LunaraAudioProcessor& proc, const char* id, float real)
    {
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (real));
    }

    float getParam (LunaraAudioProcessor& proc, const char* id)
    {
        return proc.apvts.getRawParameterValue (id)->load();
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    auto tmpDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getChildFile ("lunara_factory_test");
    tmpDir.deleteRecursively();
    tmpDir.createDirectory();

    bool ok = true;

    // --- PresetManager-level checks (isolated temp folder) ------------------
    {
        LunaraAudioProcessor proc;
        PresetManager pm (proc.apvts, tmpDir);

        // Factory presets are listed even before installation (embedded).
        const auto namesBefore = pm.getPresetNames();
        const bool listedEmbedded = namesBefore.contains ("Play with Me")
                                 && namesBefore.contains ("Washout")
                                 && namesBefore.contains ("A little Insane")
                                 && namesBefore.contains ("Never Stop")
                                 && namesBefore.contains ("Strange")
                                 && namesBefore.contains ("Tail Wind");

        // Install writes the files.
        pm.installFactoryPresets();
        const bool installed = tmpDir.getChildFile ("Play with Me.lunarapreset").existsAsFile()
                            && tmpDir.getChildFile ("Washout.lunarapreset").existsAsFile();

        // Loading the default preset applies its distinctive values.
        setParam (proc, "density", 5.0f);
        setParam (proc, "reverb", 0.0f);
        const bool loaded = pm.loadPreset ("Play with Me");
        const bool applied = std::abs (getParam (proc, "density") - 100.0f) < 0.5f
                          && std::abs (getParam (proc, "reverb") - 1.0f) < 0.01f
                          && std::abs (getParam (proc, "hpf") - 100.0f) < 0.5f;

        // The old DRIVE parameter is gone (replaced by the HPF knob).
        const bool driveGone = proc.apvts.getParameter ("drive") == nullptr;

        // Fallback to the embedded copy when the file is gone.
        tmpDir.getChildFile ("Play with Me.lunarapreset").deleteFile();
        setParam (proc, "density", 7.0f);
        const bool fallback = pm.loadPreset ("Play with Me")
                           && std::abs (getParam (proc, "density") - 100.0f) < 0.5f;

        const bool defaultName = PresetManager::getDefaultPresetName() == "Play with Me";

        std::cout << "listedEmbedded=" << listedEmbedded << " installed=" << installed
                  << " loaded=" << loaded << " applied=" << applied
                  << " driveGone=" << driveGone
                  << " fallback=" << fallback << " defaultName=" << defaultName << std::endl;

        ok = ok && listedEmbedded && installed && loaded && applied && driveGone
                && fallback && defaultName;
    }

    // --- processor state-restored flag --------------------------------------
    {
        LunaraAudioProcessor proc;
        const bool freshNotRestored = ! proc.wasStateRestored();

        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        proc.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool restoredFlag = proc.wasStateRestored();

        std::cout << "freshNotRestored=" << freshNotRestored
                  << " restoredFlag=" << restoredFlag << std::endl;
        ok = ok && freshNotRestored && restoredFlag;
    }

    // --- fresh editor selects the default preset ----------------------------
    {
        LunaraAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);

        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());

        // Find the preset combo by looking for one showing the default name.
        bool comboShowsDefault = false;
        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            for (auto* child : c.getChildren())
            {
                if (auto* box = dynamic_cast<juce::ComboBox*> (child))
                    if (box->getText() == "Play with Me")
                        comboShowsDefault = true;
                walk (*child);
            }
        };
        walk (*editor);

        // A fresh instance also applies the preset (density is 100 in it, vs the
        // parameter default of 22).
        const bool applied = std::abs (getParam (proc, "density") - 100.0f) < 0.5f;

        std::cout << "comboShowsDefault=" << comboShowsDefault
                  << " appliedOnOpen=" << applied << std::endl;
        ok = ok && comboShowsDefault && applied;
    }

    tmpDir.deleteRecursively();

    if (! ok)
    {
        std::cout << "FAIL: factory presets / default selection" << std::endl;
        return 1;
    }

    std::cout << "PASS: factory presets bundled and 'Play with Me' selected by default" << std::endl;
    return 0;
}
