# Bare freestanding toolchain file for ARM Cortex-M (arm-none-eabi-gcc).
# Requires arm-none-eabi-gcc/-g++ already on PATH — never fetched here.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(LWEH_CORTEX_M_CPU "cortex-m4" CACHE STRING
    "Target Cortex-M core, e.g. cortex-m0plus, cortex-m4, cortex-m33")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=${LWEH_CORTEX_M_CPU} -mthumb")
set(CMAKE_C_FLAGS_INIT   "-mcpu=${LWEH_CORTEX_M_CPU} -mthumb")
