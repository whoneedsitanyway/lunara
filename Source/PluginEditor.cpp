#include "PluginEditor.h"

using namespace lunara;

//==============================================================================
LunaraAudioProcessorEditor::LunaraAudioProcessorEditor (LunaraAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p),
      tooltipWindow (this, 650), presetManager (p.apvts), waveDisplay (p)
{
    setLookAndFeel (&lookAndFeel);
    setSize (940, kFullHeight);

    // --- knobs ---------------------------------------------------------------
    addKnob (mix,       ids::mix,       "MIX",
             "MIX - balance between the dry and the processed (wet) signal");
    addKnob (size,      ids::size,      "SIZE",
             "SIZE - grain length in milliseconds. Larger = softer, more smeared");
    addKnob (density,   ids::density,   "DENSITY",
             "DENSITY - grains per second. Larger = denser cloud");
    addKnob (spray,     ids::spray,     "SPRAY",
             "SPRAY - spread of grain read positions. Larger = more chaotic texture");
    addKnob (pitch,     ids::pitch,     "PITCH",
             "PITCH - transposition from -12 to +12 semitones");
    addKnob (chaos,     ids::chaos,     "CHAOS",
             "CHAOS - detunes grains and adds unpredictable glitchy artefacts");
    addKnob (delay,     ids::delay,     "TIME",
             "TIME - delay time in ms (used when Sync is off)");
    addKnob (feedback,  ids::feedback,  "FEEDBACK",
             "FEEDBACK - number of delay repeats");
    addKnob (pingpong,  ids::pingpong,  "PING-PONG",
             "PING-PONG - cross-feed between the left and right channels");
    addKnob (tone,      ids::tone,      "TONE",
             "TONE - low-pass filter inside the feedback loop (darker/lighter tail)");
    addKnob (reverb,    ids::reverb,    "REVERB",
             "REVERB - how much signal is sent into the cloud reverb");
    addKnob (decay,     ids::decay,     "DECAY",
             "DECAY - length of the reverb tail");
    addKnob (cloud,     ids::cloud,     "CLOUD",
             "CLOUD - blur and smear of the reverb cloud");
    addKnob (hpf,       ids::hpf,       "HPF",
             "HPF - low-cut filter (20-500 Hz, 100 Hz by default): trims the low end so big reverb/delay tails stay tight");
    addKnob (output,    ids::output,    "OUTPUT",
             "OUTPUT - output level");

    // --- combos / toggles ----------------------------------------------------
    addCombo (modeBox,       modeLabel,       ids::mode,       "MODE",
              "MODE - character: Mosaic, Glide, Haze, Bloom, Nebula, Drift");
    addCombo (pitchModeBox,  pitchModeLabel,  ids::pitchMode,  "PITCH MODE",
              "PITCH MODE - Quantize: whole semitones; Smooth: continuous glide");
    addCombo (divisionBox,   divisionLabel,   ids::division,   "DIVISION",
              "DIVISION - musical division for the synced delay (1/4, 1/8, triplets, dotted)");

    // The mode combo is styled like the header buttons (no separate label).
    modeBox.setColour (juce::ComboBox::textColourId, findColour (LunaraLookAndFeel::accentColourId));
    modeLabel.setVisible (false);

    syncButton.setTooltip ("SYNC - lock the delay time to the host tempo (disables TIME)");
    addAndMakeVisible (syncButton);
    syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, ids::sync, syncButton);
    syncButton.onClick = [this] { updateSyncUi(); };
    updateSyncUi();

    autoGainButton.setTooltip ("AUTO GAIN - keep the output level matching the input (disables OUTPUT)");
    addAndMakeVisible (autoGainButton);
    autoGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, ids::autoGain, autoGainButton);
    autoGainButton.onClick = [this] { updateAutoGainUi(); };
    updateAutoGainUi();

    limiterButton.setTooltip ("LIMITER - transparent peak limiter on the output (off = bypass)");
    addAndMakeVisible (limiterButton);
    limiterAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, ids::limiter, limiterButton);

    freezeButton.setTooltip ("FREEZE - hold the granular cloud: the texture keeps playing even when the input goes silent");
    addAndMakeVisible (freezeButton);
    freezeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, ids::freeze, freezeButton);

    reverseButton.setTooltip ("REVERSE - play every grain backwards (a 100% reversed texture)");
    addAndMakeVisible (reverseButton);
    reverseAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, ids::reverse, reverseButton);

    extremeButton.setTooltip ("STRETCH - melt everything into a long, PaulStretch-like texture");
    addAndMakeVisible (extremeButton);
    extremeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, ids::extreme, extremeButton);

    startTimerHz (10);

    // --- header buttons (same style) ----------------------------------------
    auto styleHeaderButton = [] (juce::TextButton& b, const juce::String& tip)
    {
        b.setTooltip (tip);
        b.setConnectedEdges (0);
    };
    styleHeaderButton (randomButton,  "RANDOM - generate a new random texture");
    styleHeaderButton (compactButton, "Toggle compact mode (hide the oscilloscope)");
    styleHeaderButton (halftimeButton, "HALF TIME - everything at half speed (grains -12 st, delay x2, longer reverb)");
    styleHeaderButton (fxSourceButton, "FX SOURCE - what the effects process: IN (live input) / OUT (cloud) / BOTH");

    randomButton.onClick  = [this] { processorRef.randomizeParameters(); };
    compactButton.onClick = [this] { setCompact (! compact); };
    addAndMakeVisible (randomButton);
    addAndMakeVisible (compactButton);

    // FX SOURCE is a 3-way cycling button (IN / OUT / BOTH), like the routing
    // switch on a Chase Bliss MOOD MK2.
    fxSourceButton.onClick = [this] { cycleFxSource(); };
    addAndMakeVisible (fxSourceButton);
    updateFxSourceButton();

    // Half time is a latching toggle (button colour shows the on state).
    halftimeButton.setClickingTogglesState (true);
    halftimeButton.setColour (juce::TextButton::buttonOnColourId, findColour (LunaraLookAndFeel::accentDimColourId));
    addAndMakeVisible (halftimeButton);
    halftimeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, ids::halftime, halftimeButton);

    // --- presets -------------------------------------------------------------
    presetBox.setTooltip ("PRESET - select a saved preset");
    presetBox.setJustificationType (juce::Justification::centredLeft);
    presetBox.setColour (juce::ComboBox::textColourId, findColour (LunaraLookAndFeel::accentColourId));
    presetBox.onChange = [this]
    {
        const auto name = presetBox.getText();
        if (name.isNotEmpty() && name != "(no presets)")
            loadPresetByName (name);
    };
    addAndMakeVisible (presetBox);

    saveButton.setTooltip ("SAVE - save the current settings as a preset");
    saveButton.onClick = [this] { showSavePresetDialog(); };
    addAndMakeVisible (saveButton);

    // --- theme ---------------------------------------------------------------
    themeButton.setTooltip ("Toggle light / dark theme");
    themeButton.onClick = [this] { applyTheme (! darkTheme); };
    addAndMakeVisible (themeButton);

    // --- debug log -----------------------------------------------------------
    logButton.setTooltip ("Open the debug log (records level anomalies, e.g. runaway)");
    logButton.onClick = [this]
    {
        auto& logger = processorRef.getDebugLogger();
        if (! logger.getLogFile().existsAsFile())
            logger.clearLog();   // create an empty log with a header
        logger.getLogFile().revealToUser();
    };
    addAndMakeVisible (logButton);

    loadThemePreference();
    presetManager.installFactoryPresets();
    refreshPresets();

    // On a fresh instance (the host has not restored a saved project) start with
    // the default factory preset selected and applied. This must happen only
    // once per instance: hosts destroy and recreate the editor (e.g. switching
    // tracks), and re-applying on every open would wipe the user's edits. A
    // restored project is never overridden.
    if (! processorRef.wasStateRestored() && ! processorRef.wasDefaultPresetApplied())
    {
        loadPresetByName (PresetManager::getDefaultPresetName());
        processorRef.markDefaultPresetApplied();
    }

    // --- oscilloscope --------------------------------------------------------
    addAndMakeVisible (waveDisplay);
}

