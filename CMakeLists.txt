cmake_minimum_required(VERSION 3.22)
project(BoomBapDrums VERSION 0.1.0)

option(COPY_AFTER_BUILD "Copy the plugin to the system plugin folder after building" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

juce_add_plugin(BoomBapDrums
    COMPANY_NAME "BoomBapLabs"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE BbLb
    PLUGIN_CODE Bbdm
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Boom Bap Drums"
    COPY_PLUGIN_AFTER_BUILD ${COPY_AFTER_BUILD})

juce_generate_juce_header(BoomBapDrums)

target_sources(BoomBapDrums PRIVATE
    Source/Groove.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp)

target_compile_definitions(BoomBapDrums PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(BoomBapDrums
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
