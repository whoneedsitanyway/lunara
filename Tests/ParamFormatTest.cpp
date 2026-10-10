// Checks that float parameters display at most 4 decimals (trailing zeros
// trimmed) instead of the JUCE default of 7.

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

#include "../Source/PluginProcessor.h"

int main()
{
    LunaraAudioProcessor proc;

    struct Case { const char* id; float value; const char* expected; };
    const Case cases[] =
    {
        { "mix",      0.35f,   "0.35" },
        { "size",     150.0f,  "150"  },
        { "pitch",   -12.0f,   "-12"  },
        { "feedback", 0.775f,  "0.775" },
        { "output",   0.0f,    "0"    }
    };

    bool ok = true;
    for (const auto& c : cases)
    {
        if (auto* p = proc.apvts.getParameter (c.id))
        {
            p->setValueNotifyingHost (p->convertTo0to1 (c.value));
            const auto t = p->getText (p->getValue(), 0);
            const bool good = (t == c.expected);
            std::cout << "  " << c.id << " = " << c.value
                      << " -> \"" << t << "\""
                      << (good ? "" : juce::String (" (expected \"") + c.expected + "\")")
                      << std::endl;
            ok = ok && good;
        }
    }

    // No parameter may show more than 4 decimals.
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto t = rp->getText (rp->getValue(), 0);
            const auto dot = t.indexOfChar ('.');
            if (dot >= 0 && t.length() - dot - 1 > 4)
            {
                std::cout << "FAIL: " << rp->paramID << " shows too many decimals: "
                          << t << std::endl;
                ok = false;
            }
        }

    if (! ok)
    {
        std::cout << "FAIL: parameter text formatting" << std::endl;
        return 1;
    }

    std::cout << "PASS: parameters show at most 4 decimals" << std::endl;
    return 0;
}
