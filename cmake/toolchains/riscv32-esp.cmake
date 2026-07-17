# Bare freestanding toolchain file for RISC-V ESP32 variants (esp32c3/c6/h2/
# c2/c5/c61), via riscv32-esp-elf-gcc. See xtensa-esp32.cmake for the general
# notes on why this file exists and what it doesn't cover.
# Requires riscv32-esp-elf-gcc/-g++ already on PATH — never fetched here.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv32)

set(CMAKE_C_COMPILER   riscv32-esp-elf-gcc)
set(CMAKE_CXX_COMPILER riscv32-esp-elf-g++)
set(CMAKE_ASM_COMPILER riscv32-esp-elf-gcc)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# rv32imc: the C (compressed instructions) extension is the RISC-V analog to
# ARM Thumb and is essential for code density (research.md §B4).
#
# -ffreestanding: research.md §B4 frames -fno-builtin (which this implies) as
# generally not worth it, "unless writing a from-scratch runtime with
# non-standard libc semantics" — exactly this target's situation:
# examples/riscv32_esp_minimal/ links -nostdlib with its own from-scratch
# startup.S/riscv32.ld and no libc at all, so leaving this off would let the
# compiler assume hosted memcpy/memset/etc. semantics and potentially emit a
# call to a symbol -nostdlib linking will never provide. Same reasoning as
# xtensa-esp32.cmake's identical addition, applied here since this target is
# in the exact same situation (its own -nostdlib example, no shared libc).
set(CMAKE_CXX_FLAGS_INIT "-march=rv32imc -mabi=ilp32 -ffreestanding")
set(CMAKE_C_FLAGS_INIT   "-march=rv32imc -mabi=ilp32 -ffreestanding")
