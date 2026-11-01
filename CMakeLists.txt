cmake_minimum_required(VERSION 3.22)
project(NDSVeil VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT DEFINED JUCE_DIR)
    set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../tools/JUCE" CACHE PATH "Path to JUCE")
endif()
add_subdirectory(${JUCE_DIR} JUCE)

set(PLUGIN_FORMATS VST3 Standalone)
if (APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

juce_add_plugin(NDSVeil
    VERSION 0.1.0
    COMPANY_NAME "NDS"
    COMPANY_WEBSITE ""
    PLUGIN_MANUFACTURER_CODE NDSa
    PLUGIN_CODE NDVe
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "NDS Veil"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
)

juce_generate_juce_header(NDSVeil)

target_sources(NDSVeil PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/VeilDSP.h
)

target_compile_definitions(NDSVeil PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(NDSVeil PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
