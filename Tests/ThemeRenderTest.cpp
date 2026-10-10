// Renders the editor in the light theme and checks that the knob labels and
// header buttons are drawn with DARK text (regression test for the labels
// staying near-white after a theme switch).

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <functional>

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/PresetManager.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // Force the light theme before the editor reads the preference, but restore
    // the user's original choice afterwards so the test has no side effects.
    auto dir = PresetManager::getAppDir();
    dir.createDirectory();
    auto themeFile = dir.getChildFile ("theme.txt");
    const bool hadTheme = themeFile.existsAsFile();
    const auto originalTheme = hadTheme ? themeFile.loadFileAsString() : juce::String();
    themeFile.replaceWithText ("light");

    LunaraAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);

    // Turn Auto Gain off so the OUTPUT label is not (intentionally) dimmed.
    if (auto* p = proc.apvts.getParameter ("autoGain"))
        p->setValueNotifyingHost (0.0f);

    // Simulate the host restoring a saved state, so the editor keeps our Auto
    // Gain choice instead of applying the default factory preset on open.
    {
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        proc.setStateInformation (mb.getData(), (int) mb.getSize());
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setSize (editor->getWidth(), editor->getHeight());

    juce::Image img (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    {
        juce::Graphics g (img);
        editor->paintEntireComponent (g, true);
    }

    auto out = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("lunara_light_theme.png");
    out.deleteFile();
    {
        juce::FileOutputStream os (out);
        juce::PNGImageFormat png;
        png.writeImageToStream (img, os);
    }

    // Collect every Label / TextButton in the editor tree.
    std::vector<juce::Label*> labels;
    std::vector<juce::TextButton*> buttons;
    std::vector<juce::ToggleButton*> toggles;
    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (auto* l = dynamic_cast<juce::Label*> (child))
                labels.push_back (l);
            if (auto* b = dynamic_cast<juce::TextButton*> (child))
                buttons.push_back (b);
            if (auto* t = dynamic_cast<juce::ToggleButton*> (child))
                toggles.push_back (t);
            walk (*child);
        }
    };
    walk (*editor);

    auto minLumaIn = [&img] (juce::Rectangle<int> r)
    {
        float minL = 1.0f;
        for (int y = r.getY(); y < r.getBottom(); ++y)
            for (int x = r.getX(); x < r.getRight(); ++x)
                minL = juce::jmin (minL, img.getPixelAt (x, y).getBrightness());
        return minL;
    };

    const juce::StringArray wanted { "REVERB", "DECAY", "OUTPUT", "HPF", "CLOUD", "TONE" };

    int checked = 0, tooLight = 0;
    for (auto* l : labels)
    {
        if (! wanted.contains (l->getText()))
            continue;

        const float minL = minLumaIn (l->getBounds().reduced (2));   // editor coords
        ++checked;
        std::cout << "  label '" << l->getText() << "' darkestLuma=" << minL << std::endl;
        if (minL > 0.35f)
            ++tooLight;
    }

    for (auto* b : buttons)
    {
        if (b->getButtonText() != "RANDOM" && b->getButtonText() != "COMPACT")
            continue;

        const float minL = minLumaIn (b->getBounds().reduced (6));
        ++checked;
        std::cout << "  button '" << b->getButtonText() << "' darkestLuma=" << minL << std::endl;
        if (minL > 0.35f)
            ++tooLight;
    }

    // OUTPUT checkboxes (Limiter / Auto Gain / Freeze / Stretch) and the GRAINS
    // Reverse toggle must be legible in the light theme too - the text sits on
    // the light panel background.
    const juce::StringArray wantedToggles { "Limiter", "Auto Gain", "Freeze", "Stretch", "Reverse" };
    for (auto* t : toggles)
    {
        if (! wantedToggles.contains (t->getButtonText()))
            continue;

        // The control must actually be laid out (not left at 0x0).
        if (t->getWidth() <= 0 || t->getHeight() <= 0)
        {
            std::cout << "  toggle '" << t->getButtonText() << "' has an empty bounds" << std::endl;
            ++tooLight;
            continue;
        }

        const float minL = minLumaIn (t->getBounds().reduced (2));
        ++checked;
        std::cout << "  toggle '" << t->getButtonText() << "' darkestLuma=" << minL << std::endl;
        if (minL > 0.35f)
            ++tooLight;
    }

    std::cout << "image=" << img.getWidth() << "x" << img.getHeight()
              << "  controlsChecked=" << checked
              << "  tooLight=" << tooLight
              << "  saved=" << out.getFullPathName() << std::endl;

    // --- dark theme: the numeric value read-out boxes must not glare --------
    // Regression test: the slider value box is a child Label whose colours are
    // copied from the LookAndFeel when it is created. If the slider is not
    // parented yet it used to fall back to JUCE's default LookAndFeel and keep a
    // bright white frame / text in the dark theme.
    themeFile.replaceWithText ("dark");
    int darkChecked = 0, darkTooBright = 0;
    {
        LunaraAudioProcessor proc2;
        proc2.setPlayConfigDetails (2, 2, 48000.0, 512);
        if (auto* p = proc2.apvts.getParameter ("autoGain"))
            p->setValueNotifyingHost (0.0f);

        std::unique_ptr<juce::AudioProcessorEditor> editor2 (proc2.createEditor());
        editor2->setSize (editor2->getWidth(), editor2->getHeight());

        juce::Image img2 (juce::Image::ARGB, editor2->getWidth(), editor2->getHeight(), true);
        {
            juce::Graphics g (img2);
            editor2->paintEntireComponent (g, true);
        }

        auto out2 = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("lunara_dark_theme.png");
        out2.deleteFile();
        {
            juce::FileOutputStream os (out2);
            juce::PNGImageFormat png;
            png.writeImageToStream (img2, os);
        }

        auto maxLumaIn = [&img2] (juce::Rectangle<int> r)
        {
            float maxL = 0.0f;
            r = r.getIntersection (img2.getBounds());
            for (int y = r.getY(); y < r.getBottom(); ++y)
                for (int x = r.getX(); x < r.getRight(); ++x)
                    maxL = juce::jmax (maxL, img2.getPixelAt (x, y).getBrightness());
            return maxL;
        };

        std::function<void (juce::Component&)> walkSliders = [&] (juce::Component& c)
        {
            for (auto* child : c.getChildren())
            {
                if (auto* s = dynamic_cast<juce::Slider*> (child))
                {
                    // The slider's value box is its only Label child.
                    for (auto* sc : s->getChildren())
                        if (auto* box = dynamic_cast<juce::Label*> (sc))
                        {
                            const auto r = editor2->getLocalArea (box, box->getLocalBounds());
                            const float maxL = maxLumaIn (r);
                            ++darkChecked;
                            std::cout << "  dark valueBox maxLuma=" << maxL << std::endl;
                            if (maxL > 0.70f)
                                ++darkTooBright;
                        }
                }
                walkSliders (*child);
            }
        };
        walkSliders (*editor2);

        std::cout << "dark image saved=" << out2.getFullPathName()
                  << "  valueBoxesChecked=" << darkChecked
                  << "  tooBright=" << darkTooBright << std::endl;
    }

    // Restore the user's theme preference.
    if (hadTheme)
        themeFile.replaceWithText (originalTheme);
    else
        themeFile.deleteFile();

    if (checked < 4 || tooLight > 0)
    {
        std::cout << "FAIL: light theme control text is not dark enough" << std::endl;
        return 1;
    }

    if (darkChecked < 4 || darkTooBright > 0)
    {
        std::cout << "FAIL: dark theme value read-out boxes are too bright" << std::endl;
        return 1;
    }

    std::cout << "PASS: light theme control text (labels + buttons) is dark" << std::endl;
    std::cout << "PASS: dark theme value read-out boxes are muted" << std::endl;
    return 0;
}
