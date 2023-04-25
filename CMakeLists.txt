# Adapted from Pamplejuce template
# https://github.com/sudara/pamplejuce/blob/580314ceb0/CMakeLists.txt

# Reference
# https://cmake.org/cmake/help/latest/manual/cmake-commands.7.html

cmake_minimum_required(VERSION 3.15)

# This is the name of your plugin
# This cannot have spaces (but PRODUCT_NAME can)
set(PROJECT_NAME "LightHost")

# Read project version from env variable, allowing it to be provided by CI/CD workflow
set(PROJECT_VERSION $ENV{PROJECT_VERSION})
if(NOT PROJECT_VERSION)
    set(PROJECT_VERSION 0.0.0.0)
endif()

# For simplicity, the name of the project is also the name of the target
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION})

# Allow to use deprecated MessageManager::runDispatchLoopUntil
add_compile_definitions(JUCE_MODAL_LOOPS_PERMITTED)

# Adds all the module sources so they appear correctly in the IDE
# Must be set before JUCE is added as a sub-dir (or any targets are made)
# https://github.com/juce-framework/JUCE/commit/6b1b4cf7f6b1008db44411f2c8887d71a3348889
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

# Create a /Modules directory in the IDE with the JUCE Module code
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Show all module sources in IDE projects" ON)

# JUCE is setup as a submodule in the /JUCE folder
# Locally, you'll need to run `git submodule update --init --recursive` once
# and `git submodule update --remote --merge` to keep it up to date
# On Github Actions, it's managed by actions/checkout
add_subdirectory(JUCE)

# Check the readme at `docs/CMake API.md` in the JUCE repo for full config
# https://github.com/juce-framework/JUCE/blob/69795dc8e5/docs/CMake%20API.md
juce_add_gui_app("${PROJECT_NAME}"
    COMPANY_NAME LightHost
    FORMATS Standalone # Valid formats: AAX Unity VST AU AUv3 Standalone
    ICON_BIG Resources/icon.png
    ICON_SMALL Resources/menu_icon.png
    PRODUCT_NAME "${PROJECT_NAME}")

# `juce_generate_juce_header` will create a JuceHeader.h for a given target, which will be generated
# into your build tree. This should be included with `#include <JuceHeader.h>`. The include path for
# this header will be automatically added to the target. The main function of the JuceHeader is to
# include all your JUCE module headers; if you're happy to include module headers directly, you
# probably don't need to call this.
juce_generate_juce_header("${PROJECT_NAME}")

# C++20, please
target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_20)

# Manually list all .h and .cpp files for the plugin
set(SourceFiles
    Source/HostStartup.cpp
    Source/IconMenu.cpp
    Source/IconMenu.hpp
    Source/PluginWindow.cpp
    Source/PluginWindow.h)
target_sources("${PROJECT_NAME}" PRIVATE ${SourceFiles})

# No, we don't want our source buried in extra nested folders
set_target_properties("${PROJECT_NAME}" PROPERTIES FOLDER "")

# The Xcode source tree should uhhh, still look like the source tree, yo
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Source PREFIX "" FILES ${SourceFiles})

# Setup our binary data as a target
juce_add_binary_data(Resources
    SOURCES
    Resources/icon.png
    Resources/menu_icon_white.png
    Resources/menu_icon.png)

# Required for Linux happiness:
# See https://forum.juce.com/t/loading-pytorch-model-using-binarydata/39997/2
set_target_properties(Resources PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_compile_definitions("${PROJECT_NAME}"
    PUBLIC
    # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
    JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
    JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=0
    JUCE_ALSA=0
    JUCE_ASIO=1
    JUCE_QUICKTIME=0
    JUCE_USE_CAMERA=0
    JUCE_USE_CDBURNER=0
    JUCE_USE_CDREADER=0
    JUCE_USE_FLAC=0
    JUCE_USE_OGGVORBIS=0
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_VST3=1
    
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=1)

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