LunaraAudioProcessorEditor::~LunaraAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void LunaraAudioProcessorEditor::timerCallback()
{
    // Keeps the OUTPUT knob in sync with the Auto Gain state (also after a
    // preset load, which changes the parameter without a click).
    updateAutoGainUi();

    // Keeps the TIME knob in sync with the SYNC state (also after a preset load,
    // which changes the parameter without a click).
    updateSyncUi();

    // Keeps the FX SOURCE label in sync with the parameter (e.g. after a preset
    // load or a host automation change).
    updateFxSourceButton();

    // Red badge on the LOG button once the black box has captured an anomaly.
    logButton.setWarning (processorRef.getDebugLogger().hasAnomaly());
}

void LunaraAudioProcessorEditor::updateAutoGainUi()
{
    const bool on = processorRef.apvts.getRawParameterValue (ids::autoGain)->load() > 0.5f;
    if (on == lastAutoGainOn)
        return;

    lastAutoGainOn = on;
    output.slider.setEnabled (! on);
    output.slider.setAlpha (on ? 0.35f : 1.0f);
    output.label.setAlpha (on ? 0.45f : 1.0f);
}

// When SYNC is on the delay time is locked to the host tempo, so the TIME knob
// has no effect. Dim and disable it (like OUTPUT under Auto Gain) to make that
// obvious; the DIVISION combo is the control that matters in this state.
void LunaraAudioProcessorEditor::updateSyncUi()
{
    const bool on = processorRef.apvts.getRawParameterValue (ids::sync)->load() > 0.5f;
    if (on == lastSyncOn)
        return;

    lastSyncOn = on;
    delay.slider.setEnabled (! on);
    delay.slider.setAlpha (on ? 0.35f : 1.0f);
    delay.label.setAlpha (on ? 0.45f : 1.0f);
}

