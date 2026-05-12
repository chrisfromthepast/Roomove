cmake_minimum_required(VERSION 3.22)
project(Roomove VERSION 0.1.0)
include(CTest)

# =======================================================
# THE "Y" SPLIT: Hardware vs. Host
# =======================================================

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    # ---------------------------------------------------
    # 1. HARDWARE WORLD (TI DSP COMPILER)
    # ---------------------------------------------------
    message(STATUS "--- Configuring AAX DSP Algorithm (TI Compiler) ---")

    # Force legacy C++ so the TI DSP compiler doesn't panic
    set(CMAKE_CXX_STANDARD 98)

    add_library(Roomove_DSP STATIC
        dsp/RoomoveDSP.cpp
    )

    # The DSP compiler still needs to know where the AAX SDK headers are
    set(AAX_SDK_PATH "C:/SDKs/AAX-SDK")
    target_include_directories(Roomove_DSP PRIVATE
        ${AAX_SDK_PATH}/Interfaces
        ${AAX_SDK_PATH}/Interfaces/ACF
        "${TI_CGT_DIR}/include"  # <--- THIS IS THE FIX
    )

else()
    # ---------------------------------------------------
    # 2. HOST WORLD (NATIVE COMPILER + JUCE)
    # ---------------------------------------------------
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

    if(APPLE)
        # -----------------------------------------------
        # macOS – Standalone build (arm64 / Apple Silicon)
        # -----------------------------------------------
        message(STATUS "--- Configuring macOS Standalone Plugin (Clang + JUCE) ---")

        # JUCE discovery: honour an explicit -DJUCE_DIR=<path> first,
        # then try the default installer location, then fall back to FetchContent.
        find_package(JUCE CONFIG QUIET
            PATHS
                "${JUCE_DIR}"
                "/Applications/JUCE/lib/cmake/JUCE-8.0.12"
                "/opt/homebrew/lib/cmake/JUCE-8.0.12"
        )
        if(JUCE_FOUND)
            message(STATUS "SUCCESS: Using Pre-Built JUCE at ${JUCE_DIR}")
            set(JUCE_BUILD_HELPER_TOOLS OFF CACHE BOOL "" FORCE)
        elseif(EXISTS "/Applications/JUCE/CMakeLists.txt")
            message(STATUS "NOTICE: Building JUCE from /Applications/JUCE source...")
            add_subdirectory("/Applications/JUCE" JUCE)
        else()
            message(STATUS "WARNING: No local JUCE found. Downloading from GitHub...")
            include(FetchContent)
            FetchContent_Declare(JUCE
                GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                GIT_TAG        8.0.12
            )
            FetchContent_MakeAvailable(JUCE)
        endif()

        juce_add_plugin(Roomove
            COPY_PLUGIN_AFTER_BUILD       FALSE
            PLUGIN_MANUFACTURER_CODE      Cftp
            PLUGIN_CODE                   Rmov
            FORMATS                       Standalone
            PLUGIN_CATEGORIES             Dynamics Effect
            BUNDLE_ID                     "com.chrisfromthepast.Roomove"
            PRODUCT_NAME                  "Roomove"
            MICROPHONE_PERMISSION_ENABLED TRUE
            MICROPHONE_PERMISSION_TEXT    "Roomove needs microphone access to process live audio."
        )

        juce_generate_juce_header(Roomove)

        target_sources(Roomove
            PRIVATE
                app/PluginEditor.cpp
                app/PluginEditor.h
                app/PluginProcessor.cpp
                app/PluginProcessor.h
                dsp/RoomoveDSP.cpp
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

    else()
        # -----------------------------------------------
        # Windows – AAX build (MSVC + JUCE)
        # -----------------------------------------------
        message(STATUS "--- Configuring Native Plugin (MSVC + JUCE) ---")

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
            FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12)
            FetchContent_MakeAvailable(JUCE)
        endif()

        set(AAX_SDK_PATH "C:/SDKs/AAX-SDK")
        juce_set_aax_sdk_path("${AAX_SDK_PATH}")

        # Configure AAX DSP support if AAX_DSP_TYPE is provided
        if(DEFINED AAX_DSP_TYPE)
            message(STATUS "AAX DSP Type configured: ${AAX_DSP_TYPE}")
            set(AAX_DSP_ENABLED ON)
        else()
            set(AAX_DSP_ENABLED OFF)
        endif()

        juce_add_plugin(Roomove_AAX
            COPY_PLUGIN_AFTER_BUILD TRUE
            PLUGIN_MANUFACTURER_CODE Cftp
            PLUGIN_CODE              Rmov
            FORMATS                  AAX
            PLUGIN_CATEGORIES        Dynamics Effect
            AAX_IDENTIFIER           "com.chrisfromthepast.Roomove"
            PRODUCT_NAME             "Roomove"
        )

        juce_generate_juce_header(Roomove_AAX)

        target_sources(Roomove_AAX
            PRIVATE
                app/PluginEditor.cpp
                app/PluginEditor.h
                app/PluginProcessor.cpp
                app/PluginProcessor.h
                dsp/RoomoveDSP.cpp
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
    endif()

    option(ROOMOVE_ENABLE_VALIDATION_TESTS "Build Roomove validation tests" ON)
    if(ROOMOVE_ENABLE_VALIDATION_TESTS AND TARGET juce::juce_core)
        enable_testing()
        add_executable(Roomove_ValidationTests
            tests/RealtimeValidationTests.cpp
            dsp/RoomoveDSP.cpp
        )
        target_include_directories(Roomove_ValidationTests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/dsp
            ${CMAKE_CURRENT_SOURCE_DIR}/app
        )
        target_link_libraries(Roomove_ValidationTests PRIVATE
            juce::juce_core
        )
        target_compile_definitions(Roomove_ValidationTests PRIVATE
            ROOMOVE_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
        )
        add_test(NAME Roomove_ValidationTests COMMAND Roomove_ValidationTests)
    endif()

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_custom_target(roomove_realtime_memory_hygiene
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_realtime_memory_hygiene.py"
                --plugin-processor "${CMAKE_CURRENT_SOURCE_DIR}/app/PluginProcessor.cpp"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Running real-time memory hygiene scan"
        )
        add_test(
            NAME roomove_realtime_memory_hygiene
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_realtime_memory_hygiene.py"
                --plugin-processor "${CMAKE_CURRENT_SOURCE_DIR}/app/PluginProcessor.cpp"
        )
    endif()
endif()
