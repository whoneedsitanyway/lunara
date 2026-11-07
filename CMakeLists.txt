cmake_minimum_required(VERSION 3.22)

project(Lunara VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# -----------------------------------------------------------------------------
# JUCE (fetched locally into the build tree, nothing installed system-wide)
# -----------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(JUCE)

# -----------------------------------------------------------------------------
# Plugin target
# -----------------------------------------------------------------------------
juce_add_plugin(Lunara
    COMPANY_NAME              "Starshinov"
    PLUGIN_MANUFACTURER_CODE  Strv
    PLUGIN_CODE               Luna
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "Lunara"
    BUNDLE_ID                 "com.starshinov.lunara"
    DESCRIPTION               "Granular cloud delay / reverb for ambient textures"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    PLUGIN_IS_RTAS            FALSE
)

target_sources(Lunara PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DebugLogger.cpp
    Source/ui/LunaraLookAndFeel.cpp
    Source/ui/WaveDisplay.cpp
)

target_include_directories(Lunara PRIVATE Source)

target_compile_definitions(Lunara PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(Lunara
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Offline test helpers
#
#   lunara_add_dsp_test        - header-only DSP test (fast, no plugin sources)
#   lunara_add_processors_test - test that needs juce_audio_processors (APVTS)
#   lunara_add_plugin_test     - runs the real processor + editor
# -----------------------------------------------------------------------------
function(lunara_add_dsp_test name source)
    juce_add_console_app(${name} PRODUCT_NAME ${name})
    target_sources(${name} PRIVATE ${source})
    target_link_libraries(${name}
        PRIVATE
            juce::juce_audio_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )
endfunction()

function(lunara_add_processors_test name source)
    juce_add_console_app(${name} PRODUCT_NAME ${name})
    target_sources(${name} PRIVATE ${source})
    target_link_libraries(${name}
        PRIVATE
            juce::juce_audio_processors
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )
endfunction()

function(lunara_add_plugin_test name source)
    juce_add_console_app(${name} PRODUCT_NAME ${name})
    target_sources(${name} PRIVATE
        ${source}
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DebugLogger.cpp
        Source/ui/LunaraLookAndFeel.cpp
        Source/ui/WaveDisplay.cpp
    )
    target_include_directories(${name} PRIVATE Source)
    target_compile_definitions(${name} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JucePlugin_Name="Lunara"
        # Keeps the black-box log out of the real per-user file (see
        # DebugLogger::getLogFile) so running the tests never clobbers it.
        LUNARA_TEST_BUILD=1
    )
    target_link_libraries(${name}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )
endfunction()

# --- DSP / engine (header-only, fast to build) --------------------------------
lunara_add_dsp_test(LunaraDspTest    Tests/DspStressTest.cpp)
lunara_add_dsp_test(LunaraDriftTest  Tests/DriftTest.cpp)
lunara_add_dsp_test(LunaraFreezeTest Tests/FreezeTest.cpp)
lunara_add_dsp_test(LunaraGrainClickTest Tests/GrainClickTest.cpp)

# --- presets ------------------------------------------------------------------
lunara_add_processors_test(LunaraPresetTest Tests/PresetTest.cpp)

# --- full plugin (real processor + editor) ------------------------------------
lunara_add_plugin_test(LunaraHalftimeTest       Tests/HalftimeTest.cpp)
lunara_add_plugin_test(LunaraRunawayTest        Tests/RunawayTest.cpp)
lunara_add_plugin_test(LunaraAutoGainTest       Tests/AutoGainTest.cpp)
lunara_add_plugin_test(LunaraThemeRenderTest    Tests/ThemeRenderTest.cpp)
lunara_add_plugin_test(LunaraParamFormatTest    Tests/ParamFormatTest.cpp)
lunara_add_plugin_test(LunaraFxSourceTest       Tests/FxSourceTest.cpp)
lunara_add_plugin_test(LunaraFactoryPresetTest  Tests/FactoryPresetTest.cpp)
lunara_add_plugin_test(LunaraHpfTest            Tests/HpfTest.cpp)
lunara_add_plugin_test(LunaraStretchReverseTest Tests/StretchReverseTest.cpp)
lunara_add_plugin_test(LunaraSyncUiTest         Tests/SyncUiTest.cpp)
lunara_add_plugin_test(LunaraMixWetTest        Tests/MixWetTest.cpp)