//==============================================================================
void LunaraAudioProcessorEditor::updateFxSourceButton()
{
    if (auto* p = processorRef.apvts.getParameter (ids::fxSource))
    {
        const auto text = "FX " + p->getCurrentValueAsText();
        if (fxSourceButton.getButtonText() != text)
            fxSourceButton.setButtonText (text);
    }
}

void LunaraAudioProcessorEditor::cycleFxSource()
{
    if (auto* p = processorRef.apvts.getParameter (ids::fxSource))
    {
        const int cur = (int) processorRef.apvts.getRawParameterValue (ids::fxSource)->load();
        const int next = (cur + 1) % 3;
        p->setValueNotifyingHost (p->convertTo0to1 ((float) next));
        updateFxSourceButton();
    }
}

//==============================================================================
void LunaraAudioProcessorEditor::setCompact (bool shouldBeCompact)
{
    compact = shouldBeCompact;
    waveDisplay.setVisible (! compact);
    compactButton.setButtonText (compact ? "FULL" : "COMPACT");
    setSize (getWidth(), compact ? kCompactHeight : kFullHeight);
    resized();
}

//==============================================================================
void LunaraAudioProcessorEditor::refreshPresets()
{
    presetBox.clear (juce::dontSendNotification);

    const auto names = presetManager.getPresetNames();
    if (names.isEmpty())
    {
        presetBox.addItem ("(no presets)", 1);
        presetBox.setText ("(no presets)", juce::dontSendNotification);
        return;
    }

    int id = 1;
    for (const auto& n : names)
        presetBox.addItem (n, id++);

    // Prefer the last-used preset, then the default factory preset, then the
    // first available name. The last-used name lives on the processor so it
    // survives the editor being recreated by the host.
    const auto last = processorRef.getLastPresetName();
    const auto def  = PresetManager::getDefaultPresetName();
    const auto toSelect = names.contains (last) ? last
                        : names.contains (def)  ? def
                                                : names[0];
    presetBox.setText (toSelect, juce::dontSendNotification);
    processorRef.setLastPresetName (toSelect);
}

void LunaraAudioProcessorEditor::loadPresetByName (const juce::String& name)
{
    if (presetManager.loadPreset (name))
    {
        processorRef.setLastPresetName (name);
        repaint();
    }
}

void LunaraAudioProcessorEditor::showSavePresetDialog()
{
    auto* aw = new juce::AlertWindow ("Save Preset",
                                      "Enter a name for this preset:",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", processorRef.getLastPresetName(), "Name:");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, aw] (int result)
        {
            if (result == 1)
            {
                const auto name = aw->getTextEditorContents ("name").trim();
                if (name.isNotEmpty() && presetManager.savePreset (name))
                {
                    processorRef.setLastPresetName (name);
                    refreshPresets();
                    presetBox.setText (name, juce::dontSendNotification);
                }
            }
        }), true);
}

