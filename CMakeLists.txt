cmake_minimum_required(VERSION 3.22)

project(Roomove VERSION 0.1.0)

# --- JUCE TRIPLE-CHECK LOGIC ---

# 1. CHECK FOR "BAKED" INSTALL (Fastest - seconds)
# 1. CHECK FOR "BAKED" INSTALL (Fastest - seconds)
find_package(JUCE CONFIG QUIET PATHS "C:/JUCE_Installed/lib/cmake/JUCE-8.0.12")

if(JUCE_FOUND)
    message(STATUS "SUCCESS: Using Pre-Built JUCE at ${JUCE_DIR}")
    set(JUCE_BUILD_HELPER_TOOLS OFF CACHE BOOL "" FORCE)

# 2. CHECK FOR LOCAL SOURCE (Medium - minutes)
elseif(EXISTS "C:/JUCE/CMakeLists.txt")
    message(STATUS "NOTICE: Pre-Built JUCE not found. Building from local source at C:/JUCE...")
    add_subdirectory("C:/JUCE" JUCE)

# 3. CLOUD FALLBACK (Slowest - "The Glacier")
else()
    message(STATUS "WARNING: No local JUCE found. Downloading from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        7.0.9 # Or your preferred version
    )
    FetchContent_MakeAvailable(JUCE)
endif()
# Point to the AAX SDK
# Hardcoded paths for local S6L AAX DSP build
set(AAX_SDK_PATH "C:/SDKs/AAX-SDK") 
set(ENV{TI_CGT_DIR} "C:/TI/c6000_7.4.24")

juce_set_aax_sdk_path("${AAX_SDK_PATH}")

juce_add_plugin(Roomove
    COMPANY_NAME            "chrisfromthepast"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Cftp
    PLUGIN_CODE             Rmov
    FORMATS                 AAX
    PRODUCT_NAME            "Roomove"
)

juce_generate_juce_header(Roomove)

target_sources(Roomove
    PRIVATE
        app/PluginProcessor.cpp
        app/PluginProcessor.h
)

target_compile_definitions(Roomove
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Roomove
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
