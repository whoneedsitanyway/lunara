#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
//  Factory presets bundled with the plugin.
//
//  The XML state of each preset is embedded in the binary so the presets are
//  always available (even on a fresh install with an empty user folder).  They
//  are installed to the user preset directory (see
//  PresetManager::installFactoryPresets) and are also used as a fallback when
//  loading a preset whose file is missing.
//
//  kFactoryPresetVersion is bumped whenever the bundled presets change; the
//  installer then refreshes the factory files in the user folder (while user
//  presets are never touched).
//==============================================================================
namespace lunara::factory
{
    inline constexpr int kFactoryPresetVersion = 3;

    struct Preset
    {
        const char* name;
        const char* xml;
    };

    inline const Preset* presets (int& count)
    {
        static const Preset kPresets[] =
        {
            { "Play with Me", R"LUNARA(
<PARAMETERS presetName="Play with Me">
  <PARAM id="autoGain" value="1.0"/>
  <PARAM id="chaos" value="0.0"/>
  <PARAM id="cloud" value="0.4338593780994415"/>
  <PARAM id="decay" value="1.0"/>
  <PARAM id="delay" value="408.1459045410156"/>
  <PARAM id="density" value="100.0"/>
  <PARAM id="division" value="9.0"/>
  <PARAM id="feedback" value="0.7969749569892883"/>
  <PARAM id="freeze" value="0.0"/>
  <PARAM id="fxSource" value="2.0"/>
  <PARAM id="halftime" value="0.0"/>
  <PARAM id="hpf" value="100.0"/>
  <PARAM id="limiter" value="1.0"/>
  <PARAM id="mix" value="0.3412968814373016"/>
  <PARAM id="mode" value="0.0"/>
  <PARAM id="output" value="7.152557373046875e-7"/>
  <PARAM id="pingpong" value="1.0"/>
  <PARAM id="pitch" value="0.0"/>
  <PARAM id="pitchMode" value="0.0"/>
  <PARAM id="reverb" value="1.0"/>
  <PARAM id="size" value="256.6499938964844"/>
  <PARAM id="spray" value="0.5279999971389771"/>
  <PARAM id="sync" value="1.0"/>
  <PARAM id="tone" value="2865.647705078125"/>
</PARAMETERS>
)LUNARA" },

            { "A little Insane", R"LUNARA(
<PARAMETERS presetName="A little Insane">
  <PARAM id="autoGain" value="1.0"/>
  <PARAM id="chaos" value="0.2164218723773956"/>
  <PARAM id="cloud" value="1.0"/>
  <PARAM id="decay" value="0.5565651059150696"/>
  <PARAM id="delay" value="1033.357543945312"/>
  <PARAM id="density" value="57.19089508056641"/>
  <PARAM id="division" value="9.0"/>
  <PARAM id="feedback" value="0.6738632321357727"/>
  <PARAM id="freeze" value="0.0"/>
  <PARAM id="fxSource" value="2.0"/>
  <PARAM id="halftime" value="0.0"/>
  <PARAM id="hpf" value="110.0"/>
  <PARAM id="limiter" value="1.0"/>
  <PARAM id="mix" value="0.829171895980835"/>
  <PARAM id="mode" value="0.0"/>
  <PARAM id="output" value="7.152557373046875e-7"/>
  <PARAM id="pingpong" value="0.5058628916740417"/>
  <PARAM id="pitch" value="12.0"/>
  <PARAM id="pitchMode" value="0.0"/>
  <PARAM id="reverb" value="0.6184390783309937"/>
  <PARAM id="size" value="207.3394927978516"/>
  <PARAM id="spray" value="0.7965144515037537"/>
  <PARAM id="sync" value="1.0"/>
  <PARAM id="tone" value="16051.5693359375"/>
</PARAMETERS>
)LUNARA" },

            { "Never Stop", R"LUNARA(
<PARAMETERS presetName="Never Stop">
  <PARAM id="autoGain" value="1.0"/>
  <PARAM id="chaos" value="0.0"/>
  <PARAM id="cloud" value="0.6703125238418579"/>
  <PARAM id="decay" value="0.6921250224113464"/>
  <PARAM id="delay" value="408.1459045410156"/>
  <PARAM id="density" value="73.27000427246094"/>
  <PARAM id="division" value="10.0"/>
  <PARAM id="feedback" value="0.7969749569892883"/>
  <PARAM id="freeze" value="0.0"/>
  <PARAM id="fxSource" value="2.0"/>
  <PARAM id="halftime" value="1.0"/>
  <PARAM id="hpf" value="80.0"/>
  <PARAM id="limiter" value="1.0"/>
  <PARAM id="mix" value="0.3412968814373016"/>
  <PARAM id="mode" value="0.0"/>
  <PARAM id="output" value="7.152557373046875e-7"/>
  <PARAM id="pingpong" value="1.0"/>
  <PARAM id="pitch" value="0.0"/>
  <PARAM id="pitchMode" value="0.0"/>
  <PARAM id="reverb" value="0.3582812547683716"/>
  <PARAM id="size" value="149.0625"/>
  <PARAM id="spray" value="0.5279999971389771"/>
  <PARAM id="sync" value="1.0"/>
  <PARAM id="tone" value="2865.647705078125"/>
</PARAMETERS>
)LUNARA" },

            { "Strange", R"LUNARA(
<PARAMETERS presetName="Strange">
  <PARAM id="autoGain" value="1.0"/>
  <PARAM id="chaos" value="0.0"/>
  <PARAM id="cloud" value="1.0"/>
  <PARAM id="decay" value="0.6780963540077209"/>
  <PARAM id="delay" value="597.4232177734375"/>
  <PARAM id="density" value="57.19089508056641"/>
  <PARAM id="division" value="2.0"/>
  <PARAM id="feedback" value="0.6738632321357727"/>
  <PARAM id="freeze" value="0.0"/>
  <PARAM id="fxSource" value="2.0"/>
  <PARAM id="halftime" value="0.0"/>
  <PARAM id="hpf" value="140.0"/>
  <PARAM id="limiter" value="1.0"/>
  <PARAM id="mix" value="0.3412968814373016"/>
  <PARAM id="mode" value="0.0"/>
  <PARAM id="output" value="7.152557373046875e-7"/>
  <PARAM id="pingpong" value="0.05700352042913437"/>
  <PARAM id="pitch" value="-5.0"/>
  <PARAM id="pitchMode" value="1.0"/>
  <PARAM id="reverb" value="0.7558922171592712"/>
  <PARAM id="size" value="84.94699859619141"/>
  <PARAM id="spray" value="0.5418113470077515"/>
  <PARAM id="sync" value="1.0"/>
  <PARAM id="tone" value="8095.7392578125"/>
</PARAMETERS>
)LUNARA" },

            { "Tail Wind", R"LUNARA(
<PARAMETERS presetName="Tail Wind">
  <PARAM id="autoGain" value="1.0"/>
  <PARAM id="chaos" value="0.0"/>
  <PARAM id="cloud" value="0.0"/>
  <PARAM id="decay" value="0.452156275510788"/>
  <PARAM id="delay" value="992.832763671875"/>
  <PARAM id="density" value="73.27000427246094"/>
  <PARAM id="division" value="8.0"/>
  <PARAM id="feedback" value="0.5517562031745911"/>
  <PARAM id="freeze" value="0.0"/>
  <PARAM id="fxSource" value="1.0"/>
  <PARAM id="halftime" value="0.0"/>
  <PARAM id="hpf" value="90.0"/>
  <PARAM id="limiter" value="1.0"/>
  <PARAM id="mix" value="0.4356875121593475"/>
  <PARAM id="mode" value="3.0"/>
  <PARAM id="output" value="7.152557373046875e-7"/>
  <PARAM id="pingpong" value="0.0"/>
  <PARAM id="pitch" value="12.0"/>
  <PARAM id="pitchMode" value="0.0"/>
  <PARAM id="reverb" value="0.6581093668937683"/>
  <PARAM id="size" value="134.5200042724609"/>
  <PARAM id="spray" value="0.8100937604904175"/>
  <PARAM id="sync" value="1.0"/>
  <PARAM id="tone" value="18000.0"/>
</PARAMETERS>
)LUNARA" },

            { "Washout", R"LUNARA(
<PARAMETERS presetName="Washout">
  <PARAM id="autoGain" value="1.0"/>
  <PARAM id="chaos" value="0.0"/>
  <PARAM id="cloud" value="1.0"/>
  <PARAM id="decay" value="0.6921250224113464"/>
  <PARAM id="delay" value="408.1459045410156"/>
  <PARAM id="density" value="73.27000427246094"/>
  <PARAM id="division" value="8.0"/>
  <PARAM id="feedback" value="0.7969749569892883"/>
  <PARAM id="freeze" value="0.0"/>
  <PARAM id="fxSource" value="2.0"/>
  <PARAM id="halftime" value="0.0"/>
  <PARAM id="hpf" value="120.0"/>
  <PARAM id="limiter" value="1.0"/>
  <PARAM id="mix" value="1.0"/>
  <PARAM id="mode" value="0.0"/>
  <PARAM id="output" value="7.152557373046875e-7"/>
  <PARAM id="pingpong" value="0.0"/>
  <PARAM id="pitch" value="0.0"/>
  <PARAM id="pitchMode" value="0.0"/>
  <PARAM id="reverb" value="1.0"/>
  <PARAM id="size" value="234.135009765625"/>
  <PARAM id="spray" value="0.8100937604904175"/>
  <PARAM id="sync" value="1.0"/>
  <PARAM id="tone" value="9871.2177734375"/>
</PARAMETERS>
)LUNARA" }
        };

        count = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
        return kPresets;
    }

    // The preset selected by default when the plugin is opened fresh.
    inline const char* defaultPresetName() { return "Play with Me"; }

    inline const char* findXml (const juce::String& name)
    {
        int n = 0;
        const auto* p = presets (n);
        for (int i = 0; i < n; ++i)
            if (name == p[i].name)
                return p[i].xml;
        return nullptr;
    }
}