//==============================================================================
void LunaraAudioProcessorEditor::refreshThemedColours()
{
    // Colours set with an explicit setColour() are *overrides* that ignore the
    // LookAndFeel, so they must be re-applied whenever the theme changes -
    // otherwise labels/combo text baked at construction keep the old theme's
    // colour (e.g. near-white text on the light theme, which blends away).
    const auto text = findColour (LunaraLookAndFeel::textColourId);
    const auto accent = findColour (LunaraLookAndFeel::accentColourId);
    const auto accentDim = findColour (LunaraLookAndFeel::accentDimColourId);

    for (auto* k : { &mix, &size, &density, &spray, &pitch, &chaos,
                     &delay, &feedback, &pingpong, &tone,
                     &reverb, &decay, &cloud, &hpf, &output })
        k->label.setColour (juce::Label::textColourId, text);

    for (auto* l : { &pitchModeLabel, &divisionLabel })
        l->setColour (juce::Label::textColourId, accentDim);

    modeBox.setColour (juce::ComboBox::textColourId, accent);
    presetBox.setColour (juce::ComboBox::textColourId, accent);
    halftimeButton.setColour (juce::TextButton::buttonOnColourId, accentDim);

    repaint();
}

//==============================================================================
void LunaraAudioProcessorEditor::applyTheme (bool shouldBeDark)
{
    darkTheme = shouldBeDark;
    lookAndFeel.setDark (shouldBeDark);
    themeButton.setIcon (shouldBeDark ? IconButton::Icon::Sun : IconButton::Icon::Moon);
    refreshThemedColours();
    sendLookAndFeelChange();
    repaint();

    // Persist the choice next to the presets.
    auto dir = PresetManager::getAppDir();
    dir.createDirectory();
    dir.getChildFile ("theme.txt").replaceWithText (shouldBeDark ? "dark" : "light");
}

void LunaraAudioProcessorEditor::loadThemePreference()
{
    const auto f = PresetManager::getAppDir().getChildFile ("theme.txt");
    darkTheme = true;
    if (f.existsAsFile())
        darkTheme = f.loadFileAsString().trim().toLowerCase() != "light";

    lookAndFeel.setDark (darkTheme);
    themeButton.setIcon (darkTheme ? IconButton::Icon::Sun : IconButton::Icon::Moon);
    refreshThemedColours();
}

//==============================================================================
void LunaraAudioProcessorEditor::addKnob (Knob& k, const juce::String& paramId,
                                          const juce::String& name,
                                          const juce::String& tooltip)
{
    // Parent the slider *before* configuring its text box. The value box is a
    // child Label whose colours are copied from the slider's LookAndFeel when it
    // is created; if the slider has no parent yet it would resolve to JUCE's
    // default LookAndFeel and keep its bright white text / outline forever.
    addAndMakeVisible (k.slider);

    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
    k.slider.setTooltip (tooltip);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (juce::FontOptions (10.5f)));
    addAndMakeVisible (k.label);

    k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, paramId, k.slider);
}

void LunaraAudioProcessorEditor::addCombo (juce::ComboBox& box, juce::Label& label,
                                           const juce::String& paramId,
                                           const juce::String& name,
                                           const juce::String& tooltip)
{
    addAndMakeVisible (box);
    box.setJustificationType (juce::Justification::centred);
    box.setTooltip (tooltip);
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    addAndMakeVisible (label);

    box.addItemList (processorRef.apvts.getParameter (paramId)->getAllValueStrings(), 1);

    comboAttachments.emplace_back (
        std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            processorRef.apvts, paramId, box));
}

