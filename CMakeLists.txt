cmake_minimum_required(VERSION 3.22)
project(VaporKey VERSION 0.4.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Link-time optimization for the plugin. Default ON for local builds (release
# binaries get the perf/size benefit); CI passes -DVAPORKEY_LTO=OFF because
# LTO-linking the plugin (JUCE + juce_dsp Oversampling instantiations + plate
# reverb) peaks well above the free GitHub runners' RAM.
option(VAPORKEY_LTO "Enable link-time optimization for the plugin" ON)

# Build-wide flags. We deliberately do NOT enable -ffast-math / /fp:fast: the
# audio path relies on IEEE-754 semantics + juce::ScopedNoDenormals (see
# PluginProcessor::processBlock) and per-stage denormal guards in FxChain.
# Per-target warning levels live on the targets below so JUCE's own
# translation units don't get pulled into our stricter set.
if(MSVC)
    add_compile_options(/MP)
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VaporKey
    COMPANY_NAME              "RocketBombs"
    BUNDLE_ID                 com.rocketbombs.vaporkey
    PLUGIN_MANUFACTURER_CODE  Rkbm
    PLUGIN_CODE               Vpky
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "VaporKey"
    DESCRIPTION               "Wavetable synth with analog warmth"
    VERSION                   ${PROJECT_VERSION}
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           "Instrument" "Synth"
)

juce_generate_juce_header(VaporKey)

# Factory presets live in Source/Presets.json so future contributors can edit
# them as plain data. JUCE bakes the file into a BinaryData blob; Presets.cpp
# parses it lazily on first access.
juce_add_binary_data(VaporKeyData SOURCES
    Source/Presets.json
)

target_sources(VaporKey PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/SynthEngine.cpp
    Source/SynthVoice.cpp
    Source/Arpeggiator.cpp
    Source/FxChain.cpp
    Source/PlateReverb.cpp
    Source/PresetStore.cpp
    Source/WavetableImport.cpp
    Source/Wavetable.cpp
    Source/WavetableShapes.cpp
    Source/LookAndFeel.cpp
    Source/Presets.cpp
    Source/Widgets/VaporWidgets.cpp
    Source/Widgets/Meters.cpp
    Source/Widgets/EqCurve.cpp
    Source/Widgets/WavetableDisplay.cpp
    Source/Widgets/WavetableShapePicker.cpp
    Source/Widgets/OscModMatrix.cpp
    Source/Pages/OscPage.cpp
    Source/Pages/FilterEnvPage.cpp
    Source/Pages/ModPage.cpp
    Source/Pages/ArpPage.cpp
    Source/Pages/FxPage.cpp
    Source/Pages/MasterPage.cpp
)

target_compile_definitions(VaporKey PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(VaporKey PRIVATE
    VaporKeyData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

if(VAPORKEY_LTO)
    target_link_libraries(VaporKey PRIVATE juce::juce_recommended_lto_flags)
endif()

# Stricter warnings on VaporKey's own sources only. juce_recommended_warning_flags
# above provides the JUCE-blessed baseline; layering extras at the target scope
# keeps them off JUCE's own translation units. -Wconversion will surface a fair
# number of float<->double promotions in DSP code; if the noise becomes
# unacceptable, drop it and rely on -Wshadow alone.
if(NOT MSVC)
    target_compile_options(VaporKey PRIVATE -Wshadow -Wconversion)
endif()

# Preset linter: validates Source/Presets.json against the parameter registry
# exposed by Parameters.h. Run as:
#   cmake --build build --target VaporKeyPresetLint
#   <path/to>/VaporKeyPresetLint Source/Presets.json
#
# CI invokes this before pluginval so a stale parameter id or out-of-range
# preset value fails fast.
juce_add_console_app(VaporKeyPresetLint
    PRODUCT_NAME "VaporKeyPresetLint"
)

juce_generate_juce_header(VaporKeyPresetLint)

target_sources(VaporKeyPresetLint PRIVATE
    Source/PresetLint.cpp
    Source/Parameters.cpp
    Source/Wavetable.cpp
    Source/WavetableShapes.cpp
)

target_compile_definitions(VaporKeyPresetLint PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(VaporKeyPresetLint PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Validation suite: unit + audio-regression tests for the synth engine,
# arpeggiator, FX chain, wavetable mip-mapping, .wav import, presets, and
# end-to-end audio. Run as:
#   cmake --build build --target VaporKeyTests
#   <path/to>/VaporKeyTests              # all suites
#   <path/to>/VaporKeyTests <substring>  # only tests whose name matches
#
# CI runs this on Linux after the preset lint and before pluginval; failures
# fail the build.
juce_add_console_app(VaporKeyTests
    PRODUCT_NAME "VaporKeyTests"
)

juce_generate_juce_header(VaporKeyTests)

target_sources(VaporKeyTests PRIVATE
    Source/Tests/TestRunner.cpp
    Source/Tests/TestSupport.cpp
    Source/Tests/ParametersTests.cpp
    Source/Tests/WavetableTests.cpp
    Source/Tests/WavetableImportTests.cpp
    Source/Tests/SynthEngineTests.cpp
    Source/Tests/ArpeggiatorTests.cpp
    Source/Tests/FxChainTests.cpp
    Source/Tests/PresetTests.cpp
    Source/Tests/AudioRegressionTests.cpp

    # The same source files as the plugin minus the GUI: the test runner
    # constructs a minimal AudioProcessor (TestProcessor) instead of the
    # real plugin so we don't pull in the editor / pages / widgets.
    Source/Parameters.cpp
    Source/SynthEngine.cpp
    Source/SynthVoice.cpp
    Source/Arpeggiator.cpp
    Source/FxChain.cpp
    Source/PlateReverb.cpp
    Source/PresetStore.cpp
    Source/Presets.cpp
    Source/Wavetable.cpp
    Source/WavetableShapes.cpp
    Source/WavetableImport.cpp
)

target_compile_definitions(VaporKeyTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(VaporKeyTests PRIVATE
    VaporKeyData
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
