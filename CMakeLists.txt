cmake_minimum_required(VERSION 3.22)
project(Roomove VERSION 0.1.0)

# =======================================================
# THE "Y" SPLIT: Hardware vs. Host
# =======================================================

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    # ... standard/library setup ...
    set(AAX_SDK_PATH "C:/SDKs/AAX-SDK")
    target_include_directories(Roomove_DSP PRIVATE 
        ${AAX_SDK_PATH}/Interfaces
        ${AAX_SDK_PATH}/Interfaces/ACF # Often required for DSP builds
    )
    # ---------------------------------------------------
    # 1. HARDWARE WORLD (TI DSP COMPILER)
    # ---------------------------------------------------
    message(STATUS "--- Configuring AAX DSP Algorithm (TI Compiler) ---")
    
    # Force legacy C++ so the TI DSP compiler doesn't panic
    set(CMAKE_CXX_STANDARD 98) 
    
    # Define the raw DSP algorithm here. 
    # NOTE: You CANNOT use PluginProcessor.cpp or JUCE here!
# Define the raw DSP algorithm here as a STATIC library
    # to satisfy the bare-metal TI compiler environment.
    add_library(Roomove_DSP STATIC
        dsp/RoomoveDSP.cpp
    )
    
    # The DSP compiler still needs to know where the AAX SDK headers are
    set(AAX_SDK_PATH "C:/SDKs/AAX-SDK")
    target_include_directories(Roomove_DSP PRIVATE ${AAX_SDK_PATH}/Interfaces)

else()
    # ---------------------------------------------------
    # 2. HOST WORLD (NATIVE MSVC COMPILER)
    # ---------------------------------------------------
    message(STATUS "--- Configuring Native Plugin (MSVC + JUCE) ---")
    
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

    # --- JUCE TRIPLE-CHECK LOGIC ---
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
        FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 7.0.9)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(AAX_SDK_PATH "C:/SDKs/AAX-SDK") 
    juce_set_aax_sdk_path("${AAX_SDK_PATH}")

    # I changed the target name to Roomove_AAX to match your manual.yaml build steps!
juce_add_plugin(Roomove_AAX
        # ... your other settings ...
        COPY_PLUGIN_AFTER_BUILD TRUE   # <--- CHANGE TO TRUE
        PLUGIN_MANUFACTURER_CODE Cftp
        PLUGIN_CODE             Rmov
        FORMATS                 AAX
        AAX_IDENTIFIER          "com.chrisfromthepast.Roomove" # Add this for better ID tracking
        PRODUCT_NAME            "Roomove"
    )

    juce_generate_juce_header(Roomove_AAX)

    target_sources(Roomove_AAX
        PRIVATE
            app/PluginEditor.cpp
            app/PluginEditor.h
            app/PluginProcessor.cpp
            app/PluginProcessor.h
    )

    target_compile_definitions(Roomove_AAX
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
    )

    target_link_libraries(Roomove_AAX
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )

    if(WIN32)
        add_custom_target(Roomove_Verify_And_Inject
            COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/cmake/VerifyAndInject.ps1" -BuildNativeDir "build_native" -BuildDspDir "build_dsp" -DspEnabled "$ENV{DSP_ENABLED}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Verify AAX bundle and inject DSP/Page Table resources"
            VERBATIM
        )
        add_dependencies(Roomove_Verify_And_Inject Roomove_AAX_AAX)
    endif()
endif()
