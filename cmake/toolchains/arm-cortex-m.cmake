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
# -ffreestanding: research.md §B4 frames -fno-builtin (which this implies) as
# generally not worth it, "unless writing a from-scratch runtime with
# non-standard libc semantics" — exactly this target's situation:
# examples/arm_cortex_m_minimal/ links -nostdlib with its own from-scratch
# startup.c/cortex_m.ld and no libc at all, so leaving this off would let the
# compiler assume hosted memcpy/memset/etc. semantics and potentially emit a
# call to a symbol -nostdlib linking will never provide. Same reasoning as
# xtensa-esp32.cmake's identical addition, applied here since this target is
# in the exact same situation (its own -nostdlib example, no shared libc).
set(CMAKE_CXX_FLAGS_INIT "-mcpu=${LWEH_CORTEX_M_CPU} -mthumb -ffreestanding")
set(CMAKE_C_FLAGS_INIT   "-mcpu=${LWEH_CORTEX_M_CPU} -mthumb -ffreestanding")
