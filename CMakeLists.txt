cmake_minimum_required(VERSION 3.22)
project(VaporKey VERSION 0.4.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    add_compile_options(/W3 /MP /fp:fast)
else()
    add_compile_options(-Wall -Wno-unused-parameter -ffast-math)
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
    Source/PresetStore.cpp
    Source/WavetableImport.cpp
    Source/Wavetable.cpp
    Source/LookAndFeel.cpp
    Source/Presets.cpp
    Source/Widgets/VaporWidgets.cpp
    Source/Widgets/Meters.cpp
    Source/Widgets/EqCurve.cpp
    Source/Widgets/WavetableDisplay.cpp
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
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
