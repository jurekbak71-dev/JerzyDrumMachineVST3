cmake_minimum_required(VERSION 3.24)
project(JerzyComposer VERSION 0.4.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT DEFINED JUCE_DIR OR NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Set JUCE_DIR to checked-out JUCE (v8 recommended)")
endif()
add_subdirectory("${JUCE_DIR}" JUCE)
juce_add_plugin(JerzyComposer
    COMPANY_NAME "Jerzy Audio"
    BUNDLE_ID "pl.jerzy.composer"
    PLUGIN_MANUFACTURER_CODE Jrzy
    PLUGIN_CODE JCmp
    FORMATS VST3
    PRODUCT_NAME "Jerzy Composer"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(JerzyComposer PRIVATE PluginProcessor.cpp PluginProcessor.h PluginEditor.cpp PluginEditor.h ComposerEngine.h)
juce_generate_juce_header(JerzyComposer)
target_compile_definitions(JerzyComposer PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(JerzyComposer PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
include(CTest)
if(BUILD_TESTING)
  add_executable(ComposerCoreTests tests/CoreTests.cpp)
  target_include_directories(ComposerCoreTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME ComposerCore COMMAND ComposerCoreTests)
endif()
