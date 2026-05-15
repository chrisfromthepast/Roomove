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

        # JUCE discovery: honour explicit JUCE_DIR first, then search common
        # local install locations (including wildcard user paths), then fall
        # back to FetchContent only if no local package/source is found.
        set(_juce_search_paths)
        if(DEFINED JUCE_DIR AND NOT "${JUCE_DIR}" STREQUAL "")
            list(APPEND _juce_search_paths "${JUCE_DIR}")
            if(IS_DIRECTORY "${JUCE_DIR}/lib/cmake")
                list(APPEND _juce_search_paths "${JUCE_DIR}/lib/cmake")
            endif()
            file(GLOB _juce_input_globbed_cmake_dirs
                "${JUCE_DIR}/lib/cmake/JUCE-*"
                "${JUCE_DIR}/JUCE-*/lib/cmake"
                "${JUCE_DIR}/JUCE-*/lib/cmake/JUCE-*"
            )
            list(APPEND _juce_search_paths ${_juce_input_globbed_cmake_dirs})
        endif()
        if(DEFINED ENV{JUCE_DIR} AND NOT "$ENV{JUCE_DIR}" STREQUAL "")
            list(APPEND _juce_search_paths "$ENV{JUCE_DIR}")
            if(IS_DIRECTORY "$ENV{JUCE_DIR}/lib/cmake")
                list(APPEND _juce_search_paths "$ENV{JUCE_DIR}/lib/cmake")
            endif()
            file(GLOB _juce_env_globbed_cmake_dirs
                "$ENV{JUCE_DIR}/lib/cmake/JUCE-*"
                "$ENV{JUCE_DIR}/JUCE-*/lib/cmake"
                "$ENV{JUCE_DIR}/JUCE-*/lib/cmake/JUCE-*"
            )
            list(APPEND _juce_search_paths ${_juce_env_globbed_cmake_dirs})
        endif()

        list(APPEND _juce_search_paths
            "$ENV{HOME}/juce-install/lib/cmake"
            "$ENV{HOME}/JUCE/lib/cmake"
            "/Applications/JUCE/lib/cmake"
            "/Applications/JUCE/lib/cmake/JUCE-8.0.12"
            "/opt/homebrew/lib/cmake"
            "/opt/homebrew/lib/cmake/JUCE-8.0.12"
            "/usr/local/lib/cmake"
        )

        list(REMOVE_DUPLICATES _juce_search_paths)

        set(_juce_found_search_path "")
        macro(_roomove_try_find_juce_in_paths)
            foreach(_juce_path IN LISTS _juce_search_paths)
                if(EXISTS "${_juce_path}")
                    find_package(JUCE CONFIG QUIET PATHS "${_juce_path}" NO_DEFAULT_PATH)
                    if(JUCE_FOUND)
                        set(_juce_found_search_path "${_juce_path}")
                        set(JUCE_DIR "${_juce_path}" CACHE PATH "Path to JUCE config package")
                        break()
                    endif()
                endif()
            endforeach()
        endmacro()

        _roomove_try_find_juce_in_paths()

        if(NOT JUCE_FOUND)
            file(GLOB _juce_globbed_cmake_dirs
                "/Users/*/juce-install/lib/cmake"
                "/Users/*/juce-install/lib/cmake/JUCE-*"
                "/Users/*/JUCE*/lib/cmake"
                "/Users/*/JUCE*/lib/cmake/JUCE-*"
                "/Applications/JUCE*/lib/cmake"
                "/Applications/JUCE*/lib/cmake/JUCE-*"
                "/opt/homebrew/lib/cmake/JUCE-*"
                "/usr/local/lib/cmake/JUCE-*"
            )
            list(APPEND _juce_search_paths ${_juce_globbed_cmake_dirs})
            list(REMOVE_DUPLICATES _juce_search_paths)
            _roomove_try_find_juce_in_paths()
        endif()

        if(NOT JUCE_FOUND)
            find_package(JUCE CONFIG QUIET)
            if(JUCE_FOUND AND DEFINED JUCE_DIR)
                set(_juce_found_search_path "${JUCE_DIR}")
            endif()
        endif()

        if(JUCE_FOUND)
            message(STATUS "SUCCESS: Using Pre-Built JUCE at ${_juce_found_search_path}")
            set(JUCE_BUILD_HELPER_TOOLS OFF CACHE BOOL "" FORCE)
        else()
            set(_juce_source_paths
                "$ENV{HOME}/juce-install"
                "$ENV{HOME}/JUCE"
                "/Applications/JUCE"
            )
            list(REMOVE_DUPLICATES _juce_source_paths)

            set(_juce_source_found OFF)
            macro(_roomove_try_add_juce_source)
                foreach(_juce_source_path IN LISTS _juce_source_paths)
                    if(EXISTS "${_juce_source_path}/CMakeLists.txt")
                        message(STATUS "NOTICE: Building JUCE from ${_juce_source_path} source...")
                        add_subdirectory("${_juce_source_path}" JUCE)
                        set(_juce_source_found ON)
                        break()
                    endif()
                endforeach()
            endmacro()

            _roomove_try_add_juce_source()

            if(NOT _juce_source_found)
                file(GLOB _juce_globbed_source_paths
                    "/Users/*/juce-install"
                    "/Users/*/JUCE*"
                    "/Applications/JUCE*"
                )
                list(APPEND _juce_source_paths ${_juce_globbed_source_paths})
                list(REMOVE_DUPLICATES _juce_source_paths)
                _roomove_try_add_juce_source()
            endif()

            if(NOT _juce_source_found)
                message(STATUS "WARNING: No local JUCE found. Downloading from GitHub...")
                include(FetchContent)
                FetchContent_Declare(JUCE
                    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                    GIT_TAG        8.0.12
                )
                FetchContent_MakeAvailable(JUCE)
            endif()
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
        set_tests_properties(Roomove_ValidationTests PROPERTIES LABELS "realtime")
    endif()

    if(ROOMOVE_ENABLE_VALIDATION_TESTS)
        enable_testing()
        add_executable(Roomove_DspSeamTests
            tests/DspSeamTests.cpp
            dsp/RoomoveDSP.cpp
        )
        target_include_directories(Roomove_DspSeamTests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/dsp
        )
        add_test(NAME Roomove_DspSeamTests COMMAND Roomove_DspSeamTests)
        set_tests_properties(Roomove_DspSeamTests PROPERTIES LABELS "dsp")
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
        set_tests_properties(roomove_realtime_memory_hygiene PROPERTIES LABELS "hygiene")
    endif()

    # ----------------------------------------------------------
    # roomove_all_tests: convenience meta-target that builds all
    # available test executables without building the full plugin.
    # Use:  cmake --build <dir> --target roomove_all_tests
    # ----------------------------------------------------------
    if(ROOMOVE_ENABLE_VALIDATION_TESTS)
        add_custom_target(roomove_all_tests)
        add_dependencies(roomove_all_tests Roomove_DspSeamTests)
        if(TARGET Roomove_ValidationTests)
            add_dependencies(roomove_all_tests Roomove_ValidationTests)
        endif()
    endif()
endif()
