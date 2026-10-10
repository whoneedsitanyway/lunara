#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PresetManager.h"
#include "ui/LunaraLookAndFeel.h"
#include "ui/WaveDisplay.h"

//==============================================================================
class LunaraAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit LunaraAudioProcessorEditor (LunaraAudioProcessor&);
    ~LunaraAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateAutoGainUi();
    void updateFxSourceButton();
    void cycleFxSource();
    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    void addKnob (Knob& k, const juce::String& paramId, const juce::String& name,
                  const juce::String& tooltip);
    void addCombo (juce::ComboBox& box, juce::Label& label,
                   const juce::String& paramId, const juce::String& name,
                   const juce::String& tooltip);
    void paintSectionLabel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);
    void paintScrew (juce::Graphics&, juce::Point<float> centre);
    void setCompact (bool shouldBeCompact);

    void refreshPresets();
    void loadPresetByName (const juce::String& name);
    void showSavePresetDialog();
    void applyTheme (bool shouldBeDark);
    void loadThemePreference();
    void refreshThemedColours();

    LunaraAudioProcessor& processorRef;
    lunara::LunaraLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow;
    PresetManager presetManager;

    Knob mix, size, density, spray, pitch, chaos, delay, feedback, pingpong,
         tone, reverb, decay, cloud, hpf, output;

    juce::ComboBox modeBox, pitchModeBox, divisionBox;
    juce::Label    modeLabel, pitchModeLabel, divisionLabel;

    juce::ToggleButton syncButton { "Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach;

    juce::ToggleButton autoGainButton { "Auto Gain" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttach;

    juce::ToggleButton limiterButton { "Limiter" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limiterAttach;

    juce::ToggleButton freezeButton { "Freeze" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttach;

    juce::ToggleButton reverseButton { "Reverse" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverseAttach;

    juce::ToggleButton extremeButton { "Stretch" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> extremeAttach;

    juce::ComboBox presetBox;
    lunara::IconButton saveButton  { lunara::IconButton::Icon::Save, "Save preset" };
    lunara::IconButton themeButton { lunara::IconButton::Icon::Moon, "Toggle theme" };
    lunara::IconButton logButton   { lunara::IconButton::Icon::Log,  "Open the debug log" };

    juce::TextButton   randomButton  { "RANDOM" };
    juce::TextButton   compactButton { "COMPACT" };
    juce::TextButton   halftimeButton { "HALF TIME" };
    juce::TextButton   fxSourceButton { "FX OUT" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> halftimeAttach;
    lunara::WaveDisplay waveDisplay;

    bool compact = false;
    bool darkTheme = true;
    bool lastAutoGainOn = false;   // cached Auto Gain state (dims OUTPUT when on)
    static constexpr int kFullHeight    = 760;
    static constexpr int kCompactHeight = 500;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;

    juce::Rectangle<int> enclosure, grainsArea, delayArea, spaceArea, outArea;
    juce::Rectangle<int> vizArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LunaraAudioProcessorEditor)
};
