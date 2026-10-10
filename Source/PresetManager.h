#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FactoryPresets.h"

//==============================================================================
//  Simple file-based preset manager.
//
//  Presets are stored as XML files (one per preset) in a per-user folder.
//  Saving copies the whole APVTS state; loading replaces it (which updates all
//  parameter attachments automatically).
//
//  Factory presets are embedded in the binary (see FactoryPresets.h): they are
//  installed into the user folder on first run and also merged into the preset
//  list / used as a load fallback, so they are always available.
//==============================================================================
class PresetManager
{
public:
    static constexpr const char* extension = ".lunarapreset";

    explicit PresetManager (juce::AudioProcessorValueTreeState& state,
                            juce::File directory = {})
        : apvts (state)
    {
        dir = directory != juce::File() ? directory : getAppDir().getChildFile ("Presets");
    }

    static juce::File getAppDir()
    {
       #if JUCE_MAC
        return juce::File ("~/Library/Application Support/Lunara");
       #else
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Lunara");
       #endif
    }

    juce::File getDirectory() const { return dir; }

    // The preset selected by default when the plugin is opened fresh.
    static juce::String getDefaultPresetName() { return lunara::factory::defaultPresetName(); }

    // Writes the factory presets to the user folder. On the first run (or after
    // the bundled presets have been updated) every factory file is (re)written;
    // afterwards missing files are restored. User presets are never touched.
    void installFactoryPresets() const
    {
        dir.createDirectory();

        auto stamp = dir.getChildFile ("factory_version.txt");
        const int installedVersion = stamp.existsAsFile()
                                         ? stamp.loadFileAsString().getIntValue()
                                         : 0;
        const bool refreshAll = installedVersion < lunara::factory::kFactoryPresetVersion;

        int count = 0;
        const auto* presets = lunara::factory::presets (count);
        for (int i = 0; i < count; ++i)
        {
            auto file = fileForName (presets[i].name);
            if (refreshAll || ! file.existsAsFile())
                file.replaceWithText (presets[i].xml);
        }

        stamp.replaceWithText (juce::String (lunara::factory::kFactoryPresetVersion));
    }

    juce::StringArray getPresetNames() const
    {
        juce::StringArray names;

        // Factory presets first (always available, even without the files).
        int count = 0;
        const auto* presets = lunara::factory::presets (count);
        for (int i = 0; i < count; ++i)
            names.addIfNotAlreadyThere (presets[i].name);

        // User presets from disk.
        if (dir.isDirectory())
            for (const auto& e : juce::RangedDirectoryIterator (dir, false, juce::String ("*") + extension))
                names.addIfNotAlreadyThere (e.getFile().getFileNameWithoutExtension());

        names.sort (true);
        return names;
    }

    bool savePreset (const juce::String& name)
    {
        const auto safe = sanitise (name);
        if (safe.isEmpty())
            return false;

        dir.createDirectory();
        auto xml = apvts.copyState().createXml();
        if (xml == nullptr)
            return false;

        xml->setAttribute ("presetName", safe);
        return xml->writeTo (fileForName (safe));
    }

    bool loadPreset (const juce::String& name)
    {
        const auto safe = sanitise (name);

        // A saved (possibly user-edited) file wins.
        const auto file = fileForName (safe);
        if (file.existsAsFile())
            if (auto xml = juce::XmlDocument::parse (file))
                if (xml->hasTagName (apvts.state.getType()))
                {
                    apvts.replaceState (juce::ValueTree::fromXml (*xml));
                    return true;
                }

        // Fallback: the embedded factory version (if this is a factory preset).
        if (const char* embedded = lunara::factory::findXml (safe))
            if (auto xml = juce::XmlDocument::parse (juce::String (embedded)))
                if (xml->hasTagName (apvts.state.getType()))
                {
                    apvts.replaceState (juce::ValueTree::fromXml (*xml));
                    return true;
                }

        return false;
    }

    juce::String getLastPreset() const { return lastPreset; }
    void setLastPreset (const juce::String& n) { lastPreset = n; }

private:
    static juce::String sanitise (const juce::String& name)
    {
        return name.retainCharacters ("abcdefghijklmnopqrstuvwxyz"
                                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                      "0123456789 _-()[].");
    }

    juce::File fileForName (const juce::String& name) const
    {
        return dir.getChildFile (name + extension);
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::File dir;
    juce::String lastPreset;
};
