# Bare freestanding toolchain file for AVR (avr-gcc). AVR uses avr-libc, a
# separate lineage from newlib/picolibc (research.md §B2) — not relevant to
# Lw-Eh itself since its headers never touch libc beyond <cstdint>/<cstddef>.
# Requires avr-gcc/avr-g++ already on PATH — never fetched here.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)

set(CMAKE_C_COMPILER   avr-gcc)
set(CMAKE_CXX_COMPILER avr-g++)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(LWEH_AVR_MCU "atmega328p" CACHE STRING "Target AVR device")
# -mmcu= also selects avr-gcc's correct default linker script (research.md §B2).
set(CMAKE_CXX_FLAGS_INIT "-mmcu=${LWEH_AVR_MCU}")
set(CMAKE_C_FLAGS_INIT   "-mmcu=${LWEH_AVR_MCU}")
