set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR c6000)

# 1. Catch the variable passed from the YAML runner
# If not passed, fallback to the default local path
if(NOT DEFINED TI_CGT_DIR)
    set(TI_CGT_DIR "C:/TI/c6000_7.4.24")
endif()

# 2. Point to the compilers
set(CMAKE_C_COMPILER "${TI_CGT_DIR}/bin/cl6x.exe")
set(CMAKE_CXX_COMPILER "${TI_CGT_DIR}/bin/cl6x.exe")

# 3. --- THE FIX ---
# Tell CMake we are only building static libraries (.a) so it doesn't 
# try (and fail) to link an executable during its initial test phase.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# The brute-force overrides just in case older CMake versions panic
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)

# Optional: Set default DSP flags (e.g., EABI, optimize for speed)
# set(CMAKE_CXX_FLAGS "-mv6740 --abi=eabi -O2" CACHE STRING "" FORCE)
