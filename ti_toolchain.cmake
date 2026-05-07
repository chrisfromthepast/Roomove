set(CMAKE_SYSTEM_NAME Generic)

# Path to the TI compiler we just set in the YAML
set(TI_CGT_DIR $ENV{TI_CGT_DIR})

set(CMAKE_C_COMPILER "${TI_CGT_DIR}/bin/cl6x.exe")
set(CMAKE_CXX_COMPILER "${TI_CGT_DIR}/bin/cl6x.exe")

# AAX DSP specific flags to keep the TI compiler happy
set(CMAKE_CXX_FLAGS "--abi=eabi -mv6600 --gcc" CACHE STRING "")
