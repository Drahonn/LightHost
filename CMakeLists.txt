# Adapted from Pamplejuce template
# https://github.com/sudara/pamplejuce/blob/580314ceb0/CMakeLists.txt

# Reference
# https://cmake.org/cmake/help/latest/manual/cmake-commands.7.html

cmake_minimum_required(VERSION 3.22)

# This is the name of your plugin
# This cannot have spaces (but PRODUCT_NAME can)
set(PROJECT_NAME "LightHost")

# Read project version from env variable, allowing it to be provided by CI/CD workflow
set(PROJECT_VERSION $ENV{PROJECT_VERSION})
if(NOT PROJECT_VERSION)
    set(PROJECT_VERSION 2.2.0)
endif()

# For simplicity, the name of the project is also the name of the target
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION})

# Adds all the module sources so they appear correctly in the IDE
# Must be set before JUCE is added as a sub-dir (or any targets are made)
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

# Create a /Modules directory in the IDE with the JUCE Module code
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Show all module sources in IDE projects" ON)

# JUCE setup: check if local JUCE directory has CMakeLists.txt, otherwise fetch JUCE 9.0.4
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# Check the readme at `docs/CMake API.md` in the JUCE repo for full config
juce_add_gui_app("${PROJECT_NAME}"
    COMPANY_NAME LightHost
    FORMATS Standalone # Valid formats: Standalone
    ICON_BIG Resources/icon.png
    ICON_SMALL Resources/menu_icon.png
    PRODUCT_NAME "${PROJECT_NAME}"
    PLIST_TO_MERGE
        "<plist><dict><key>NSMicrophoneUsageDescription</key><string>LightHost requires audio input access to process audio through your VST and AU plugins.</string></dict></plist>")

# `juce_generate_juce_header` will create a JuceHeader.h for a given target
juce_generate_juce_header("${PROJECT_NAME}")

# C++20 standard
target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_20)

# Manually list all .h and .cpp files for the application
set(SourceFiles
    Source/HostStartup.cpp
    Source/IconMenu.cpp
    Source/IconMenu.hpp
    Source/PluginWindow.cpp
    Source/PluginWindow.h)
target_sources("${PROJECT_NAME}" PRIVATE ${SourceFiles})

set_target_properties("${PROJECT_NAME}" PROPERTIES FOLDER "")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Source PREFIX "" FILES ${SourceFiles})

# Setup our binary data as a target
juce_add_binary_data(Resources
    SOURCES
    Resources/icon.png
    Resources/menu_icon_white.png
    Resources/menu_icon.png)

set_target_properties(Resources PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_compile_definitions("${PROJECT_NAME}"
    PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_WASAPI=$<IF:$<PLATFORM_ID:Windows>,1,0>
    JUCE_DIRECTSOUND=0
    JUCE_ALSA=$<IF:$<PLATFORM_ID:Linux>,1,0>
    JUCE_JACK=$<IF:$<PLATFORM_ID:Linux>,1,0>
    JUCE_ASIO=$<IF:$<PLATFORM_ID:Windows>,1,0>
    JUCE_USE_CAMERA=0
    JUCE_USE_FLAC=0
    JUCE_USE_OGGVORBIS=0
    JUCE_PLUGINHOST_AU=$<IF:$<PLATFORM_ID:Darwin>,1,0>
    JUCE_PLUGINHOST_VST3=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries("${PROJECT_NAME}"
    PRIVATE
    Resources
    juce::juce_audio_utils
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Color our warnings and errors
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   add_compile_options (-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
   add_compile_options (-fcolor-diagnostics)
endif ()
