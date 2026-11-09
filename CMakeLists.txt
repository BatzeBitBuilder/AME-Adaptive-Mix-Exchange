cmake_minimum_required(VERSION 3.22)

project(AME VERSION 1.0.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: lokaler Checkout (-DAME_JUCE_PATH=...) oder automatischer Download
set(AME_JUCE_PATH "" CACHE PATH "Pfad zu einem lokalen JUCE-Checkout (optional)")

if(AME_JUCE_PATH)
    add_subdirectory(${AME_JUCE_PATH} ${CMAKE_BINARY_DIR}/juce)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.10
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(juce)
endif()

# Eingebettete Schriften (OFL) + Handbuch (docs/manual, Markdown)
juce_add_binary_data(AmeBinaryData SOURCES
    docs/manual/handbuch_de.md
    docs/manual/manual_en.md
    Resources/Fonts/AbrilFatface-Regular.ttf
    Resources/Fonts/BebasNeue-Regular.ttf
    Resources/Fonts/CourierPrime-Regular.ttf
    Resources/Fonts/CourierPrime-Bold.ttf)
set_target_properties(AmeBinaryData PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(AME
    COMPANY_NAME               "Batze the bit builder"
    COMPANY_COPYRIGHT          "(c) 2026 Batze the bit builder"
    PLUGIN_MANUFACTURER_CODE   Tbnn
    PLUGIN_CODE                Mtmx   # keep: plug-in ID of saved projects
    PRODUCT_NAME               "AME Adaptive Mix Exchange"
    BUNDLE_ID                  com.batzebitbuilder.ame
    FORMATS                    VST3
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES            Fx Mixing EQ Dynamics
    COPY_PLUGIN_AFTER_BUILD    FALSE)

target_sources(AME PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/hub/MixHub.cpp
    Source/dsp/ChannelStrip.cpp
    Source/dsp/SpectrumAnalyzer.cpp
    Source/hub/MaskingModel.cpp
    Source/engine/Profiles.cpp
    Source/engine/MixEngine.cpp
    Source/engine/RoleClassifier.cpp
    Source/ui/Fonts.cpp
    Source/ui/VintageLookAndFeel.cpp
    Source/ui/VuMeter.cpp
    Source/ui/Waterfall.cpp
    Source/ui/MaskingMatrix.cpp
    Source/ui/EqGraph.cpp
    Source/ui/StripPanel.cpp
    Source/ui/MixOverview.cpp
    Source/ui/HandbookView.cpp
    Source/i18n/Lang.cpp
        Source/Settings.cpp
        Source/dsp/StripParams.cpp
        Source/engine/StripPresets.cpp
        Source/engine/Recording.cpp)

target_include_directories(AME PRIVATE Source)

if(MSVC)
    add_compile_options(/utf-8)
    target_compile_options(AME PRIVATE /utf-8)
endif()

target_compile_definitions(AME PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AME
    PRIVATE
        AmeBinaryData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optionaler Kurztest ohne DAW
option(AME_BUILD_TESTS "Smoke-Test bauen" OFF)
if(AME_BUILD_TESTS)
    juce_add_console_app(AMESmokeTest PRODUCT_NAME "AMESmokeTest")
    target_sources(AMESmokeTest PRIVATE
        Tests/SmokeTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Parameters.cpp
        Source/hub/MixHub.cpp
        Source/dsp/ChannelStrip.cpp
        Source/dsp/SpectrumAnalyzer.cpp
        Source/hub/MaskingModel.cpp
        Source/engine/Profiles.cpp
        Source/engine/MixEngine.cpp
        Source/engine/RoleClassifier.cpp
        Source/ui/Fonts.cpp
        Source/ui/VintageLookAndFeel.cpp
        Source/ui/VuMeter.cpp
        Source/ui/Waterfall.cpp
        Source/ui/MaskingMatrix.cpp
        Source/ui/EqGraph.cpp
        Source/ui/StripPanel.cpp
        Source/ui/MixOverview.cpp
        Source/ui/HandbookView.cpp
        Source/i18n/Lang.cpp
        Source/Settings.cpp
        Source/dsp/StripParams.cpp
        Source/engine/StripPresets.cpp
        Source/engine/Recording.cpp)
    target_include_directories(AMESmokeTest PRIVATE Source)
    target_compile_definitions(AMESmokeTest PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="AME Adaptive Mix Exchange"
        JucePlugin_VersionString="${PROJECT_VERSION}")
    target_link_libraries(AMESmokeTest PRIVATE
        AmeBinaryData juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
