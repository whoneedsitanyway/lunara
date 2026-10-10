cmake_minimum_required(VERSION 3.22)

project(Lunara VERSION 0.1.0 LANGUAGES C CXX)

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
# Offline DSP stress test (no plugin host required)
# -----------------------------------------------------------------------------
juce_add_console_app(LunaraDspTest PRODUCT_NAME "LunaraDspTest")

target_sources(LunaraDspTest PRIVATE Tests/DspStressTest.cpp)

target_link_libraries(LunaraDspTest
    PRIVATE
        juce::juce_audio_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Offline preset round-trip test
# -----------------------------------------------------------------------------
juce_add_console_app(LunaraPresetTest PRODUCT_NAME "LunaraPresetTest")

target_sources(LunaraPresetTest PRIVATE Tests/PresetTest.cpp)

target_link_libraries(LunaraPresetTest
    PRIVATE
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Offline reverb probe (measures the tail level vs parameters)
# -----------------------------------------------------------------------------
juce_add_console_app(LunaraReverbProbe PRODUCT_NAME "LunaraReverbProbe")

target_sources(LunaraReverbProbe PRIVATE Tests/ReverbProbe.cpp)

target_link_libraries(LunaraReverbProbe
    PRIVATE
        juce::juce_audio_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Offline drift test (slow pitch drift, not jitter)
# -----------------------------------------------------------------------------
juce_add_console_app(LunaraDriftTest PRODUCT_NAME "LunaraDriftTest")

target_sources(LunaraDriftTest PRIVATE Tests/DriftTest.cpp)

target_link_libraries(LunaraDriftTest
    PRIVATE
        juce::juce_audio_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Offline half-time test (runs the real processor)
# -----------------------------------------------------------------------------
juce_add_console_app(LunaraHalftimeTest PRODUCT_NAME "LunaraHalftimeTest")

target_sources(LunaraHalftimeTest PRIVATE
    Tests/HalftimeTest.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DebugLogger.cpp
    Source/ui/LunaraLookAndFeel.cpp
    Source/ui/WaveDisplay.cpp
)

target_include_directories(LunaraHalftimeTest PRIVATE Source)

target_compile_definitions(LunaraHalftimeTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JucePlugin_Name="Lunara"
)

target_link_libraries(LunaraHalftimeTest
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Offline runaway / black-box logger test (runs the real processor)
# -----------------------------------------------------------------------------
juce_add_console_app(LunaraRunawayTest PRODUCT_NAME "LunaraRunawayTest")

target_sources(LunaraRunawayTest PRIVATE
    Tests/RunawayTest.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DebugLogger.cpp
    Source/ui/LunaraLookAndFeel.cpp
    Source/ui/WaveDisplay.cpp
)

target_include_directories(LunaraRunawayTest PRIVATE Source)

target_compile_definitions(LunaraRunawayTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JucePlugin_Name="Lunara"
)

target_link_libraries(LunaraRunawayTest
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Offline auto-gain test (runs the real processor)
# -----------------------------------------------------------------------------
juce_add_console_app(LunaraAutoGainTest PRODUCT_NAME "LunaraAutoGainTest")

target_sources(LunaraAutoGainTest PRIVATE
    Tests/AutoGainTest.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DebugLogger.cpp
    Source/ui/LunaraLookAndFeel.cpp
    Source/ui/WaveDisplay.cpp
)

target_include_directories(LunaraAutoGainTest PRIVATE Source)

target_compile_definitions(LunaraAutoGainTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JucePlugin_Name="Lunara"
)

target_link_libraries(LunaraAutoGainTest
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Offline theme render test (checks light-theme text contrast)
# -----------------------------------------------------------------------------
juce_add_console_app(LunaraThemeRenderTest PRODUCT_NAME "LunaraThemeRenderTest")

target_sources(LunaraThemeRenderTest PRIVATE
    Tests/ThemeRenderTest.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DebugLogger.cpp
    Source/ui/LunaraLookAndFeel.cpp
    Source/ui/WaveDisplay.cpp
)

target_include_directories(LunaraThemeRenderTest PRIVATE Source)

target_compile_definitions(LunaraThemeRenderTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JucePlugin_Name="Lunara"
)

target_link_libraries(LunaraThemeRenderTest
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Offline parameter text formatting test (<= 4 decimals)
# -----------------------------------------------------------------------------
juce_add_console_app(LunaraParamFormatTest PRODUCT_NAME "LunaraParamFormatTest")

target_sources(LunaraParamFormatTest PRIVATE
    Tests/ParamFormatTest.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DebugLogger.cpp
    Source/ui/LunaraLookAndFeel.cpp
    Source/ui/WaveDisplay.cpp
)

target_include_directories(LunaraParamFormatTest PRIVATE Source)

target_compile_definitions(LunaraParamFormatTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JucePlugin_Name="Lunara"
)

target_link_libraries(LunaraParamFormatTest
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Offline FX-source routing test (MOOD-style IN / OUT / BOTH switch)
# -----------------------------------------------------------------------------
juce_add_console_app(LunaraFxSourceTest PRODUCT_NAME "LunaraFxSourceTest")

target_sources(LunaraFxSourceTest PRIVATE
    Tests/FxSourceTest.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DebugLogger.cpp
    Source/ui/LunaraLookAndFeel.cpp
    Source/ui/WaveDisplay.cpp
)

target_include_directories(LunaraFxSourceTest PRIVATE Source)

target_compile_definitions(LunaraFxSourceTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JucePlugin_Name="Lunara"
)

target_link_libraries(LunaraFxSourceTest
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Offline factory-preset / default-selection test (runs the real processor)
# -----------------------------------------------------------------------------
juce_add_console_app(LunaraFactoryPresetTest PRODUCT_NAME "LunaraFactoryPresetTest")

target_sources(LunaraFactoryPresetTest PRIVATE
    Tests/FactoryPresetTest.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DebugLogger.cpp
    Source/ui/LunaraLookAndFeel.cpp
    Source/ui/WaveDisplay.cpp
)

target_include_directories(LunaraFactoryPresetTest PRIVATE Source)

target_compile_definitions(LunaraFactoryPresetTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JucePlugin_Name="Lunara"
)

target_link_libraries(LunaraFactoryPresetTest
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Offline HPF (low-cut) test
# -----------------------------------------------------------------------------
juce_add_console_app(LunaraHpfTest PRODUCT_NAME "LunaraHpfTest")

target_sources(LunaraHpfTest PRIVATE
    Tests/HpfTest.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DebugLogger.cpp
    Source/ui/LunaraLookAndFeel.cpp
    Source/ui/WaveDisplay.cpp
)

target_include_directories(LunaraHpfTest PRIVATE Source)

target_compile_definitions(LunaraHpfTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JucePlugin_Name="Lunara"
)

target_link_libraries(LunaraHpfTest
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Offline freeze test (the granular cloud is held while frozen)
# -----------------------------------------------------------------------------
juce_add_console_app(LunaraFreezeTest PRODUCT_NAME "LunaraFreezeTest")

target_sources(LunaraFreezeTest PRIVATE Tests/FreezeTest.cpp)

target_link_libraries(LunaraFreezeTest
    PRIVATE
        juce::juce_audio_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Offline REVERSE / STRETCH test (runs the real processor)
# -----------------------------------------------------------------------------
juce_add_console_app(LunaraStretchReverseTest PRODUCT_NAME "LunaraStretchReverseTest")

target_sources(LunaraStretchReverseTest PRIVATE
    Tests/StretchReverseTest.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DebugLogger.cpp
    Source/ui/LunaraLookAndFeel.cpp
    Source/ui/WaveDisplay.cpp
)

target_include_directories(LunaraStretchReverseTest PRIVATE Source)

target_compile_definitions(LunaraStretchReverseTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JucePlugin_Name="Lunara"
)

target_link_libraries(LunaraStretchReverseTest
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