//==============================================================================
void LunaraAudioProcessorEditor::paintScrew (juce::Graphics& g, juce::Point<float> centre)
{
    const float r = 7.0f;
    juce::Rectangle<float> c (r * 2.0f, r * 2.0f);
    c.setCentre (centre);

    juce::ColourGradient grad (juce::Colours::white.withAlpha (0.22f), c.getX(), c.getY(),
                               juce::Colours::black.withAlpha (0.45f), c.getRight(), c.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (c);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (c, 1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawLine (centre.x - r * 0.55f, centre.y - r * 0.55f,
                centre.x + r * 0.55f, centre.y + r * 0.55f, 1.8f);
}

void LunaraAudioProcessorEditor::paintSectionLabel (juce::Graphics& g, juce::Rectangle<int> r,
                                                    const juce::String& title)
{
    auto header = r.reduced (14, 10).removeFromTop (16);
    g.setColour (findColour (LunaraLookAndFeel::accentDimColourId));
    g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    g.drawText (title, header, juce::Justification::centredLeft, false);

    g.setColour (findColour (LunaraLookAndFeel::accentDimColourId).withAlpha (0.35f));
    g.drawLine ((float) header.getX(), (float) header.getBottom() + 1.0f,
                (float) header.getRight(), (float) header.getBottom() + 1.0f, 1.0f);
}

//==============================================================================
void LunaraAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto encColour = findColour (LunaraLookAndFeel::enclosureColourId);
    g.fillAll (encColour);

    const auto encF = enclosure.toFloat();

    {
        juce::ColourGradient body (encColour.brighter (0.14f), encF.getX(), encF.getY(),
                                   encColour.darker (0.35f), encF.getRight(), encF.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (encF, 14.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (darkTheme ? 0.07f : 0.35f));
    g.drawRoundedRectangle (encF.reduced (1.5f), 12.5f, 1.5f);
    g.setColour (juce::Colours::black.withAlpha (darkTheme ? 0.45f : 0.20f));
    g.drawRoundedRectangle (encF.reduced (0.5f), 14.0f, 1.5f);

    paintScrew (g, { encF.getX() + 22.0f, encF.getY() + 22.0f });
    paintScrew (g, { encF.getRight() - 22.0f, encF.getY() + 22.0f });
    paintScrew (g, { encF.getX() + 22.0f, encF.getBottom() - 22.0f });
    paintScrew (g, { encF.getRight() - 22.0f, encF.getBottom() - 22.0f });

    g.setColour (findColour (LunaraLookAndFeel::textColourId));
    g.setFont (juce::Font (juce::FontOptions (34.0f).withStyle ("Bold")));
    g.drawText ("LUNARA", juce::roundToInt (encF.getX() + 34.0f), juce::roundToInt (encF.getY() + 20.0f),
                320, 36, juce::Justification::centredLeft, false);

    g.setColour (findColour (LunaraLookAndFeel::textDimColourId));
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawText ("GRANULAR ENGINE",
                juce::roundToInt (encF.getX() + 36.0f), juce::roundToInt (encF.getY() + 56.0f),
                380, 16, juce::Justification::centredLeft, false);

    // Version tag. JucePlugin_VersionString is defined for the plugin targets;
    // the offline test builds compile this block out.
   #ifdef JucePlugin_VersionString
    g.setColour (findColour (LunaraLookAndFeel::textDimColourId).withAlpha (0.75f));
    g.drawText ("v" JucePlugin_VersionString,
                juce::roundToInt (encF.getRight() - 34.0f - 120.0f),
                juce::roundToInt (encF.getY() + 56.0f),
                120, 16, juce::Justification::centredRight, false);
   #endif

    paintSectionLabel (g, grainsArea, "GRAINS");
    paintSectionLabel (g, delayArea,  "DELAY");
    paintSectionLabel (g, spaceArea,  "SPACE");
    paintSectionLabel (g, outArea,    "OUTPUT");
}

//==============================================================================
void LunaraAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    enclosure = area;

    auto inner = area.reduced (34, 28);

    // Reserve the painted title zone, then a single control row.
    inner.removeFromTop (44);

    auto row = inner.removeFromTop (30);

    // Left: preset selector + save + theme.
    presetBox.setBounds (row.removeFromLeft (200));
    row.removeFromLeft (8);
    saveButton.setBounds (row.removeFromLeft (30));
    row.removeFromLeft (6);
    themeButton.setBounds (row.removeFromLeft (30));
    row.removeFromLeft (6);
    logButton.setBounds (row.removeFromLeft (30));

    // Right: HALF TIME / RANDOM / COMPACT / MODE / FX SOURCE (same size).
    const int btnW = 100;
    const int btnGap = 12;
    modeBox.setBounds (row.removeFromRight (btnW));
    row.removeFromRight (btnGap);
    compactButton.setBounds (row.removeFromRight (btnW));
    row.removeFromRight (btnGap);
    randomButton.setBounds (row.removeFromRight (btnW));
    row.removeFromRight (btnGap);
    halftimeButton.setBounds (row.removeFromRight (btnW));
    row.removeFromRight (btnGap);
    fxSourceButton.setBounds (row.removeFromRight (btnW));

    inner.removeFromTop (10);

    // --- oscilloscope row (hidden in compact mode) --------------------------
    if (! compact)
    {
        vizArea = inner.removeFromBottom (150);
        inner.removeFromBottom (10);
        waveDisplay.setBounds (vizArea.reduced (6, 0));
    }
    else
    {
        vizArea = {};
    }

    // --- knob columns --------------------------------------------------------
    const int gap = 14;
    const int colW = (inner.getWidth() - gap * 3) / 4;

    grainsArea = inner.removeFromLeft (colW);
    inner.removeFromLeft (gap);
    delayArea = inner.removeFromLeft (colW);
    inner.removeFromLeft (gap);
    spaceArea = inner.removeFromLeft (colW);
    inner.removeFromLeft (gap);
    outArea = inner;

    auto placeGrid = [] (std::initializer_list<Knob*> ks, juce::Rectangle<int> r, int cols)
    {
        const int n = (int) ks.size();
        const int rows = (n + cols - 1) / cols;
        const int cw = r.getWidth() / cols;
        const int ch = r.getHeight() / rows;
        int i = 0;
        for (auto* k : ks)
        {
            const int c = i % cols, row2 = i / cols;
            juce::Rectangle<int> cell (r.getX() + c * cw, r.getY() + row2 * ch, cw, ch);
            auto lab = cell.removeFromBottom (15);
            k->label.setBounds (lab);
            k->slider.setBounds (cell.reduced (6, 2));
            ++i;
        }
    };

    // GRAINS: 5 knobs (2 cols) + [PITCH MODE | REVERSE] row.
    {
        auto col = grainsArea.reduced (10, 8);
        col.removeFromTop (20);
        auto bottom = col.removeFromBottom (50);
        col.removeFromBottom (8);   // small gap so the row is not tight on the knobs
        placeGrid ({ &size, &density, &spray, &pitch, &chaos }, col, 2);

        pitchModeLabel.setBounds (bottom.removeFromTop (14));

        auto comboRow = bottom.removeFromTop (28);
        reverseButton.setBounds (comboRow.removeFromRight (juce::roundToInt ((float) comboRow.getWidth() * 0.42f)));
        comboRow.removeFromRight (8);
        pitchModeBox.setBounds (comboRow);
    }

    // DELAY: 2x2 knobs + [DIVISION | SYNC] row.
    {
        auto col = delayArea.reduced (10, 8);
        col.removeFromTop (20);
        auto bottom = col.removeFromBottom (52);
        col.removeFromBottom (8);   // small gap above the division/sync row

        auto labelRow = bottom.removeFromTop (14);
        divisionLabel.setBounds (labelRow.removeFromLeft (juce::roundToInt ((float) col.getWidth() * 0.6f)));

        auto comboRow = bottom.removeFromTop (30);
        auto divPart = comboRow.removeFromLeft (juce::roundToInt ((float) comboRow.getWidth() * 0.6f));
        divisionBox.setBounds (divPart);
        comboRow.removeFromLeft (8);
        syncButton.setBounds (comboRow);

        placeGrid ({ &delay, &feedback, &pingpong, &tone }, col, 2);
    }

    // SPACE: 3 knobs stacked.
    {
        auto col = spaceArea.reduced (10, 8);
        col.removeFromTop (20);
        placeGrid ({ &reverb, &decay, &cloud }, col, 1);
    }

    // OUTPUT: 3 knobs stacked + [LIMITER | AUTO GAIN] / [FREEZE | STRETCH] block.
    {
        auto col = outArea.reduced (10, 8);
        col.removeFromTop (20);
        auto bottom = col.removeFromBottom (52);
        col.removeFromBottom (8);   // small gap above the checkbox block

        const int half = juce::roundToInt ((float) col.getWidth() * 0.5f);

        auto checkRow = bottom.removeFromTop (22);
        limiterButton.setBounds (checkRow.removeFromLeft (half));
        checkRow.removeFromLeft (8);
        autoGainButton.setBounds (checkRow);

        bottom.removeFromTop (8);
        auto toggleRow = bottom.removeFromTop (22);
        freezeButton.setBounds (toggleRow.removeFromLeft (half));
        toggleRow.removeFromLeft (8);
        extremeButton.setBounds (toggleRow);

        placeGrid ({ &hpf, &output, &mix }, col, 1);
    }
}
